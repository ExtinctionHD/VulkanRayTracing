#pragma once

#include "Engine/Render/Vulkan/Resources/Texture.hpp"
#include "Engine/Filesystem.hpp"

class TextureCache
{
public:
    TextureCache() = default;
    ~TextureCache();

    Texture GetTexture(const Filepath &filepath, const SamplerDescription &samplerDescription);

    vk::Sampler GetSampler(const SamplerDescription &description);

    Texture CreateColorTexture(const glm::vec3 &color, const SamplerDescription &samplerDescription);

    Texture CreateCubeTexture(const Texture &panoramaTexture,
            const vk::Extent2D &extent, const SamplerDescription &samplerDescription);

private:
    struct TextureEntry
    {
        vk::Image image;
        vk::ImageView view;
    };

    std::unordered_map<Filepath, TextureEntry> textures;
    std::unordered_map<SamplerDescription, vk::Sampler> samplers;
};
