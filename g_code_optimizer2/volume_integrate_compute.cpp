#include <array>

#include "volume_integrate_compute.hpp"
#include <nvvk/barriers.hpp>
#include <nvvk/check_error.hpp>
#include <nvvk/commands.hpp>
#include <nvvk/compute_pipeline.hpp>
#include <nvvk/debug_util.hpp>
#include <nvvk/default_structs.hpp>

#include <cmath>

VkResult nvshaders::VolumeIntegrateCompute::init(nvvk::ResourceAllocator* alloc, std::span<const uint32_t> spirv)
{
  assert(!m_device);
  m_alloc  = alloc;
  m_device = alloc->getDevice();

  // Shader descriptor set layout
  nvvk::DescriptorBindings bindings;
  bindings.addBinding(shaderio::volume_integrate_Binding::vInImage, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT);
  bindings.addBinding(shaderio::volume_integrate_Binding::vOutVolume, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);

  NVVK_CHECK(m_descriptorPack.init(bindings, m_device, 0, VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR));
  NVVK_DBG_NAME(m_descriptorPack.getLayout());

  // Push constant
  VkPushConstantRange pushConstantRange{.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                        .size       = sizeof(shaderio::VolumeIntegratePushConstant)};

  // Pipeline layout
  const VkPipelineLayoutCreateInfo pipelineLayoutInfo{
      .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount         = 1,
      .pSetLayouts            = m_descriptorPack.getLayoutPtr(),
      .pushConstantRangeCount = 1,
      .pPushConstantRanges    = &pushConstantRange,
  };
  NVVK_FAIL_RETURN(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout));
  NVVK_DBG_NAME(m_pipelineLayout);

  // Compute Pipeline
  VkComputePipelineCreateInfo compInfo   = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
  VkShaderModuleCreateInfo    shaderInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  compInfo.stage                         = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
  compInfo.stage.stage                   = VK_SHADER_STAGE_COMPUTE_BIT;
  compInfo.stage.pNext                   = &shaderInfo;
  compInfo.layout                        = m_pipelineLayout;

  shaderInfo.codeSize = uint32_t(spirv.size_bytes());  // All shaders are in the same spirv
  shaderInfo.pCode    = spirv.data();

  // Volume calculation pipeline
  compInfo.stage.pName = "IntegrateVolume";
  NVVK_FAIL_RETURN(vkCreateComputePipelines(m_device, nullptr, 1, &compInfo, nullptr, &m_volumeIntegrationPipeline));
  NVVK_DBG_NAME(m_volumeIntegrationPipeline);

  return VK_SUCCESS;
}

void nvshaders::VolumeIntegrateCompute::deinit()
{
  if(!m_device)
    return;

  vkDestroyPipeline(m_device, m_volumeIntegrationPipeline, nullptr);
  vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
  m_descriptorPack.deinit();

  m_pipelineLayout            = VK_NULL_HANDLE;
  m_volumeIntegrationPipeline = VK_NULL_HANDLE;
  m_device                    = VK_NULL_HANDLE;
}

void nvshaders::VolumeIntegrateCompute::runCompute(VkCommandBuffer cmd,
                                                   VkImageView     srcImageView,
                                                   nvvk::Buffer*   dstBuffer,
                                                   shaderio::uint2 textureSize,
                                                   shaderio::float2 areaSize)
{
	shaderio::uint2 groupCount = {std::ceil(textureSize.x / (float)VOLUME_INTEGRATE_SHADER_WG_SIZE),
                                std::ceil(textureSize.y / (float)VOLUME_INTEGRATE_SHADER_WG_SIZE)};

	pushConst.image_size = textureSize;
	pushConst.area_size  = areaSize;

	// Push constant
    vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(shaderio::VolumeIntegratePushConstant), &pushConst);

	nvvk::WriteSetContainer writeSetContainer;
	writeSetContainer.append(m_descriptorPack.makeWrite(shaderio::volume_integrate_Binding::vInImage), srcImageView, VK_IMAGE_LAYOUT_GENERAL);
	writeSetContainer.append(m_descriptorPack.makeWrite(shaderio::volume_integrate_Binding::vOutVolume), dstBuffer);
	vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, writeSetContainer.size(),
								writeSetContainer.data());

	// Run
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_volumeIntegrationPipeline);
    vkCmdDispatch(cmd, groupCount.x, groupCount.y, 1);

	// Barrier
	nvvk::cmdMemoryBarrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
}
