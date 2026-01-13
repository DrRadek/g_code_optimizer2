#include <array>

#include "aabb_compute.hpp"
#include <nvvk/barriers.hpp>
#include <nvvk/check_error.hpp>
#include <nvvk/commands.hpp>
#include <nvvk/compute_pipeline.hpp>
#include <nvvk/debug_util.hpp>
#include <nvvk/default_structs.hpp>

#include "shaders/shaderio.h"


VkResult nvshaders::AABBCompute::init(VkCommandBuffer               cmd,
                                      nvvk::ResourceAllocator*      alloc,
                                      std::span<const uint32_t>     spirv,
                                      std::vector<shaderio::float3> vertices)
{
  auto         vertCount = vertices.size();
  unsigned int groupSize = ceil(vertCount / 1024);

  assert(!m_device);
  m_alloc  = alloc;
  m_device = alloc->getDevice();

  // Create buffers
  alloc->createBuffer(m_vertBuffer, sizeof(shaderio::float3) * vertCount, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO);
  NVVK_DBG_NAME(m_vertBuffer.buffer);
  alloc->createBuffer(m_aabbBuffer_partial, sizeof(shaderio::AABB) * groupSize, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT,
                      VMA_MEMORY_USAGE_AUTO);
  NVVK_DBG_NAME(m_aabbBuffer_partial.buffer);
  alloc->createBuffer(m_aabbBuffer_final, sizeof(shaderio::AABB),
                      VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_GPU_TO_CPU,
                      VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
  NVVK_DBG_NAME(m_aabbBuffer_final.buffer);

  // Shader descriptor set layout
  nvvk::DescriptorBindings bindings;
  bindings.addBinding(shaderio::aabb_Binding::Vertices, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
  bindings.addBinding(shaderio::aabb_Binding::PartialAabb, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
  bindings.addBinding(shaderio::aabb_Binding::OutAabb, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);

  NVVK_CHECK(m_descriptorPack.init(bindings, m_device, 0, VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR));
  NVVK_DBG_NAME(m_descriptorPack.getLayout());

  // Push constant
  VkPushConstantRange pushConstantRange{.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .size = sizeof(shaderio::aabb_Params)};

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

  // AABB pipeline pass 1
  compInfo.stage.pName = "CalculateAABB_Pass1";
  NVVK_FAIL_RETURN(vkCreateComputePipelines(m_device, nullptr, 1, &compInfo, nullptr, &m_aabbPipelinePass1));
  NVVK_DBG_NAME(m_aabbPipelinePass1);

  // AABB pipeline pass 2
  compInfo.stage.pName = "CalculateAABB_Pass2";
  NVVK_FAIL_RETURN(vkCreateComputePipelines(m_device, nullptr, 1, &compInfo, nullptr, &m_aabbPipelinePass2));
  NVVK_DBG_NAME(m_aabbPipelinePass2);

  // Allocate data
  //nvvk::Buffer stagingBuffer;
  alloc->createBuffer(stagingBuffer, sizeof(shaderio::float3) * vertCount, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);
  memcpy(stagingBuffer.mapping, vertices.data(), sizeof(shaderio::float3) * vertCount);
  VkBufferCopy copyRegion{};
  copyRegion.srcOffset = 0;
  copyRegion.dstOffset = 0;
  copyRegion.size      = sizeof(shaderio::float3) * vertCount;
  vkCmdCopyBuffer(cmd, stagingBuffer.buffer, m_vertBuffer.buffer, 1, &copyRegion);

  // Update push constant
  aabb_params_data.vertCount = (shaderio::uint)vertCount;
  aabb_params_data.groupSize = groupSize;


  // Create WriteSetContainer
  //nvvk::WriteSetContainer writeSetContainer;
  writeSetContainer.append(m_descriptorPack.makeWrite(shaderio::aabb_Binding::Vertices), m_vertBuffer);
  writeSetContainer.append(m_descriptorPack.makeWrite(shaderio::aabb_Binding::PartialAabb), m_aabbBuffer_partial);
  writeSetContainer.append(m_descriptorPack.makeWrite(shaderio::aabb_Binding::OutAabb), m_aabbBuffer_final);

  return VK_SUCCESS;
}

void nvshaders::AABBCompute::cleanupAfterInit(nvvk::ResourceAllocator* alloc)
{
  m_alloc->destroyBuffer(stagingBuffer);
}

void nvshaders::AABBCompute::deinit()
{
  if(!m_device)
    return;

  m_alloc->destroyBuffer(m_vertBuffer);
  m_alloc->destroyBuffer(m_aabbBuffer_partial);
  m_alloc->destroyBuffer(m_aabbBuffer_final);

  vkDestroyPipeline(m_device, m_aabbPipelinePass1, nullptr);
  vkDestroyPipeline(m_device, m_aabbPipelinePass2, nullptr);
  vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
  m_descriptorPack.deinit();

  m_pipelineLayout    = VK_NULL_HANDLE;
  m_aabbPipelinePass1 = VK_NULL_HANDLE;
  m_aabbPipelinePass2 = VK_NULL_HANDLE;
  m_device            = VK_NULL_HANDLE;
}

#include <iostream>
void nvshaders::AABBCompute::runCompute(VkCommandBuffer cmd, const shaderio::float4x4 projInvMatrix)
{
  // Push constant
  aabb_params_data.projInvMatrix = projInvMatrix;

  vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(shaderio::aabb_Params), &aabb_params_data);


  vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, writeSetContainer.size(),
                            writeSetContainer.data());

  // Run pass 1 - calculate min/max + reduction within WG
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_aabbPipelinePass1);
  vkCmdDispatch(cmd, aabb_params_data.groupSize, 1, 1);

  // Barrier
  nvvk::cmdMemoryBarrier(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

  // Run pass 2 - final reduction on one WG
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_aabbPipelinePass2);
  vkCmdDispatch(cmd, 1, 1, 1);
}


shaderio::AABB nvshaders::AABBCompute::readResult(nvvk::ResourceAllocator* alloc)
{
  shaderio::AABB* aabbPtr = reinterpret_cast<shaderio::AABB*>(m_aabbBuffer_final.mapping);
  return aabbPtr[0];
}
