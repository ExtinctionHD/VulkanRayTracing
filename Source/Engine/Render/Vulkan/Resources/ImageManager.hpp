#pragma once

#include "Engine/Render/Vulkan/Device.hpp"
#include "Engine/Render/Vulkan/Resources/Image.hpp"
#include "Engine/Render/Vulkan/Resources/MemoryManager.hpp"

class ImageManager
{
public:
    ImageManager(std::shared_ptr<Device> device_, std::shared_ptr<MemoryManager> memoryManager_);
    ~ImageManager();

    ImageHandle CreateImage(const ImageDescription &description, vk::DeviceSize stagingBufferSize);

    void CreateView(ImageHandle handle, const vk::ImageSubresourceRange &subresourceRange) const;

    void UpdateImage(ImageHandle handle, vk::CommandBuffer commandBuffer) const;

    void DestroyImage(ImageHandle handle);

private:
    std::shared_ptr<Device> device;
    std::shared_ptr<MemoryManager> memoryManager;

    std::map<ImageHandle, vk::Buffer> images;
};
