#pragma once

#include "Engine/Render/Vulkan/Device.hpp"

struct DescriptorDescription
{
    vk::DescriptorType type;
    vk::ShaderStageFlags stageFlags;

    bool operator ==(const DescriptorDescription &other) const;
};

using DescriptorSetDescription = std::vector<DescriptorDescription>;

using DescriptorInfo = std::variant<vk::DescriptorBufferInfo, vk::DescriptorImageInfo, vk::BufferView>;

struct DescriptorData
{
    vk::DescriptorType type;
    DescriptorInfo info;
};

using DescriptorSetData = std::vector<DescriptorData>;

class DescriptorPool
{
public:
    static std::unique_ptr<DescriptorPool> Create(std::shared_ptr<Device> device,
            const std::vector<vk::DescriptorPoolSize> &descriptorPoolSizes, uint32_t maxSetCount);
    ~DescriptorPool();

    vk::DescriptorSetLayout CreateDescriptorSetLayout(const DescriptorSetDescription &description);

    void DestroyDescriptorSetLayout(vk::DescriptorSetLayout layout);

    std::vector<vk::DescriptorSet> AllocateDescriptorSets(
            const std::vector<vk::DescriptorSetLayout> &layouts) const;

    vk::DescriptorSet AllocateDescriptorSet(vk::DescriptorSetLayout layout) const;

    void UpdateDescriptorSet(vk::DescriptorSet descriptorSet,
            const DescriptorSetData &descriptorSetData, uint32_t bindingOffset = 0);

    void PerformUpdate();

private:
    struct LayoutCacheEntry
    {
        DescriptorSetDescription description;
        vk::DescriptorSetLayout layout;
    };

    std::shared_ptr<Device> device;

    vk::DescriptorPool descriptorPool;

    std::list<LayoutCacheEntry> layoutCache;

    std::vector<vk::WriteDescriptorSet> descriptorWrites;

    DescriptorPool(std::shared_ptr<Device> aDevice, vk::DescriptorPool aDescriptorPool);
};
