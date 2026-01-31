#include <array>

#include "volumesum_compute.hpp"
#include <nvvk/barriers.hpp>
#include <nvvk/check_error.hpp>
#include <nvvk/commands.hpp>
#include <nvvk/compute_pipeline.hpp>
#include <nvvk/debug_util.hpp>
#include <nvvk/default_structs.hpp>


VkResult nvshaders::VolumeSumCompute::init(nvvk::ResourceAllocator* alloc, std::span<const uint32_t> spirv)
{
  assert(!m_device);
  m_alloc  = alloc;
  m_device = alloc->getDevice();

  alloc->createBuffer(stagingBuffer, sizeof(float), VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,
                      VMA_MEMORY_USAGE_GPU_TO_CPU, VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
  NVVK_DBG_NAME(stagingBuffer.buffer);

  // Shader descriptor set layout
  nvvk::DescriptorBindings bindings;
  bindings.addBinding(shaderio::volumesum_Binding::PartialVolume, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
  bindings.addBinding(shaderio::volumesum_Binding::OutVolume, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);

  NVVK_CHECK(m_descriptorPack.init(bindings, m_device, 0, VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR));
  NVVK_DBG_NAME(m_descriptorPack.getLayout());

  // Push constant
  VkPushConstantRange pushConstantRange{.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .size = sizeof(shaderio::volume_Params)};

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
  compInfo.stage.pName = "CalculateVolume";
  NVVK_FAIL_RETURN(vkCreateComputePipelines(m_device, nullptr, 1, &compInfo, nullptr, &m_volumeCalculationPipeline));
  NVVK_DBG_NAME(m_volumeCalculationPipeline);

  return VK_SUCCESS;
}

void nvshaders::VolumeSumCompute::deinit()
{
  if(!m_device)
    return;

  vkDestroyPipeline(m_device, m_volumeCalculationPipeline, nullptr);
  vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
  m_descriptorPack.deinit();

  m_alloc->destroyBuffer(stagingBuffer);

  m_pipelineLayout            = VK_NULL_HANDLE;
  m_volumeCalculationPipeline = VK_NULL_HANDLE;
  m_device                    = VK_NULL_HANDLE;
}

#include <iostream>
void nvshaders::VolumeSumCompute::runCompute(VkCommandBuffer cmd, int elementCount, nvvk::Buffer* srcBuffer, nvvk::Buffer* dstBuffer)
{
  nvvk::cmdBufferMemoryBarrier(cmd, {srcBuffer->buffer, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                     VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT});

  nvvk::Buffer* finalBuffer = srcBuffer;

  size_t partialCount = elementCount;
  while(partialCount > 1)
  {
    shaderio::uint groupCount = std::ceil((double)partialCount / itemsPerGroup);

    params_data.arraySize = partialCount;
    params_data.groupSize = K;
    // Push constant
    vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(shaderio::volume_Params), &params_data);

    nvvk::WriteSetContainer writeSetContainer;
    writeSetContainer.append(m_descriptorPack.makeWrite(shaderio::volumesum_Binding::PartialVolume), srcBuffer);
    writeSetContainer.append(m_descriptorPack.makeWrite(shaderio::volumesum_Binding::OutVolume), dstBuffer);
    vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, writeSetContainer.size(),
                              writeSetContainer.data());

    // Run
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_volumeCalculationPipeline);
    vkCmdDispatch(cmd, groupCount, 1, 1);

    // Barrier
    nvvk::cmdMemoryBarrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    std::swap(srcBuffer, dstBuffer);
    finalBuffer  = srcBuffer;
    partialCount = groupCount;
  }
  resultBuffer = finalBuffer;
}

void nvshaders::VolumeSumCompute::recordCopyResultToStaging(VkCommandBuffer cmd)
{
  nvvk::cmdMemoryBarrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

  VkBufferCopy copyRegion{0, 0, sizeof(float)};
  vkCmdCopyBuffer(cmd, resultBuffer->buffer, stagingBuffer.buffer, 1, &copyRegion);

  nvvk::cmdMemoryBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);
}

float nvshaders::VolumeSumCompute::readResult()
{
  return reinterpret_cast<float*>(stagingBuffer.mapping)[0];
}

int nvshaders::VolumeSumCompute::calculateMaxGroups(int elementCount)
{
  return (int)std::ceil((double)elementCount / itemsPerGroup);
}
