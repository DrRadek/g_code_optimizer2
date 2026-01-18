#pragma once
#include <glm/glm.hpp>

#include "vulkan/vulkan_core.h"
#include "nvvk/resource_allocator.hpp"

#include <nvshaders/tonemap_io.h.slang>
#include <nvutils/timers.hpp>
#include <nvvk/descriptors.hpp>

#include "shaders/shaderio.h"

namespace nvshaders {

class VolumeSumCompute
{
public:
  VolumeSumCompute() {};
  ~VolumeSumCompute() { assert(m_device == VK_NULL_HANDLE); }  //  "Missing to call deinit"

  VkResult init(nvvk::ResourceAllocator* alloc, std::span<const uint32_t> spirv);
  void     deinit();

  void  runCompute(VkCommandBuffer cmd, int elementCount, nvvk::Buffer* srcBuffer, nvvk::Buffer* dstBuffer);
  void  recordCopyResultToStaging(VkCommandBuffer cmd);
  bool  IsResultBufferValid() { return resultBuffer != nullptr; }
  float readResult();

  int calculateMaxGroups(int elementCount);

private:
  nvvk::ResourceAllocator* m_alloc{};

  VkDevice             m_device{};
  nvvk::DescriptorPack m_descriptorPack;
  VkPipelineLayout     m_pipelineLayout{};
  VkPipeline           m_volumeCalculationPipeline{};

  nvvk::Buffer  stagingBuffer;
  nvvk::Buffer* resultBuffer = nullptr;

  shaderio::volume_Params params_data{};

  const shaderio::uint WG            = VOLUMESUM_SHADER_WG_SIZE;
  const shaderio::uint K             = 4;  // elemsPerThread
  const shaderio::uint itemsPerGroup = WG * K;
};


}  // namespace nvshaders
