#pragma once

#include "Engine/Render/Vulkan/Device.hpp"

namespace VulkanConfig
{
#ifdef NDEBUG
    constexpr bool kValidationEnabled = false;
#else
    constexpr bool kValidationEnabled = true;
#endif

    const std::vector<const char*> kRequiredExtensions = {};

    const std::vector<const char*> kRequiredDeviceExtensions{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
        VK_KHR_RAY_QUERY_EXTENSION_NAME,
    };

    constexpr Device::Features kRequiredDeviceFeatures{
        .samplerAnisotropy = true,
        .accelerationStructure = true,
        .rayTracingPipeline = true,
        .descriptorIndexing = true,
        .bufferDeviceAddress = true,
        .rayQuery = true
    };

    const std::vector<vk::DescriptorPoolSize> kDescriptorPoolSizes{
        { vk::DescriptorType::eUniformBuffer, 2048 },
        { vk::DescriptorType::eCombinedImageSampler, 2048 },
        { vk::DescriptorType::eStorageImage, 2048 },
        { vk::DescriptorType::eAccelerationStructureKHR, 512 }
    };

    constexpr uint32_t kSwapchainMinImageCount = 3;

    constexpr uint32_t kMaxDescriptorSetCount = 512;

    constexpr std::optional<float> kMaxAnisotropy = 16.0f;
}
