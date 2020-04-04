#include "Engine/Render/Vulkan/Resources/BufferHelpers.hpp"

#include "Engine/Render/Vulkan/VulkanContext.hpp"

void BufferHelpers::SetupPipelineBarrier(vk::CommandBuffer commandBuffer,
        vk::Buffer buffer, vk::DeviceSize size, const PipelineBarrier &barrier)
{
    const vk::BufferMemoryBarrier bufferMemoryBarrier(
            barrier.waitedScope.access, barrier.blockedScope.access,
            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, buffer, 0, size);

    commandBuffer.pipelineBarrier(barrier.waitedScope.stages, barrier.blockedScope.stages,
            vk::DependencyFlags(), {}, { bufferMemoryBarrier }, {});
}

vk::Buffer BufferHelpers::CreateUniformBuffer(vk::DeviceSize size)
{
    const BufferDescription description{
        size, vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal
    };

    return VulkanContext::bufferManager->CreateBuffer(description, BufferCreateFlagBits::eStagingBuffer);
}

void BufferHelpers::UpdateUniformBuffer(vk::CommandBuffer commandBuffer, vk::Buffer buffer,
        const ByteView &byteView, const SyncScope &blockedScope)
{
    VulkanContext::bufferManager->UpdateBuffer(commandBuffer, buffer, byteView);

    const PipelineBarrier barrier{
        SyncScope::kTransferWrite,
        blockedScope
    };

    BufferHelpers::SetupPipelineBarrier(commandBuffer, buffer, byteView.size, barrier);
}
