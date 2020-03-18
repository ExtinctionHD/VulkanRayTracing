#include "Engine/Render/RenderSystem.hpp"
#include "Engine/Render/Vulkan/VulkanHelpers.hpp"
#include "Engine/Render/Vulkan/Shaders/ShaderCompiler.hpp"

#include "Utils/Helpers.hpp"
#include "Utils/Assert.hpp"

namespace SRenderSystem
{
    std::unique_ptr<RenderPass> CreateRenderPass(const VulkanContext &vulkanContext,
            bool hasUIRenderFunction)
    {
        AttachmentDescription attachmentDescription{
            AttachmentDescription::Usage::eColor,
            vulkanContext.swapchain->GetFormat(),
            vk::AttachmentLoadOp::eClear,
            vk::AttachmentStoreOp::eStore,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::ePresentSrcKHR
        };

        if (hasUIRenderFunction)
        {
            attachmentDescription.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
        }

        const RenderPassDescription description{
            vk::PipelineBindPoint::eGraphics, vk::SampleCountFlagBits::e1, { attachmentDescription }
        };

        std::unique_ptr<RenderPass> renderPass = RenderPass::Create(vulkanContext.device,
                description, RenderPassDependencies{});

        return renderPass;
    }

    std::unique_ptr<GraphicsPipeline> CreateGraphicsPipeline(const VulkanContext &vulkanContext,
            const RenderPass &renderPass, const std::vector<vk::DescriptorSetLayout> &descriptorSetLayouts)
    {
        ShaderCache &shaderCache = *vulkanContext.shaderCache;
        const std::vector<ShaderModule> shaderModules{
            shaderCache.CreateShaderModule(vk::ShaderStageFlagBits::eVertex, Filepath("~/Shaders/Rasterize.vert"), {}),
            shaderCache.CreateShaderModule(vk::ShaderStageFlagBits::eFragment, Filepath("~/Shaders/Rasterize.frag"), {})
        };

        const VertexDescription vertexDescription{
            { vk::Format::eR32G32B32Sfloat, vk::Format::eR32G32Sfloat },
            vk::VertexInputRate::eVertex
        };

        const std::vector<vk::PushConstantRange> pushConstantRanges{
            { vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::vec4) },
            { vk::ShaderStageFlagBits::eFragment, sizeof(glm::vec4), sizeof(glm::vec4) }
        };

        const GraphicsPipelineDescription description{
            vulkanContext.swapchain->GetExtent(), vk::PrimitiveTopology::eTriangleList,
            vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack, vk::FrontFace::eClockwise,
            vk::SampleCountFlagBits::e1, std::nullopt, shaderModules, { vertexDescription },
            { BlendMode::eDisabled }, descriptorSetLayouts, pushConstantRanges
        };

        return GraphicsPipeline::Create(vulkanContext.device, renderPass.Get(), description);
    }

    std::unique_ptr<RayTracingPipeline> CreateRayTracingPipeline(const VulkanContext &vulkanContext,
            const std::vector<vk::DescriptorSetLayout> &descriptorSetLayouts)
    {
        ShaderCache &shaderCache = *vulkanContext.shaderCache;
        const std::vector<ShaderModule> shaderModules{
            shaderCache.CreateShaderModule(
                    vk::ShaderStageFlagBits::eRaygenNV, Filepath("~/Shaders/RayTrace.rgen"), {}),
            shaderCache.CreateShaderModule(
                    vk::ShaderStageFlagBits::eMissNV, Filepath("~/Shaders/RayTrace.rmiss"), {}),
            shaderCache.CreateShaderModule(
                    vk::ShaderStageFlagBits::eClosestHitNV, Filepath("~/Shaders/RayTrace.rchit"), {})
        };

        const std::vector<RayTracingShaderGroup> shaderGroups{
            { vk::RayTracingShaderGroupTypeNV::eGeneral, 0, VK_SHADER_UNUSED_NV, VK_SHADER_UNUSED_NV },
            { vk::RayTracingShaderGroupTypeNV::eGeneral, 1, VK_SHADER_UNUSED_NV, VK_SHADER_UNUSED_NV },
            { vk::RayTracingShaderGroupTypeNV::eTrianglesHitGroup, VK_SHADER_UNUSED_NV, 2, VK_SHADER_UNUSED_NV },
        };

        const RayTracingPipelineDescription description{
            shaderModules, shaderGroups, descriptorSetLayouts, {}
        };

        return RayTracingPipeline::Create(vulkanContext.device, GetRef(vulkanContext.bufferManager), description);
    }

    BufferHandle CreateVertexBuffer(const VulkanContext &vulkanContext)
    {
        struct Vertex
        {
            glm::vec3 position;
            glm::vec2 texCoord;
        };

        const std::vector<Vertex> vertices{
            Vertex{ glm::vec3(-0.5f, -0.5f, 0.0f), glm::vec2(0.0f, 0.0f) },
            Vertex{ glm::vec3(0.5f, -0.5f, 0.0f), glm::vec2(1.0f, 0.0f) },
            Vertex{ glm::vec3(0.5f, 0.5f, 0.0f), glm::vec2(1.0f, 1.0f) },
            Vertex{ glm::vec3(-0.5f, 0.5f, 0.0f), glm::vec2(0.0f, 1.0f) }
        };

        const BufferDescription description{
            sizeof(Vertex) * vertices.size(),
            vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal
        };

        const SynchronizationScope blockedScope{
            vk::PipelineStageFlagBits::eVertexInput,
            vk::AccessFlagBits::eVertexAttributeRead
        };

        return vulkanContext.bufferManager->CreateBuffer(description,
                BufferCreateFlags::kNone, GetByteView(vertices), blockedScope);
    }

    BufferHandle CreateIndexBuffer(const VulkanContext &vulkanContext)
    {
        const std::vector<uint32_t> indices{
            0, 1, 2, 2, 3, 0
        };

        const BufferDescription indexBufferDescription{
            sizeof(uint32_t) * indices.size(),
            vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal
        };

        const SynchronizationScope blockedScope{
            vk::PipelineStageFlagBits::eVertexInput,
            vk::AccessFlagBits::eIndexRead
        };

        return vulkanContext.bufferManager->CreateBuffer(indexBufferDescription,
                BufferCreateFlags::kNone, GetByteView(indices), blockedScope);
    }

    RenderObject CreateRenderObject(const VulkanContext &vulkanContext)
    {
        const VertexFormat vertexFormat{
            vk::Format::eR32G32B32Sfloat, vk::Format::eR32G32Sfloat
        };

        const Mesh mesh{
            4, vertexFormat, CreateVertexBuffer(vulkanContext),
            6, vk::IndexType::eUint32, CreateIndexBuffer(vulkanContext)
        };

        const RenderObject renderObject{
            mesh, vulkanContext.accelerationStructureManager->GenerateBlas(mesh),
            { glm::mat4(1.0f), translate(glm::mat4(1.0f), glm::vec3(5.0f, 0.0f, 0.0f)) }
        };

        return renderObject;
    }

    Texture CreateTexture(const VulkanContext &vulkanContext)
    {
        const SamplerDescription samplerDescription{
            vk::Filter::eLinear, vk::Filter::eLinear,
            vk::SamplerMipmapMode::eLinear,
            vk::SamplerAddressMode::eRepeat,
            std::nullopt, 0.0f, 0.0f
        };

        return vulkanContext.textureCache->GetTexture(Filepath("~/Assets/Textures/logo-256x256-solid.png"),
                samplerDescription);
    }

    vk::AccelerationStructureNV GenerateTlas(const VulkanContext &vulkanContext,
            std::vector<RenderObject> renderObjects)
    {
        std::vector<GeometryInstance> instances;
        instances.reserve(renderObjects.size());

        for (const auto &renderObject : renderObjects)
        {
            for (const auto &transform : renderObject.transforms)
            {
                instances.push_back({ renderObject.blas, transform });
            }
        }

        return vulkanContext.accelerationStructureManager->GenerateTlas(instances);
    }
}

RenderSystem::RenderSystem(std::shared_ptr<VulkanContext> vulkanContext_, const RenderFunction &uiRenderFunction_)
    : vulkanContext(vulkanContext_)
    , uiRenderFunction(uiRenderFunction_)
{
    renderPass = SRenderSystem::CreateRenderPass(GetRef(vulkanContext), static_cast<bool>(uiRenderFunction));

    renderObject = SRenderSystem::CreateRenderObject(GetRef(vulkanContext));

    CreateRasterizationDescriptors();

    CreateRayTracingDescriptors();
    UpdateRayTracingDescriptors();

    ShaderCompiler::Initialize();

    graphicsPipeline = SRenderSystem::CreateGraphicsPipeline(GetRef(vulkanContext), GetRef(renderPass),
            { rasterizationDescriptors.layout });
    rayTracingPipeline = SRenderSystem::CreateRayTracingPipeline(GetRef(vulkanContext),
            { rayTracingDescriptors.layout });

    ShaderCompiler::Finalize();

    framebuffers = VulkanHelpers::CreateSwapchainFramebuffers(GetRef(vulkanContext->device),
            GetRef(vulkanContext->swapchain), GetRef(renderPass));

    frames.resize(framebuffers.size());
    for (auto &frame : frames)
    {
        frame.commandBuffer = vulkanContext->device->AllocateCommandBuffer(CommandsType::eOneTime);
        frame.presentCompleteSemaphore = VulkanHelpers::CreateSemaphore(GetRef(vulkanContext->device));
        frame.renderCompleteSemaphore = VulkanHelpers::CreateSemaphore(GetRef(vulkanContext->device));
        frame.fence = VulkanHelpers::CreateFence(GetRef(vulkanContext->device), vk::FenceCreateFlagBits::eSignaled);
    }

    switch (renderFlow)
    {
    case RenderFlow::eRasterization:
        presentCompleteWaitStages = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        mainRenderFunction = MakeFunction(&RenderSystem::Rasterize, this);
        break;
    case RenderFlow::eRayTracing:
        presentCompleteWaitStages = vk::PipelineStageFlagBits::eRayTracingShaderNV;
        mainRenderFunction = MakeFunction(&RenderSystem::RayTrace, this);
        break;
    }

    drawingSuspended = false;
}

RenderSystem::~RenderSystem()
{
    for (auto &frame : frames)
    {
        vulkanContext->device->Get().destroySemaphore(frame.presentCompleteSemaphore);
        vulkanContext->device->Get().destroySemaphore(frame.renderCompleteSemaphore);
        vulkanContext->device->Get().destroyFence(frame.fence);
    }

    for (auto &framebuffer : framebuffers)
    {
        vulkanContext->device->Get().destroyFramebuffer(framebuffer);
    }
}

void RenderSystem::Process(float)
{
    DrawFrame();
}

void RenderSystem::OnResize(const vk::Extent2D &extent)
{
    drawingSuspended = extent.width == 0 || extent.height == 0;

    if (!drawingSuspended)
    {
        for (auto &framebuffer : framebuffers)
        {
            vulkanContext->device->Get().destroyFramebuffer(framebuffer);
        }

        renderPass = SRenderSystem::CreateRenderPass(GetRef(vulkanContext), static_cast<bool>(uiRenderFunction));
        graphicsPipeline = SRenderSystem::CreateGraphicsPipeline(GetRef(vulkanContext),
                GetRef(renderPass), { rasterizationDescriptors.layout });
        framebuffers = VulkanHelpers::CreateSwapchainFramebuffers(GetRef(vulkanContext->device),
                GetRef(vulkanContext->swapchain), GetRef(renderPass));

        UpdateRayTracingDescriptors();
    }
}

void RenderSystem::DrawFrame()
{
    if (drawingSuspended) return;

    const vk::SwapchainKHR swapchain = vulkanContext->swapchain->Get();
    const vk::Device device = vulkanContext->device->Get();

    auto [graphicsQueue, presentQueue] = vulkanContext->device->GetQueues();
    auto [commandBuffer, presentCompleteSemaphore, renderingCompleteSemaphore, fence] = frames[frameIndex];

    auto [result, imageIndex] = vulkanContext->device->Get().acquireNextImageKHR(swapchain,
            std::numeric_limits<uint64_t>::max(), presentCompleteSemaphore,
            nullptr);

    if (result == vk::Result::eErrorOutOfDateKHR) return;
    Assert(result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR);

    VulkanHelpers::WaitForFences(GetRef(vulkanContext->device), { fence });

    result = device.resetFences(1, &fence);
    Assert(result == vk::Result::eSuccess);

    const vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    result = commandBuffer.begin(beginInfo);
    Assert(result == vk::Result::eSuccess);

    mainRenderFunction(commandBuffer, imageIndex);
    if (uiRenderFunction)
    {
        uiRenderFunction(commandBuffer, imageIndex);
    }

    result = commandBuffer.end();
    Assert(result == vk::Result::eSuccess);

    const vk::SubmitInfo submitInfo(1, &presentCompleteSemaphore, &presentCompleteWaitStages,
            1, &commandBuffer, 1, &renderingCompleteSemaphore);

    result = graphicsQueue.submit(1, &submitInfo, fence);
    Assert(result == vk::Result::eSuccess);

    const vk::PresentInfoKHR presentInfo(1, &renderingCompleteSemaphore, 1, &swapchain, &imageIndex, nullptr);

    result = presentQueue.presentKHR(presentInfo);
    Assert(result == vk::Result::eSuccess);

    frameIndex = (frameIndex + 1) % frames.size();
}

void RenderSystem::Rasterize(vk::CommandBuffer commandBuffer, uint32_t imageIndex) const
{
    const ImageLayoutTransition swapchainImageLayoutTransition{
        vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
        PipelineBarrier{
            SynchronizationScope{
                presentCompleteWaitStages,
                vk::AccessFlags()
            },
            SynchronizationScope{
                vk::PipelineStageFlagBits::eColorAttachmentOutput,
                vk::AccessFlagBits::eColorAttachmentWrite
            }
        }
    };

    VulkanHelpers::TransitImageLayout(commandBuffer, vulkanContext->swapchain->GetImages()[imageIndex],
            VulkanHelpers::kSubresourceRangeFlatColor, swapchainImageLayoutTransition);

    const vk::Extent2D &extent = vulkanContext->swapchain->GetExtent();

    const vk::Rect2D renderArea(vk::Offset2D(0, 0), extent);

    const vk::ClearValue clearValue(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f });

    const vk::RenderPassBeginInfo beginInfo(renderPass->Get(), framebuffers[imageIndex], renderArea, 1, &clearValue);

    commandBuffer.beginRenderPass(beginInfo, vk::SubpassContents::eInline);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline->Get());
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphicsPipeline->GetLayout(),
            0, 1, &rasterizationDescriptors.descriptorSet, 0, nullptr);

    const glm::vec4 vertexOffset(0.2f, 0.0f, 0.0f, 0.0f);
    const glm::vec4 colorMultiplier(0.4f, 0.1f, 0.8f, 1.0f);
    commandBuffer.pushConstants(graphicsPipeline->GetLayout(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::vec4),
            &vertexOffset);
    commandBuffer.pushConstants(graphicsPipeline->GetLayout(), vk::ShaderStageFlagBits::eFragment, sizeof(glm::vec4),
            sizeof(glm::vec4), &colorMultiplier);

    const Mesh mesh = renderObject.mesh;
    const vk::DeviceSize offset = 0;

    commandBuffer.bindVertexBuffers(0, 1, &mesh.vertexBuffer->buffer, &offset);
    commandBuffer.bindIndexBuffer(mesh.indexBuffer->buffer, 0, mesh.indexType);

    commandBuffer.drawIndexed(mesh.indexCount, 1, 0, 0, 0);

    commandBuffer.endRenderPass();
}

void RenderSystem::RayTrace(vk::CommandBuffer commandBuffer, uint32_t imageIndex) const
{
    const ImageLayoutTransition preRayTraceLayoutTransition{
        vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral,
        PipelineBarrier{
            SynchronizationScope{
                presentCompleteWaitStages,
                vk::AccessFlags()
            },
            SynchronizationScope{
                vk::PipelineStageFlagBits::eRayTracingShaderNV,
                vk::AccessFlagBits::eShaderWrite
            }
        }
    };

    VulkanHelpers::TransitImageLayout(commandBuffer, vulkanContext->swapchain->GetImages()[imageIndex],
            VulkanHelpers::kSubresourceRangeFlatColor, preRayTraceLayoutTransition);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eRayTracingNV, rayTracingPipeline->Get());

    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingNV, rayTracingPipeline->GetLayout(),
            0, 1, &rayTracingDescriptors.descriptorSets[imageIndex], 0, nullptr);

    const ShaderBindingTable &shaderBindingTable = rayTracingPipeline->GetShaderBindingTable();
    const auto &[buffer, raygenOffset, missOffset, hitOffset, stride] = shaderBindingTable;

    const vk::Extent2D &extent = vulkanContext->swapchain->GetExtent();

    commandBuffer.traceRaysNV(buffer->buffer, raygenOffset,
            buffer->buffer, missOffset, stride,
            buffer->buffer, hitOffset, stride,
            nullptr, 0, 0, extent.width, extent.height, 1);

    const ImageLayoutTransition postRayTraceLayoutTransition{
        vk::ImageLayout::eGeneral, vk::ImageLayout::eColorAttachmentOptimal,
        PipelineBarrier{
            SynchronizationScope{
                vk::PipelineStageFlagBits::eRayTracingShaderNV,
                vk::AccessFlagBits::eShaderWrite,
            },
            SynchronizationScope{
                vk::PipelineStageFlagBits::eColorAttachmentOutput,
                vk::AccessFlagBits::eColorAttachmentWrite
            },
        }
    };

    VulkanHelpers::TransitImageLayout(commandBuffer, vulkanContext->swapchain->GetImages()[imageIndex],
            VulkanHelpers::kSubresourceRangeFlatColor, postRayTraceLayoutTransition);
}

void RenderSystem::CreateRasterizationDescriptors()
{
    texture = SRenderSystem::CreateTexture(GetRef(vulkanContext));

    const DescriptorSetDescription description{
        { vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment },
    };

    auto &[layout, descriptorSet] = rasterizationDescriptors;

    layout = vulkanContext->descriptorPool->CreateDescriptorSetLayout(description);
    descriptorSet = vulkanContext->descriptorPool->AllocateDescriptorSet(layout);

    const vk::DescriptorImageInfo textureInfo{
        texture.sampler,
        texture.image->views.front(),
        vk::ImageLayout::eShaderReadOnlyOptimal
    };

    const DescriptorSetData descriptorSetData{
        { vk::DescriptorType::eCombinedImageSampler, textureInfo }
    };

    vulkanContext->descriptorPool->UpdateDescriptorSet(descriptorSet, descriptorSetData, 0);
}

void RenderSystem::CreateRayTracingDescriptors()
{
    tlas = SRenderSystem::GenerateTlas(GetRef(vulkanContext), { renderObject });

    const uint32_t imageCount = static_cast<uint32_t>(vulkanContext->swapchain->GetImageViews().size());

    const DescriptorSetDescription description{
        { vk::DescriptorType::eAccelerationStructureNV, vk::ShaderStageFlagBits::eRaygenNV },
        { vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eRaygenNV }
    };

    auto &[layout, descriptorSets] = rayTracingDescriptors;

    layout = vulkanContext->descriptorPool->CreateDescriptorSetLayout(description);
    descriptorSets = vulkanContext->descriptorPool->AllocateDescriptorSets(layout, imageCount);
}

void RenderSystem::UpdateRayTracingDescriptors() const
{
    const std::vector<vk::ImageView> &swapchainImageViews = vulkanContext->swapchain->GetImageViews();
    const uint32_t imageCount = static_cast<uint32_t>(swapchainImageViews.size());

    const std::vector<vk::DescriptorSet> &descriptorSets = rayTracingDescriptors.descriptorSets;

    const DescriptorInfo tlasInfo = vk::WriteDescriptorSetAccelerationStructureNV(1, &tlas);

    for (uint32_t i = 0; i < imageCount; ++i)
    {
        const DescriptorInfo imageInfo = vk::DescriptorImageInfo(vk::Sampler(),
                swapchainImageViews[i], vk::ImageLayout::eGeneral);

        const DescriptorSetData descriptorSetData{
            { vk::DescriptorType::eAccelerationStructureNV, tlasInfo },
            { vk::DescriptorType::eStorageImage, imageInfo }
        };

        vulkanContext->descriptorPool->UpdateDescriptorSet(descriptorSets[i], descriptorSetData, 0);
    }
}
