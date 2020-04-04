#include "Engine/Render/RenderSystem.hpp"

#include "Engine/Render/Vulkan/VulkanContext.hpp"
#include "Engine/Render/Vulkan/VulkanConfig.hpp"
#include "Engine/Render/Vulkan/Shaders/ShaderCompiler.hpp"
#include "Engine/Render/Rasterizer.hpp"
#include "Engine/Render/RayTracer.hpp"

#include "Utils/Helpers.hpp"
#include "Utils/Assert.hpp"

namespace SRenderSystem
{
    Texture CreateTexture()
    {
        const SamplerDescription samplerDescription{
            vk::Filter::eLinear, vk::Filter::eLinear,
            vk::SamplerMipmapMode::eLinear,
            vk::SamplerAddressMode::eRepeat,
            VulkanConfig::kMaxAnisotropy,
            0.0f, std::numeric_limits<float>::max()
        };

        const Filepath texturePath("~/Assets/Scenes/DamagedHelmet/Default_albedo.jpg");

        return VulkanContext::textureCache->GetTexture(texturePath, samplerDescription);
    }

    vk::AccelerationStructureNV GenerateBlas(const RenderObject &renderObject)
    {
        return VulkanContext::accelerationStructureManager->GenerateBlas(renderObject);
    }

    vk::AccelerationStructureNV GenerateTlas(vk::AccelerationStructureNV blas, const glm::mat4 &transform)
    {
        const GeometryInstance geometryInstance{ blas, transform };

        return VulkanContext::accelerationStructureManager->GenerateTlas({ geometryInstance });
    }
}

RenderSystem::RenderSystem(Scene &scene_, Camera &camera_,
        const RenderFunction &uiRenderFunction_)
    : uiRenderFunction(uiRenderFunction_)
{
    frames.resize(VulkanContext::swapchain->GetImageViews().size());
    for (auto &frame : frames)
    {
        frame.commandBuffer = VulkanContext::device->AllocateCommandBuffer(CommandBufferType::eOneTime);
        frame.sync.waitSemaphores.push_back(VulkanHelpers::CreateSemaphore(VulkanContext::device->Get()));
        frame.sync.signalSemaphores.push_back(VulkanHelpers::CreateSemaphore(VulkanContext::device->Get()));
        frame.sync.fence = VulkanHelpers::CreateFence(VulkanContext::device->Get(), vk::FenceCreateFlagBits::eSignaled);
        switch (renderFlow)
        {
        case RenderFlow::eRasterization:
            frame.sync.waitStages.emplace_back(vk::PipelineStageFlagBits::eColorAttachmentOutput);
            break;
        case RenderFlow::eRayTracing:
            frame.sync.waitStages.emplace_back(vk::PipelineStageFlagBits::eRayTracingShaderNV);
            break;
        }
    }

    ShaderCompiler::Initialize();

    rasterizer = std::make_unique<Rasterizer>(scene_, camera_);
    rayTracer = std::make_unique<RayTracer>(scene_, camera_);

    ShaderCompiler::Finalize();

    drawingSuspended = false;
}

RenderSystem::~RenderSystem()
{
    for (auto &frame : frames)
    {
        VulkanHelpers::DestroyCommandBufferSync(VulkanContext::device->Get(), frame.sync);
    }
}

void RenderSystem::Process(float)
{
    if (drawingSuspended) return;

    const vk::SwapchainKHR swapchain = VulkanContext::swapchain->Get();
    const vk::Device device = VulkanContext::device->Get();

    const auto &[graphicsQueue, presentQueue] = VulkanContext::device->GetQueues();
    const auto &[commandBuffer, synchronization] = frames[frameIndex];

    const vk::Semaphore presentCompleteSemaphore = synchronization.waitSemaphores.front();
    const vk::Semaphore renderingCompleteSemaphore = synchronization.signalSemaphores.front();
    const vk::Fence renderingFence = synchronization.fence;

    const auto &[acquireResult, imageIndex] = VulkanContext::device->Get().acquireNextImageKHR(
            swapchain, Numbers::kMaxUint,
            presentCompleteSemaphore, nullptr);

    if (acquireResult == vk::Result::eErrorOutOfDateKHR) return;
    Assert(acquireResult == vk::Result::eSuccess || acquireResult == vk::Result::eSuboptimalKHR);

    VulkanHelpers::WaitForFences(VulkanContext::device->Get(), { renderingFence });

    const vk::Result resetResult = device.resetFences(1, &renderingFence);
    Assert(resetResult == vk::Result::eSuccess);

    const DeviceCommands renderCommands = std::bind(&RenderSystem::Render,
            this, std::placeholders::_1, imageIndex);

    VulkanHelpers::SubmitCommandBuffer(graphicsQueue, commandBuffer,
            renderCommands, synchronization);

    const vk::PresentInfoKHR presentInfo(1, &renderingCompleteSemaphore,
            1, &swapchain, &imageIndex, nullptr);

    const vk::Result presentResult = presentQueue.presentKHR(presentInfo);
    Assert(presentResult == vk::Result::eSuccess);

    frameIndex = (frameIndex + 1) % frames.size();
}

void RenderSystem::OnResize(const vk::Extent2D &extent)
{
    drawingSuspended = extent.width == 0 || extent.height == 0;

    if (!drawingSuspended)
    {
        rasterizer->OnResize(extent);
        rayTracer->OnResize(extent);
    }
}

void RenderSystem::Render(vk::CommandBuffer commandBuffer, uint32_t imageIndex) const
{
    switch (renderFlow)
    {
    case RenderFlow::eRasterization:
        rasterizer->Render(commandBuffer, imageIndex);
        break;

    case RenderFlow::eRayTracing:
        rayTracer->Render(commandBuffer, imageIndex);
        break;
    }

    uiRenderFunction(commandBuffer, imageIndex);
}
