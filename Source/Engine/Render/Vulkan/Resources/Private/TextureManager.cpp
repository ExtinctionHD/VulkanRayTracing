#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Engine/Render/Vulkan/Resources/TextureManager.hpp"

#include "Engine/Render/Vulkan/VulkanContext.hpp"
#include "Engine/Render/Vulkan/ComputePipeline.hpp"

#include "Shaders/Compute/Compute.h"

namespace STextureManager
{
    struct Pixels
    {
        uint8_t *data;
        vk::Extent2D extent;
        bool isHDR;
    };

    constexpr uint32_t kCubeFaceCount = 6;

    const Filepath kPanoramaToCubeShaderPath("~/Shaders/Compute/PanoramaToCube.comp");

    uint8_t FloatToUnorm(float value)
    {
        const float min = static_cast<float>(std::numeric_limits<uint8_t>::min());
        const float max = static_cast<float>(std::numeric_limits<uint8_t>::max());
        return static_cast<uint8_t>(std::clamp(max * value, min, max));
    }

    Pixels LoadTexture(const Filepath &filepath)
    {
        uint8_t *data;
        int32_t width, height;

        const bool isHDR = stbi_is_hdr(filepath.GetAbsolute().c_str());
        if (isHDR)
        {
            float *hdrData = stbi_loadf(filepath.GetAbsolute().c_str(), &width, &height, nullptr, STBI_rgb_alpha);
            data = reinterpret_cast<uint8_t*>(hdrData);
        }
        else
        {
            data = stbi_load(filepath.GetAbsolute().c_str(), &width, &height, nullptr, STBI_rgb_alpha);
        }
        Assert(data != nullptr);

        const vk::Extent2D extent(
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height));

        return Pixels{ data, extent, isHDR };
    }

    uint32_t CalculateMipLevelCount(const vk::Extent2D &extent)
    {
        const float maxSize = static_cast<float>(std::max(extent.width, extent.height));
        return 1 + static_cast<uint32_t>(std::floorf(std::log2f(maxSize)));
    }

    void UpdateImage(vk::CommandBuffer commandBuffer, vk::Image image,
            const ImageDescription &description, const uint8_t *data)
    {
        const vk::ImageSubresourceRange fullImage(vk::ImageAspectFlagBits::eColor,
                0, description.mipLevelCount, 0, description.layerCount);

        const vk::ImageSubresourceLayers baseMipLevel = ImageHelpers::GetSubresourceLayers(fullImage, 0);

        const size_t size = static_cast<size_t>(ImageHelpers::CalculateBaseMipLevelSize(description));

        const ImageUpdate imageUpdate{
            baseMipLevel,
            { 0, 0, 0 },
            description.extent,
            ByteView{ data, size }
        };

        const ImageLayoutTransition layoutTransition{
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eTransferDstOptimal,
            PipelineBarrier{
                SyncScope::kWaitForNothing,
                SyncScope::kTransferWrite
            }
        };

        ImageHelpers::TransitImageLayout(commandBuffer, image, fullImage, layoutTransition);

        VulkanContext::imageManager->UpdateImage(commandBuffer, image, { imageUpdate });
    }

    void TransitImageLayoutAfterMipmapsGenerating(vk::CommandBuffer commandBuffer,
            vk::Image image, const vk::ImageSubresourceRange &subresourceRange)
    {
        const vk::ImageSubresourceRange lastMipLevel(vk::ImageAspectFlagBits::eColor,
                subresourceRange.levelCount - 1, 1,
                subresourceRange.baseArrayLayer, subresourceRange.layerCount);

        const vk::ImageSubresourceRange exceptLastMipLevel(vk::ImageAspectFlagBits::eColor,
                subresourceRange.baseMipLevel, subresourceRange.levelCount - 1,
                subresourceRange.baseArrayLayer, subresourceRange.layerCount);

        const ImageLayoutTransition layoutTransition{
            vk::ImageLayout::eTransferSrcOptimal,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            PipelineBarrier{
                SyncScope::kTransferRead,
                SyncScope::kShaderRead
            }
        };

        ImageHelpers::TransitImageLayout(commandBuffer, image, exceptLastMipLevel, layoutTransition);

        const ImageLayoutTransition lastMipLevelLayoutTransition{
            vk::ImageLayout::eTransferDstOptimal,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            PipelineBarrier{
                SyncScope::kTransferWrite,
                SyncScope::kShaderRead
            }
        };

        ImageHelpers::TransitImageLayout(commandBuffer, image, lastMipLevel, lastMipLevelLayoutTransition);
    }

    MultiDescriptorSet CreateCubeMultiDescriptorSet(vk::Image cubeImage)
    {
        const DescriptorDescription description{
            vk::DescriptorType::eStorageImage, 1,
            vk::ShaderStageFlagBits::eCompute,
            vk::DescriptorBindingFlagBits()
        };

        std::vector<DescriptorSetData> multiDescriptorData;
        multiDescriptorData.reserve(STextureManager::kCubeFaceCount);

        for (uint32_t i = 0; i < STextureManager::kCubeFaceCount; ++i)
        {
            const vk::ImageSubresourceRange subresourceRange(
                    vk::ImageAspectFlagBits::eColor, 0, 1, i, 1);

            const vk::ImageView view = VulkanContext::imageManager->CreateView(cubeImage,
                    vk::ImageViewType::e2D, subresourceRange);

            multiDescriptorData.push_back({ DescriptorHelpers::GetData(view) });
        }

        return DescriptorHelpers::CreateMultiDescriptorSet({ description }, multiDescriptorData);
    }

    DescriptorSet CreatePanoramaDescriptorSet(const Texture &panoramaTexture)
    {
        const DescriptorDescription description{
            vk::DescriptorType::eCombinedImageSampler, 1,
            vk::ShaderStageFlagBits::eCompute,
            vk::DescriptorBindingFlagBits()
        };

        const DescriptorData descriptorData = DescriptorHelpers::GetData(
                panoramaTexture.sampler, panoramaTexture.view);

        return DescriptorHelpers::CreateDescriptorSet({ description }, { descriptorData });
    }

    void ConvertPanoramaToCube(const Texture &panoramaTexture,
            vk::Image cubeImage, const vk::Extent2D &cubeImageExtent)
    {
        const MultiDescriptorSet cubeMultiDescriptor = CreateCubeMultiDescriptorSet(cubeImage);
        const DescriptorSet panoramaDescriptor = CreatePanoramaDescriptorSet(panoramaTexture);

        const ShaderModule computeShaderModule = VulkanContext::shaderManager->CreateShaderModule(
                vk::ShaderStageFlagBits::eCompute, kPanoramaToCubeShaderPath);

        const vk::PushConstantRange pushConstantRange(vk::ShaderStageFlagBits::eCompute,
                0, sizeof(vk::Extent2D) + sizeof(uint32_t));

        const ComputePipeline::Description pipelineDescription{
            cubeImageExtent, computeShaderModule,
            { cubeMultiDescriptor.layout, panoramaDescriptor.layout },
            { pushConstantRange }
        };

        std::unique_ptr<ComputePipeline> computePipeline = ComputePipeline::Create(pipelineDescription);

        VulkanContext::device->ExecuteOneTimeCommands([&](vk::CommandBuffer commandBuffer)
            {
                const vk::ImageSubresourceRange subresourceRange(
                        vk::ImageAspectFlagBits::eColor, 0, 1, 0, kCubeFaceCount);

                const ImageLayoutTransition toGeneralLayoutTransition{
                    vk::ImageLayout::eUndefined,
                    vk::ImageLayout::eGeneral,
                    PipelineBarrier{
                        SyncScope::kWaitForNothing,
                        SyncScope::KComputeShaderWrite
                    }
                };

                ImageHelpers::TransitImageLayout(commandBuffer, cubeImage,
                        subresourceRange, toGeneralLayoutTransition);

                commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, computePipeline->Get());

                commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                        computePipeline->GetLayout(), 1, { panoramaDescriptor.value }, {});

                commandBuffer.pushConstants(computePipeline->GetLayout(),
                        vk::ShaderStageFlagBits::eCompute, 0, vk::ArrayProxy<const vk::Extent2D>{ cubeImageExtent });

                const uint32_t groupCountX = static_cast<uint32_t>(std::ceil(
                        cubeImageExtent.width / static_cast<float>(LOCAL_SIZE_X)));
                const uint32_t groupCountY = static_cast<uint32_t>(std::ceil(
                        cubeImageExtent.width / static_cast<float>(LOCAL_SIZE_Y)));

                for (uint32_t i = 0; i < kCubeFaceCount; ++i)
                {
                    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                            computePipeline->GetLayout(), 0, { cubeMultiDescriptor.values[i] }, {});

                    commandBuffer.pushConstants(computePipeline->GetLayout(),
                            vk::ShaderStageFlagBits::eCompute, sizeof(vk::Extent2D), sizeof(uint32_t), &i);

                    commandBuffer.dispatch(groupCountX, groupCountY, 1);
                }

                const ImageLayoutTransition GeneralToShaderOptimalLayoutTransition{
                    vk::ImageLayout::eGeneral,
                    vk::ImageLayout::eShaderReadOnlyOptimal,
                    PipelineBarrier{
                        SyncScope::KComputeShaderWrite,
                        SyncScope::kShaderRead
                    }
                };

                ImageHelpers::TransitImageLayout(commandBuffer, cubeImage,
                        subresourceRange, GeneralToShaderOptimalLayoutTransition);
            });

        DescriptorHelpers::DestroyMultiDescriptorSet(cubeMultiDescriptor);
        DescriptorHelpers::DestroyDescriptorSet(panoramaDescriptor);
    }

    Texture CreateTexture(const Pixels &pixels, vk::Sampler sampler)
    {
        const vk::Format format = pixels.isHDR ? vk::Format::eR32G32B32A32Sfloat : vk::Format::eR8G8B8A8Unorm;
        const vk::Extent3D extent(pixels.extent.width, pixels.extent.height, 1);
        const vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eSampled
                | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;

        const ImageDescription description{
            ImageType::e2D, format, extent,
            STextureManager::CalculateMipLevelCount(pixels.extent), 1,
            vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal, usage,
            vk::ImageLayout::eUndefined, vk::MemoryPropertyFlagBits::eDeviceLocal
        };

        const vk::Image image = VulkanContext::imageManager->CreateImage(description,
                ImageCreateFlagBits::eStagingBuffer);

        const vk::ImageSubresourceRange fullImage(vk::ImageAspectFlagBits::eColor,
                0, description.mipLevelCount, 0, description.layerCount);

        const vk::ImageView view = VulkanContext::imageManager->CreateView(image, vk::ImageViewType::e2D, fullImage);

        // TODO
        /*VulkanContext::device->ExecuteOneTimeCommands([&](vk::CommandBuffer commandBuffer)
            {
                STextureManager::UpdateImage(commandBuffer, image, description, pixels.data);

                if (description.mipLevelCount > 1)
                {
                    const vk::ImageSubresourceRange baseMipLevel(vk::ImageAspectFlagBits::eColor,
                            0, 1, 0, description.layerCount);

                    const ImageLayoutTransition dstToSrcLayoutTransition{
                        vk::ImageLayout::eTransferDstOptimal,
                        vk::ImageLayout::eTransferSrcOptimal,
                        PipelineBarrier{
                            SyncScope::kTransferWrite,
                            SyncScope::kTransferRead
                        }
                    };

                    ImageHelpers::TransitImageLayout(commandBuffer, image, baseMipLevel, dstToSrcLayoutTransition);

                    ImageHelpers::GenerateMipmaps(commandBuffer, image, description.extent, fullImage);

                    TransitImageLayoutAfterMipmapsGenerating(commandBuffer, image, fullImage);
                }
                else
                {
                    const ImageLayoutTransition layoutTransition{
                        vk::ImageLayout::eTransferDstOptimal,
                        vk::ImageLayout::eShaderReadOnlyOptimal,
                        PipelineBarrier{
                            SyncScope::kTransferWrite,
                            SyncScope::kShaderRead
                        }
                    };

                    ImageHelpers::TransitImageLayout(commandBuffer, image, fullImage, layoutTransition);
                }
            });*/

        return Texture{ image, view, sampler };
    }
}

TextureManager::TextureManager(const SamplerDescription &defaultSamplerDescription)
{
    defaultSampler = CreateSampler(defaultSamplerDescription);
}

TextureManager::~TextureManager()
{
    VulkanContext::device->Get().destroySampler(defaultSampler);
}

Texture TextureManager::CreateTexture(const Filepath &filepath) const
{
    const STextureManager::Pixels pixels = STextureManager::LoadTexture(filepath);

    const Texture texture = STextureManager::CreateTexture(pixels, defaultSampler);

    stbi_image_free(pixels.data);

    return texture;
}

Texture TextureManager::CreateColorTexture(const glm::vec3 &color) const
{
    std::array<uint8_t, glm::vec3::length()> data{
        STextureManager::FloatToUnorm(color.r),
        STextureManager::FloatToUnorm(color.g),
        STextureManager::FloatToUnorm(color.b)
    };

    const STextureManager::Pixels pixels{
        data.data(), vk::Extent2D(1, 1), false
    };

    return STextureManager::CreateTexture(pixels, defaultSampler);
}

Texture TextureManager::CreateCubeTexture(const Texture &panoramaTexture, const vk::Extent2D &extent) const
{
    const vk::Format format = VulkanContext::imageManager->GetImageDescription(panoramaTexture.image).format;
    const vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled
            | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;

    const ImageDescription imageDescription{
        ImageType::eCube, format,
        VulkanHelpers::GetExtent3D(extent),
        1, STextureManager::kCubeFaceCount,
        vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal, usage,
        vk::ImageLayout::eUndefined, vk::MemoryPropertyFlagBits::eDeviceLocal
    };

    const vk::Image cubeImage = VulkanContext::imageManager->CreateImage(imageDescription, ImageCreateFlags::kNone);

    STextureManager::ConvertPanoramaToCube(panoramaTexture, cubeImage, extent);

    const vk::ImageSubresourceRange subresourceRange(vk::ImageAspectFlagBits::eColor,
            0, 1, 0, STextureManager::kCubeFaceCount);

    const vk::ImageView cubeView = VulkanContext::imageManager->CreateView(cubeImage,
            vk::ImageViewType::eCube, subresourceRange);

    return Texture{ cubeImage, cubeView, defaultSampler };
}

vk::Sampler TextureManager::CreateSampler(const SamplerDescription &description) const
{
    const vk::SamplerCreateInfo createInfo({},
            description.magFilter, description.minFilter,
            description.mipmapMode, description.addressMode,
            description.addressMode, description.addressMode, 0.0f,
            description.maxAnisotropy.has_value(),
            description.maxAnisotropy.value_or(0.0f),
            false, vk::CompareOp(),
            description.minLod, description.maxLod,
            vk::BorderColor::eFloatOpaqueBlack, false);

    const auto &[result, sampler] = VulkanContext::device->Get().createSampler(createInfo);
    Assert(result == vk::Result::eSuccess);

    return sampler;
}

void TextureManager::DestroyTexture(const Texture &texture) const
{
    VulkanContext::imageManager->DestroyImage(texture.image);

    if (texture.sampler != defaultSampler)
    {
        VulkanContext::device->Get().destroySampler(texture.sampler);
    }
}
