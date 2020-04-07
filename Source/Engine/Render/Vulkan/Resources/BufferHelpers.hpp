#pragma once

#include "Engine/Render/Vulkan/VulkanHelpers.hpp"

#include "Utils/Flags.hpp"
#include "Utils/DataHelpers.hpp"

struct BufferDescription
{
    vk::DeviceSize size;
    vk::BufferUsageFlags usage;
    vk::MemoryPropertyFlags memoryProperties;
};

enum class BufferCreateFlagBits
{
    eStagingBuffer,
};

using BufferCreateFlags = Flags<BufferCreateFlagBits>;

OVERLOAD_LOGIC_OPERATORS(BufferCreateFlags, BufferCreateFlagBits)

namespace BufferHelpers
{
    vk::DescriptorBufferInfo GetInfo(vk::Buffer buffer);

    void SetupPipelineBarrier(vk::CommandBuffer commandBuffer,
            vk::Buffer buffer, const PipelineBarrier &barrier);

    vk::Buffer CreateVertexBuffer(vk::DeviceSize size);

    vk::Buffer CreateIndexBuffer(vk::DeviceSize size);

    vk::Buffer CreateStorageBuffer(vk::DeviceSize size);

    vk::Buffer CreateUniformBuffer(vk::DeviceSize size);

    void UpdateBuffer(vk::CommandBuffer commandBuffer, vk::Buffer buffer,
            const ByteView &data, const SyncScope &blockedScope);
}
