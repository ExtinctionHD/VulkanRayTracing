#pragma once

#include "Engine/Render/Vulkan/RayTracing/AccelerationStructureHelpers.hpp"

class AccelerationStructureManager
{
public:
    vk::AccelerationStructureKHR GenerateBoundingBoxBlas();

    vk::AccelerationStructureKHR GenerateBlas(const GeometryVertexData& vertexData, const GeometryIndexData& indexData);

    vk::AccelerationStructureKHR GenerateTlas(const std::vector<GeometryInstanceData>& instances);

    void DestroyAccelerationStructure(vk::AccelerationStructureKHR accelerationStructure);

private:
    std::map<vk::AccelerationStructureKHR, vk::Buffer> accelerationStructures;
};
