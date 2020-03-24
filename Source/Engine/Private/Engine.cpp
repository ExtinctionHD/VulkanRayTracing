#include "Engine/Engine.hpp"

#include "Engine/Config.hpp"
#include "Engine/EngineHelpers.hpp"
#include "Engine/CameraSystem.hpp"
#include "Engine/Scene/SceneLoader.hpp"
#include "Engine/Render/UIRenderSystem.hpp"
#include "Engine/Render/RenderSystem.hpp"
#include "Engine/Render/Vulkan/VulkanContext.hpp"

namespace SEngine
{
    CameraDescription GetCameraInfo(const vk::Extent2D &extent)
    {
        return CameraDescription{
            glm::vec3(0.0f, 0.0f, -5.0f),
            Direction::kForward,
            Direction::kUp,
            90.0f, extent.width / static_cast<float>(extent.height),
            0.01f, 100.0f
        };
    }

    const CameraParameters kCameraParameters{
        1.0f, 2.0f, 6.0f
    };

    const CameraKeyBindings kCameraKeyBindings{
        Key::eW, Key::eS, Key::eA, Key::eD,
        Key::eSpace, Key::eLeftControl, Key::eLeftShift
    };
}

Engine::Engine()
{
    window = std::make_unique<Window>(Config::kExtent, Config::kWindowMode);
    window->SetResizeCallback(MakeFunction(&Engine::ResizeCallback, this));
    window->SetKeyInputCallback(MakeFunction(&Engine::KeyInputCallback, this));
    window->SetMouseInputCallback(MakeFunction(&Engine::MouseInputCallback, this));
    window->SetMouseMoveCallback(MakeFunction(&Engine::MouseMoveCallback, this));

    VulkanContext::Create(GetRef(window));

    camera = std::make_shared<Camera>(SEngine::GetCameraInfo(window->GetExtent()));
    scene = SceneLoader::LoadFromFile(Filepath("~/Assets/Scenes/BoxTextured/BoxTextured.gltf"));

    systems.emplace_back(new CameraSystem(camera, SEngine::kCameraParameters, SEngine::kCameraKeyBindings));
    systems.emplace_back(new UIRenderSystem(GetRef(window)));
    UIRenderSystem *uiRenderSystem = dynamic_cast<UIRenderSystem *>(systems.back().get());
    systems.emplace_back(new RenderSystem(camera, MakeFunction(&UIRenderSystem::Render, uiRenderSystem)));
}

Engine::~Engine()
{
    VulkanContext::device->WaitIdle();
}

void Engine::Run()
{
    while (!window->ShouldClose())
    {
        window->PollEvents();

        for (auto &system : systems)
        {
            system->Process(timer.GetDeltaSeconds());
        }
    }
}

void Engine::ResizeCallback(const vk::Extent2D &extent) const
{
    VulkanContext::device->WaitIdle();

    if (extent.width > 0 && extent.height > 0)
    {
        const SwapchainDescription description{
            extent, Config::kVSyncEnabled
        };

        VulkanContext::swapchain->Recreate(description);
    }

    for (auto &system : systems)
    {
        system->OnResize(extent);
    }
}

void Engine::KeyInputCallback(Key key, KeyAction action, ModifierFlags modifiers) const
{
    for (auto &system : systems)
    {
        system->OnKeyInput(key, action, modifiers);
    }
}

void Engine::MouseInputCallback(MouseButton button, MouseButtonAction action, ModifierFlags modifiers) const
{
    for (auto &system : systems)
    {
        system->OnMouseInput(button, action, modifiers);
    }
}

void Engine::MouseMoveCallback(const glm::vec2 &position) const
{
    for (auto &system : systems)
    {
        system->OnMouseMove(position);
    }
}
