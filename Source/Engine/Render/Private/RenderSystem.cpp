#include "Engine/Render/RenderSystem.hpp"
#include "Engine/Render/Vulkan/VulkanHelpers.hpp"
#include "Engine/Render/Vulkan/Shaders/ShaderCompiler.hpp"
#include "Engine/Render/Vulkan/Resources/Texture.hpp"

#include "Utils/Helpers.hpp"
#include "Utils/Assert.hpp"

namespace SRenderSystem
{
    std::unique_ptr<RenderPass> CreateRenderPass(const VulkanContext &context)
    {
        const AttachmentDescription attachmentDescription{
            AttachmentDescription::eUsage::kColor,
            context.swapchain->GetFormat(),
            vk::AttachmentLoadOp::eClear,
            vk::AttachmentStoreOp::eStore,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::ePresentSrcKHR
        };

        const RenderPassDescription description{
            vk::PipelineBindPoint::eGraphics, vk::SampleCountFlagBits::e1, { attachmentDescription }
        };

        std::unique_ptr<RenderPass> renderPass = RenderPass::Create(context.device, description, {});

        return renderPass;
    }

    std::vector<vk::Framebuffer> CreateFramebuffers(const VulkanContext &context, const RenderPass &renderPass)
    {
        const std::vector<vk::ImageView> &imageViews = context.swapchain->GetImageViews();
        const vk::Extent2D &extent = context.swapchain->GetExtent();

        std::vector<vk::Framebuffer> framebuffers;
        framebuffers.reserve(imageViews.size());

        for (const auto &imageView : imageViews)
        {
            const vk::FramebufferCreateInfo createInfo({}, renderPass.Get(),
                    1, &imageView, extent.width, extent.height, 1);

            const auto [result, framebuffer] = context.device->Get().createFramebuffer(createInfo);
            Assert(result == vk::Result::eSuccess);

            framebuffers.push_back(framebuffer);
        }

        return framebuffers;
    }

    std::unique_ptr<GraphicsPipeline> CreateGraphicsPipeline(const VulkanContext &context, const RenderPass &renderPass)
    {
        ShaderCompiler::Initialize();

        const std::vector<ShaderModule> shaderModules{
            context.shaderCache->CreateShaderModule(vk::ShaderStageFlagBits::eVertex, Filepath("~/Shaders/Test.vert"),
                    {}),
            context.shaderCache->CreateShaderModule(vk::ShaderStageFlagBits::eFragment, Filepath("~/Shaders/Test.frag"),
                    {})
        };

        ShaderCompiler::Finalize();

        const VertexDescription vertexDescription{
            { vk::Format::eR32G32B32Sfloat, vk::Format::eR32G32B32Sfloat },
            vk::VertexInputRate::eVertex
        };

        const GraphicsPipelineDescription description{
            context.swapchain->GetExtent(), vk::PrimitiveTopology::eTriangleList,
            vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack, vk::FrontFace::eClockwise,
            vk::SampleCountFlagBits::e1, std::nullopt, shaderModules, { vertexDescription },
            { eBlendMode::kDisabled }, {}, {}
        };

        return GraphicsPipeline::Create(context.device, renderPass.Get(), description);
    }

    BufferHandle CreateVertexBuffer(const VulkanContext &context)
    {
        struct Vertex
        {
            glm::vec3 position;
            glm::vec3 color;
        };

        const std::vector<Vertex> vertices{
            { glm::vec3(0.0f, -0.5f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f) },
            { glm::vec3(0.5f, 0.5f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f) },
            { glm::vec3(-0.5f, 0.5f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f) }
        };

        const BufferDescription description{
            sizeof(Vertex) * vertices.size(),
            vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal
        };

        return context.bufferManager->CreateBuffer(description, vertices);
    }
}

RenderSystem::RenderSystem(const Window &window)
{
    vulkanContext = std::make_unique<VulkanContext>(window);
    renderPass = SRenderSystem::CreateRenderPass(GetRef(vulkanContext));
    pipeline = SRenderSystem::CreateGraphicsPipeline(GetRef(vulkanContext), GetRef(renderPass));
    framebuffers = SRenderSystem::CreateFramebuffers(GetRef(vulkanContext), GetRef(renderPass));
    vertexBuffer = SRenderSystem::CreateVertexBuffer(GetRef(vulkanContext));

    frames.resize(framebuffers.size());
    for (auto &frame : frames)
    {
        frame.commandBuffer = vulkanContext->device->AllocateCommandBuffer(eCommandsType::kOneTime);
        frame.presentCompleteSemaphore = VulkanHelpers::CreateSemaphore(GetRef(vulkanContext->device));
        frame.renderCompleteSemaphore = VulkanHelpers::CreateSemaphore(GetRef(vulkanContext->device));
        frame.fence = VulkanHelpers::CreateFence(GetRef(vulkanContext->device), vk::FenceCreateFlagBits::eSignaled);
    }

    static Texture textureAlpha = vulkanContext->textureCache->GetTexture(
            Filepath("~/Assets/Textures/logo-256x256.png"), {});
    static Texture textureSolid = vulkanContext->textureCache->GetTexture(
            Filepath("~/Assets/Textures/logo-256x256-solid.png"), {});
}

RenderSystem::~RenderSystem()
{
    const vk::Result result = vulkanContext->device->Get().waitIdle();
    Assert(result == vk::Result::eSuccess);

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

void RenderSystem::Process() const
{
    vulkanContext->bufferManager->UpdateMarkedBuffers();
    vulkanContext->imageManager->UpdateMarkedImages();
    vulkanContext->resourceUpdateSystem->ExecuteUpdateCommands();
}

void RenderSystem::Draw()
{
    const vk::SwapchainKHR swapchain = vulkanContext->swapchain->Get();
    const vk::Device device = vulkanContext->device->Get();

    auto [graphicsQueue, presentQueue] = vulkanContext->device->GetQueues();
    auto [commandBuffer, presentCompleteSemaphore, renderCompleteSemaphore, fence] = frames[frameIndex];

    auto [result, imageIndex] = vulkanContext->device->Get().acquireNextImageKHR(swapchain,
            std::numeric_limits<uint64_t>::max(), presentCompleteSemaphore,
            nullptr);
    Assert(result == vk::Result::eSuccess);

    while (device.waitForFences(1, &fence, true, std::numeric_limits<uint64_t>::max()) == vk::Result::eTimeout) {}

    result = device.resetFences(1, &fence);
    Assert(result == vk::Result::eSuccess);

    const vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    result = commandBuffer.begin(beginInfo);
    Assert(result == vk::Result::eSuccess);

    DrawInternal(commandBuffer, imageIndex);

    result = commandBuffer.end();
    Assert(result == vk::Result::eSuccess);

    const vk::PipelineStageFlags waitStageFlags = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    const vk::SubmitInfo submitInfo(1, &presentCompleteSemaphore, &waitStageFlags,
            1, &commandBuffer, 1, &renderCompleteSemaphore);

    result = graphicsQueue.submit(1, &submitInfo, fence);
    Assert(result == vk::Result::eSuccess);

    const vk::PresentInfoKHR presentInfo(1, &renderCompleteSemaphore, 1, &swapchain, &imageIndex, nullptr);

    result = presentQueue.presentKHR(presentInfo);
    Assert(result == vk::Result::eSuccess);

    frameIndex = (frameIndex + 1) % frames.size();
}

void RenderSystem::DrawInternal(vk::CommandBuffer commandBuffer, uint32_t imageIndex) const
{
    const vk::Image image = vulkanContext->swapchain->GetImages()[frameIndex];
    vulkanContext->resourceUpdateSystem->GetLayoutUpdateCommands(image, VulkanHelpers::kSubresourceRangeColor,
            vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal)(commandBuffer);

    const vk::Extent2D &extent = vulkanContext->swapchain->GetExtent();

    const vk::Rect2D renderArea(vk::Offset2D(0, 0), extent);

    const vk::ClearValue clearValue(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f });

    const vk::RenderPassBeginInfo beginInfo(renderPass->Get(), framebuffers[imageIndex], renderArea, 1, &clearValue);

    commandBuffer.beginRenderPass(beginInfo, vk::SubpassContents::eInline);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline->Get());

    const vk::DeviceSize offset = 0;
    commandBuffer.bindVertexBuffers(0, 1, &vertexBuffer->buffer, &offset);

    commandBuffer.draw(3, 1, 0, 0);

    commandBuffer.endRenderPass();
}
