#pragma once
#include <glm/glm.hpp>

#include "vulkan/vulkan_core.h"
#include "nvvk/resource_allocator.hpp"

#include <nvutils/timers.hpp>
#include <nvvk/descriptors.hpp>

#include "shaders/shaderio.h"

namespace nvshaders {

class VolumeIntegrateCompute
{
public:
  VolumeIntegrateCompute() {};
  ~VolumeIntegrateCompute() { assert(m_device == VK_NULL_HANDLE); }  //  "Missing to call deinit"

  VkResult init(nvvk::ResourceAllocator* alloc, std::span<const uint32_t> spirv);
  void     deinit();

  void runCompute(VkCommandBuffer cmd, VkImageView srcImageView, nvvk::Buffer* dstBuffer, shaderio::uint2 textureSize, shaderio::float2 areaSize);

private:
  nvvk::ResourceAllocator* m_alloc{};

  VkDevice             m_device{};
  nvvk::DescriptorPack m_descriptorPack;
  VkPipelineLayout     m_pipelineLayout{};
  VkPipeline           m_volumeIntegrationPipeline{};

  shaderio::VolumeIntegratePushConstant pushConst{};
};


}  // namespace nvshaders
