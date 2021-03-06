#include "Engine/Systems/RenderSystem.hpp"

#include "Engine/Render/Vulkan/VulkanContext.hpp"
#include "Engine/Scene/Environment.hpp"
#include "Engine/Camera.hpp"
#include "Engine/Engine.hpp"
#include "Engine/EngineHelpers.hpp"
#include "Engine/InputHelpers.hpp"
#include "Engine/Render/Stages/ForwardStage.hpp"
#include "Engine/Render/Stages/GBufferStage.hpp"
#include "Engine/Render/Stages/LightingStage.hpp"

namespace Details
{
    std::vector<vk::ImageView> GetImageViews(const std::vector<Texture> textures)
    {
        std::vector<vk::ImageView> imageViews(textures.size());

        for (size_t i = 0; i < textures.size(); ++i)
        {
            imageViews[i] = textures[i].view;
        }

        return imageViews;
    }
}

RenderSystem::RenderSystem(Scene* scene_, Camera* camera_, Environment* environment_)
    : scene(scene_)
    , camera(camera_)
    , environment(environment_)
{
    SetupGBufferTextures();
    SetupRenderStages();

    Engine::AddEventHandler<vk::Extent2D>(EventType::eResize,
            MakeFunction(this, &RenderSystem::HandleResizeEvent));

    Engine::AddEventHandler<KeyInput>(EventType::eKeyInput,
            MakeFunction(this, &RenderSystem::HandleKeyInputEvent));
}

RenderSystem::~RenderSystem()
{
    for (const auto& texture : gBufferTextures)
    {
        VulkanContext::imageManager->DestroyImage(texture.image);
    }
}

void RenderSystem::Process(float) {}

void RenderSystem::Render(vk::CommandBuffer commandBuffer, uint32_t imageIndex) const
{
    gBufferStage->Execute(commandBuffer, imageIndex);

    lightingStage->Execute(commandBuffer, imageIndex);

    forwardStage->Execute(commandBuffer, imageIndex);
}

void RenderSystem::SetupGBufferTextures()
{
    const vk::Extent2D& extent = VulkanContext::swapchain->GetExtent();

    gBufferTextures.resize(GBufferStage::kFormats.size());

    for (size_t i = 0; i < gBufferTextures.size(); ++i)
    {
        constexpr vk::ImageUsageFlags colorImageUsage
                = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage;

        constexpr vk::ImageUsageFlags depthImageUsage
                = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;

        const vk::Format format = GBufferStage::kFormats[i];

        const vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1;

        const vk::ImageUsageFlags imageUsage = ImageHelpers::IsDepthFormat(format)
                ? depthImageUsage : colorImageUsage;

        gBufferTextures[i] = ImageHelpers::CreateRenderTarget(
                format, extent, sampleCount, imageUsage);
    }

    VulkanContext::device->ExecuteOneTimeCommands([this](vk::CommandBuffer commandBuffer)
        {
            const ImageLayoutTransition colorLayoutTransition{
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eGeneral,
                PipelineBarrier{
                    SyncScope::kWaitForNone,
                    SyncScope::kBlockNone
                }
            };

            const ImageLayoutTransition depthLayoutTransition{
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eDepthStencilAttachmentOptimal,
                PipelineBarrier{
                    SyncScope::kWaitForNone,
                    SyncScope::kBlockNone
                }
            };

            for (size_t i = 0; i < gBufferTextures.size(); ++i)
            {
                const vk::Image image = gBufferTextures[i].image;

                const vk::Format format = GBufferStage::kFormats[i];

                const vk::ImageSubresourceRange& subresourceRange = ImageHelpers::IsDepthFormat(format)
                        ? ImageHelpers::kFlatDepth : ImageHelpers::kFlatColor;

                const ImageLayoutTransition& layoutTransition = ImageHelpers::IsDepthFormat(format)
                        ? depthLayoutTransition : colorLayoutTransition;

                ImageHelpers::TransitImageLayout(commandBuffer, image, subresourceRange, layoutTransition);
            }
        });
}

void RenderSystem::SetupRenderStages()
{
    const std::vector<vk::ImageView> gBufferImageViews = Details::GetImageViews(gBufferTextures);

    gBufferStage = std::make_unique<GBufferStage>(scene, camera, gBufferImageViews);

    lightingStage = std::make_unique<LightingStage>(scene, camera, environment, gBufferImageViews);

    forwardStage = std::make_unique<ForwardStage>(scene, camera, environment, gBufferImageViews.back());
}

void RenderSystem::HandleResizeEvent(const vk::Extent2D& extent)
{
    if (extent.width != 0 && extent.height != 0)
    {
        for (const auto& texture : gBufferTextures)
        {
            VulkanContext::imageManager->DestroyImage(texture.image);
        }

        SetupGBufferTextures();

        const std::vector<vk::ImageView> gBufferImageViews = Details::GetImageViews(gBufferTextures);

        gBufferStage->Resize(gBufferImageViews);

        lightingStage->Resize(gBufferImageViews);

        forwardStage->Resize(gBufferImageViews.back());
    }
}

void RenderSystem::HandleKeyInputEvent(const KeyInput& keyInput) const
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

void RenderSystem::ReloadShaders() const
{
    VulkanContext::device->WaitIdle();

    gBufferStage->ReloadShaders();

    lightingStage->ReloadShaders();

    forwardStage->ReloadShaders();
}
