#pragma once

#include "Engine/Render/Vulkan/Device.hpp"
#include "Engine/Render/Vulkan/Resources/Buffer.hpp"
#include "Engine/Render/Vulkan/Resources/MemoryManager.hpp"

#include "Utils/Flags.hpp"

enum class eBufferAccessFlagBits
{
    kCpuMemory,
    kStagingBuffer,
};

using BufferAccessFlags = Flags<eBufferAccessFlagBits>;

OVERLOAD_LOGIC_OPERATORS(BufferAccessFlags, eBufferAccessFlagBits)

class BufferManager
{
public:
    BufferManager(std::shared_ptr<Device> aDevice, std::shared_ptr<MemoryManager> aMemoryManager);
    ~BufferManager();

    BufferHandle CreateBuffer(const BufferDescription &description, BufferAccessFlags bufferAccess);

    template <class T>
    BufferHandle CreateBuffer(const BufferDescription &description, BufferAccessFlags bufferAccess,
            const std::vector<T> &initialData);

    void UpdateBuffer(BufferHandle handle, vk::CommandBuffer commandBuffer);

    void DestroyBuffer(BufferHandle handle);

private:
    std::shared_ptr<Device> device;
    std::shared_ptr<MemoryManager> memoryManager;

    std::map<Buffer *, vk::Buffer> buffers;
};

template <class T>
BufferHandle BufferManager::CreateBuffer(const BufferDescription &description,
        BufferAccessFlags bufferAccess, const std::vector<T> &initialData)
{
    const BufferHandle handle = CreateBuffer(description, bufferAccess);
    auto [data, size] = handle->AccessCpuData<T>();

    Assert(initialData.size() <= size);

    std::copy(initialData.begin(), initialData.end(), data);

    device->ExecuteOneTimeCommands([this, &handle](vk::CommandBuffer commandBuffer)
        {
            UpdateBuffer(handle, commandBuffer);
        });

    return handle;
}
