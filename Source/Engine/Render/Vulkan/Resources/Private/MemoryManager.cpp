#define VMA_IMPLEMENTATION

#include "Engine/Render/Vulkan/Resources/MemoryManager.hpp"

#include "Engine/Render/Vulkan/VulkanContext.hpp"

namespace Details
{
    VmaAllocationCreateInfo GetAllocationCreateInfo(vk::MemoryPropertyFlags memoryProperties)
    {
        VmaAllocationCreateInfo allocationCreateInfo = {};
        allocationCreateInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(memoryProperties);

        return allocationCreateInfo;
    }
}

MemoryManager::MemoryManager()
{
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.instance = VulkanContext::instance->Get();
    allocatorInfo.physicalDevice = VulkanContext::device->GetPhysicalDevice();
    allocatorInfo.device = VulkanContext::device->Get();
    allocatorInfo.flags = VmaAllocatorCreateFlagBits::VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    vmaCreateAllocator(&allocatorInfo, &allocator);
}

MemoryManager::~MemoryManager()
{
    vmaDestroyAllocator(allocator);
}

MemoryBlock MemoryManager::AllocateMemory(const vk::MemoryRequirements& memoryRequirements,
        vk::MemoryPropertyFlags memoryProperties)
{
    const VmaAllocationCreateInfo allocationCreateInfo = Details::GetAllocationCreateInfo(memoryProperties);

    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo;

    vmaAllocateMemory(allocator, &memoryRequirements.operator struct VkMemoryRequirements const&(),
            &allocationCreateInfo, &allocation, &allocationInfo);

    const MemoryBlock memoryBlock{
        allocationInfo.deviceMemory,
        allocationInfo.offset,
        allocationInfo.size
    };

    memoryAllocations.emplace(memoryBlock, allocation);

    return memoryBlock;
}

void MemoryManager::FreeMemory(const MemoryBlock& memoryBlock)
{
    const auto it = memoryAllocations.find(memoryBlock);
    Assert(it != memoryAllocations.end());

    vmaFreeMemory(allocator, it->second);

    memoryAllocations.erase(it);
}

void MemoryManager::CopyDataToMemory(const ByteView& data, const MemoryBlock& memoryBlock) const
{
    Assert(data.size <= memoryBlock.size);

    void* mappedMemory = nullptr;

    const vk::Result result = VulkanContext::device->Get().mapMemory(memoryBlock.memory,
            memoryBlock.offset, data.size, vk::MemoryMapFlags(),
            &mappedMemory);

    Assert(result == vk::Result::eSuccess);

    std::copy(data.data, data.data + data.size, static_cast<uint8_t*>(mappedMemory));

    VulkanContext::device->Get().unmapMemory(memoryBlock.memory);
}

vk::Buffer MemoryManager::CreateBuffer(const vk::BufferCreateInfo& createInfo, vk::MemoryPropertyFlags memoryProperties)
{
    const VmaAllocationCreateInfo allocationCreateInfo = Details::GetAllocationCreateInfo(memoryProperties);

    VkBuffer buffer;
    VmaAllocation allocation;

    const VkResult result = vmaCreateBuffer(allocator, &createInfo.operator struct VkBufferCreateInfo const&(),
            &allocationCreateInfo, &buffer, &allocation, nullptr);

    Assert(result == VK_SUCCESS);

    bufferAllocations.emplace(buffer, allocation);

    return buffer;
}

void MemoryManager::DestroyBuffer(vk::Buffer buffer)
{
    const auto it = bufferAllocations.find(buffer);
    Assert(it != bufferAllocations.end());

    vmaDestroyBuffer(allocator, buffer, it->second);

    bufferAllocations.erase(it);
}

vk::Image MemoryManager::CreateImage(const vk::ImageCreateInfo& createInfo, vk::MemoryPropertyFlags memoryProperties)
{
    const VmaAllocationCreateInfo allocationCreateInfo = Details::GetAllocationCreateInfo(memoryProperties);

    VkImage image;
    VmaAllocation allocation;

    const VkResult result = vmaCreateImage(allocator, &createInfo.operator struct VkImageCreateInfo const&(),
            &allocationCreateInfo, &image, &allocation, nullptr);

    Assert(result == VK_SUCCESS);

    imageAllocations.emplace(image, allocation);

    return image;
}

void MemoryManager::DestroyImage(vk::Image image)
{
    const auto it = imageAllocations.find(image);
    Assert(it != imageAllocations.end());

    vmaDestroyImage(allocator, image, it->second);

    imageAllocations.erase(it);
}

MemoryBlock MemoryManager::GetBufferMemoryBlock(vk::Buffer buffer) const
{
    return GetObjectMemoryBlock(buffer, bufferAllocations);
}

MemoryBlock MemoryManager::GetImageMemoryBlock(vk::Image image) const
{
    return GetObjectMemoryBlock(image, imageAllocations);
}

MemoryBlock MemoryManager::GetAccelerationStructureMemoryBlock(vk::AccelerationStructureKHR accelerationStructure) const
{
    return GetObjectMemoryBlock(accelerationStructure, accelerationStructureAllocations);
}
