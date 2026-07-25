#pragma once
#include <glm/glm.hpp>

#include "vulkan/vulkan_core.h"
#include "nvvk/resource_allocator.hpp"

#include <nvshaders/tonemap_io.h.slang>
#include <nvutils/timers.hpp>
#include <nvvk/descriptors.hpp>

#include "shaders/shaderio.h"

namespace nvshaders {

class OBBCompute
{
public:
  OBBCompute() {};
  ~OBBCompute() { assert(m_device == VK_NULL_HANDLE); }  //  "Missing to call deinit"

  VkResult init(VkCommandBuffer cmd, nvvk::ResourceAllocator* alloc, std::vector<shaderio::float3>& vertices);
  void     cleanupAfterInit();
  void     deinit();

  void           runCompute(VkCommandBuffer cmd, const shaderio::float4x4 projInvMatrix);
  shaderio::OBB readResult();

private:
  nvvk::ResourceAllocator* m_alloc{};

  VkDevice             m_device{};
  nvvk::DescriptorPack m_descriptorPack;
  VkPipelineLayout     m_pipelineLayout{};
  VkPipeline           m_obbPipelinePass1{};
  VkPipeline           m_obbPipelinePass2{};

  nvutils::PerformanceTimer m_timer;  // Timer for performance measurement

  nvvk::Buffer m_vertBuffer;
  nvvk::Buffer m_obbBuffer_partial;
  nvvk::Buffer m_obbBuffer_final;

  nvvk::Buffer stagingBuffer;

  shaderio::obb_Params obb_params_data{};

  nvvk::WriteSetContainer writeSetContainer;
};


}  // namespace nvshaders
