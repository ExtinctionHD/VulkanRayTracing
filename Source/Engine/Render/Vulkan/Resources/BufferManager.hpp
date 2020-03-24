#pragma once

#include "Engine/Render/Vulkan/Device.hpp"
#include "Engine/Render/Vulkan/Resources/Buffer.hpp"
#include "Engine/Render/Vulkan/Resources/MemoryManager.hpp"
#include "Engine/Render/Vulkan/Resources/ResourcesHelpers.hpp"

#include "Utils/Flags.hpp"

struct SyncScope;

enum class BufferCreateFlagBits
{
    eStagingBuffer,
};

using BufferCreateFlags = Flags<BufferCreateFlagBits>;

OVERLOAD_LOGIC_OPERATORS(BufferCreateFlags, BufferCreateFlagBits)

class BufferManager
        : SharedStagingBufferProvider
{
public:
    BufferManager() = default;
    ~BufferManager();

    BufferHandle CreateBuffer(const BufferDescription &description, BufferCreateFlags createFlags);

    BufferHandle CreateBuffer(const BufferDescription &description, BufferCreateFlags createFlags,
            const ByteView &initialData, const SyncScope &blockedScope);

    void DestroyBuffer(BufferHandle handle);

    void UpdateBuffer(vk::CommandBuffer commandBuffer, BufferHandle handle, const ByteView &data);

private:
    std::map<BufferHandle, vk::Buffer> buffers;
};
