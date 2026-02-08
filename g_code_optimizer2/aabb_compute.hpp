#pragma once
#include <glm/glm.hpp>

#include "vulkan/vulkan_core.h"
#include "nvvk/resource_allocator.hpp"

#include <nvshaders/tonemap_io.h.slang>
#include <nvutils/timers.hpp>
#include <nvvk/descriptors.hpp>

#include "shaders/shaderio.h"

namespace nvshaders {

class AABBCompute
{
public:
  AABBCompute() {};
  ~AABBCompute() { assert(m_device == VK_NULL_HANDLE); }  //  "Missing to call deinit"

  VkResult init(VkCommandBuffer cmd, nvvk::ResourceAllocator* alloc, std::span<const uint32_t> spirv, std::vector<shaderio::float3> vertices);
  void cleanupAfterInit(nvvk::ResourceAllocator* alloc);
  void deinit();

  void           runCompute(VkCommandBuffer cmd, const shaderio::float4x4 projInvMatrix);
  shaderio::AABB readResult(nvvk::ResourceAllocator* alloc);

private:
  nvvk::ResourceAllocator* m_alloc{};

  VkDevice             m_device{};
  nvvk::DescriptorPack m_descriptorPack;
  VkPipelineLayout     m_pipelineLayout{};
  VkPipeline           m_aabbPipelinePass1{};
  VkPipeline           m_aabbPipelinePass2{};

  nvutils::PerformanceTimer m_timer;  // Timer for performance measurement

  nvvk::Buffer m_vertBuffer;
  nvvk::Buffer m_aabbBuffer_partial;
  nvvk::Buffer m_aabbBuffer_final;

  nvvk::Buffer stagingBuffer;

  shaderio::aabb_Params aabb_params_data{};

  nvvk::WriteSetContainer writeSetContainer;
};


}  // namespace nvshaders
