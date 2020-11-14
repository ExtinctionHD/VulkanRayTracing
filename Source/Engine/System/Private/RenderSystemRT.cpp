#include "Engine/System/RenderSystemRT.hpp"

#include "Engine/Render/Vulkan/VulkanContext.hpp"
#include "Engine/Render/Vulkan/Shaders/ShaderManager.hpp"
#include "Engine/Render/Vulkan/RayTracing/RayTracingPipeline.hpp"
#include "Engine/Scene/SceneRT.hpp"
#include "Engine/Config.hpp"
#include "Engine/Engine.hpp"
#include "Engine/System/RenderSystem.hpp"

namespace Details
{
    std::unique_ptr<RayTracingPipeline> CreateRayTracingPipeline(const SceneRT& scene,
            const std::vector<vk::DescriptorSetLayout>& layouts)
    {
        std::vector<ShaderModule> shaderModules{
            VulkanContext::shaderManager->CreateShaderModule(
                    vk::ShaderStageFlagBits::eRaygenKHR,
                    Filepath("~/Shaders/RayTracing/RayGen.rgen"),
                    std::make_tuple(scene.GetInfo().materialCount)),
            VulkanContext::shaderManager->CreateShaderModule(
                    vk::ShaderStageFlagBits::eMissKHR,
                    Filepath("~/Shaders/RayTracing/Miss.rmiss")),
            VulkanContext::shaderManager->CreateShaderModule(
                    vk::ShaderStageFlagBits::eClosestHitKHR,
                    Filepath("~/Shaders/RayTracing/ClosestHit.rchit"))
        };

        const std::vector<RayTracingPipeline::ShaderGroup> shaderGroups{
            RayTracingPipeline::ShaderGroup{
                vk::RayTracingShaderGroupTypeKHR::eGeneral,
                0, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR
            },
            RayTracingPipeline::ShaderGroup{
                vk::RayTracingShaderGroupTypeKHR::eGeneral,
                1, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR
            },
            RayTracingPipeline::ShaderGroup{
                vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
                VK_SHADER_UNUSED_KHR, 2, VK_SHADER_UNUSED_KHR
            },
        };

        const vk::PushConstantRange pushConstantRange(vk::ShaderStageFlagBits::eRaygenKHR, 0, sizeof(uint32_t));

        const RayTracingPipeline::Description description{
            shaderModules, shaderGroups, layouts, { pushConstantRange }
        };

        std::unique_ptr<RayTracingPipeline> pipeline = RayTracingPipeline::Create(description);

        for (const auto& shaderModule : shaderModules)
        {
            VulkanContext::shaderManager->DestroyShaderModule(shaderModule);
        }

        return pipeline;
    }

    void TransitSwapchainImageLayout(vk::CommandBuffer commandBuffer, uint32_t imageIndex)
    {
        const vk::Image swapchainImage = VulkanContext::swapchain->GetImages()[imageIndex];

        const ImageLayoutTransition layoutTransition{
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eGeneral,
            PipelineBarrier{
                SyncScope::kWaitForNothing,
                SyncScope::kRayTracingShaderWrite
            }
        };

        ImageHelpers::TransitImageLayout(commandBuffer, swapchainImage,
                ImageHelpers::kFlatColor, layoutTransition);
    }
}

RenderSystemRT::RenderSystemRT(SceneRT* scene_)
    : scene(scene_)
{
    SetupRenderTargets();
    SetupAccumulationTarget();
    SetupRayTracingPipeline();
    SetupDescriptorSets();

    Engine::AddEventHandler<vk::Extent2D>(EventType::eResize,
            MakeFunction(this, &RenderSystemRT::HandleResizeEvent));

    Engine::AddEventHandler<KeyInput>(EventType::eKeyInput,
            MakeFunction(this, &RenderSystemRT::HandleKeyInputEvent));

    Engine::AddEventHandler(EventType::eCameraUpdate,
            MakeFunction(this, &RenderSystemRT::ResetAccumulation));
}

RenderSystemRT::~RenderSystemRT()
{
    DescriptorHelpers::DestroyMultiDescriptorSet(renderTargets.descriptorSet);
    DescriptorHelpers::DestroyDescriptorSet(accumulationTarget.descriptorSet);

    VulkanContext::imageManager->DestroyImage(accumulationTarget.image);
}

void RenderSystemRT::Process(float) {}

void RenderSystemRT::Render(vk::CommandBuffer commandBuffer, uint32_t imageIndex)
{
    scene->UpdateCameraBuffer(commandBuffer);

    Details::TransitSwapchainImageLayout(commandBuffer, imageIndex);

    descriptorSets[0] = renderTargets.descriptorSet.values[imageIndex];

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, rayTracingPipeline->Get());

    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR,
            rayTracingPipeline->GetLayout(), 0, descriptorSets, {});

    commandBuffer.pushConstants<uint32_t>(rayTracingPipeline->GetLayout(),
            vk::ShaderStageFlagBits::eRaygenKHR, 0, { accumulationTarget.accumulationCount++ });

    const ShaderBindingTable& sbt = rayTracingPipeline->GetShaderBindingTable();

    const vk::StridedBufferRegionKHR raygenSBT(sbt.buffer, sbt.raygenOffset, sbt.stride, sbt.stride);
    const vk::StridedBufferRegionKHR missSBT(sbt.buffer, sbt.missOffset, sbt.stride, sbt.stride);
    const vk::StridedBufferRegionKHR hitSBT(sbt.buffer, sbt.hitOffset, sbt.stride, sbt.stride);

    const vk::Extent2D& extent = VulkanContext::swapchain->GetExtent();

    commandBuffer.traceRaysKHR(raygenSBT, missSBT, hitSBT,
            vk::StridedBufferRegionKHR(), extent.width, extent.height, 1);
}

void RenderSystemRT::SetupRenderTargets()
{
    const std::vector<vk::ImageView>& swapchainImageViews = VulkanContext::swapchain->GetImageViews();

    const DescriptorDescription descriptorDescription{
        1, vk::DescriptorType::eStorageImage,
        vk::ShaderStageFlagBits::eRaygenKHR,
        vk::DescriptorBindingFlags()
    };

    std::vector<DescriptorSetData> multiDescriptorData;
    multiDescriptorData.reserve(swapchainImageViews.size());

    for (const auto& swapchainImageView : swapchainImageViews)
    {
        multiDescriptorData.push_back({ DescriptorHelpers::GetData(swapchainImageView) });
    }

    renderTargets.descriptorSet = DescriptorHelpers::CreateMultiDescriptorSet(
            { descriptorDescription }, multiDescriptorData);
}

void RenderSystemRT::SetupAccumulationTarget()
{
    const vk::Extent2D& swapchainExtent = VulkanContext::swapchain->GetExtent();

    const ImageDescription imageDescription{
        ImageType::e2D,
        vk::Format::eR8G8B8A8Unorm,
        VulkanHelpers::GetExtent3D(swapchainExtent),
        1, 1, vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eStorage,
        vk::ImageLayout::eUndefined,
        vk::MemoryPropertyFlagBits::eDeviceLocal
    };

    ImageManager& imageManager = *VulkanContext::imageManager;
    accumulationTarget.image = imageManager.CreateImage(imageDescription, ImageCreateFlags::kNone);
    accumulationTarget.view = imageManager.CreateView(accumulationTarget.image,
            vk::ImageViewType::e2D, ImageHelpers::kFlatColor);

    const DescriptorDescription descriptorDescription{
        1, vk::DescriptorType::eStorageImage,
        vk::ShaderStageFlagBits::eRaygenKHR,
        vk::DescriptorBindingFlags()
    };

    const DescriptorData descriptorData = DescriptorHelpers::GetData(accumulationTarget.view);

    accumulationTarget.descriptorSet = DescriptorHelpers::CreateDescriptorSet(
            { descriptorDescription }, { descriptorData });

    VulkanContext::device->ExecuteOneTimeCommands([this](vk::CommandBuffer commandBuffer)
        {
            const ImageLayoutTransition layoutTransition{
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eGeneral,
                PipelineBarrier{
                    SyncScope::kWaitForNothing,
                    SyncScope::kRayTracingShaderRead | SyncScope::kRayTracingShaderWrite
                }
            };

            ImageHelpers::TransitImageLayout(commandBuffer, accumulationTarget.image,
                    ImageHelpers::kFlatColor, layoutTransition);
        });
}

void RenderSystemRT::SetupRayTracingPipeline()
{
    std::vector<vk::DescriptorSetLayout> layouts{
        renderTargets.descriptorSet.layout,
        accumulationTarget.descriptorSet.layout,
    };

    const std::vector<vk::DescriptorSetLayout> sceneLayouts = scene->GetDescriptorSetLayouts();

    layouts.insert(layouts.end(), sceneLayouts.begin(), sceneLayouts.end());

    rayTracingPipeline = Details::CreateRayTracingPipeline(*scene, layouts);
}

void RenderSystemRT::SetupDescriptorSets()
{
    descriptorSets = std::vector<vk::DescriptorSet>{
        renderTargets.descriptorSet.values.front(),
        accumulationTarget.descriptorSet.value
    };

    const std::vector<vk::DescriptorSet> sceneDescriptorSets = scene->GetDescriptorSets();

    descriptorSets.insert(descriptorSets.end(),
            sceneDescriptorSets.begin(), sceneDescriptorSets.end());
}

void RenderSystemRT::HandleResizeEvent(const vk::Extent2D& extent)
{
    if (extent.width != 0 && extent.height != 0)
    {
        ResetAccumulation();

        DescriptorHelpers::DestroyMultiDescriptorSet(renderTargets.descriptorSet);
        DescriptorHelpers::DestroyDescriptorSet(accumulationTarget.descriptorSet);

        VulkanContext::imageManager->DestroyImage(accumulationTarget.image);

        SetupRenderTargets();
        SetupAccumulationTarget();
        SetupDescriptorSets();
    }
}

void RenderSystemRT::HandleKeyInputEvent(const KeyInput& keyInput)
{
    if (keyInput.action == KeyAction::ePress)
    {
        switch (keyInput.key)
        {
        case Key::eR:
            ReloadShaders();
            break;
        default:
            break;
        }
    }
}

void RenderSystemRT::ReloadShaders()
{
    VulkanContext::device->WaitIdle();

    SetupRayTracingPipeline();

    ResetAccumulation();
}

void RenderSystemRT::ResetAccumulation()
{
    accumulationTarget.accumulationCount = 0;
}
