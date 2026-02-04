#include <nvshaders/slang_types.h>
#include "common/shaders/pbr.h.slang"
#include "shaderio.h"

// clang-format off
[[vk::push_constant]]                       ConstantBuffer<TutoPushConstant> pushConst;
// clang-format on

// Per-vertex attributes to be assembled from bound vertex buffers.
struct VSin
{
  float3 position : POSITION;
  float3 normal : NORMAl;
};

// Output of the vertex shader
struct VSout
{
  float4 sv_position : SV_Position;
  //float3 worldPos : POSITION;
  //float3 worldNormal : NORMAL;
};

// Output of the fragment shader
struct PSout
{
  float4 color : SV_Target0;
  float volume : SV_Target1;
};

__generic<T : IFloat> T getAttribute(uint8_t* dataBufferAddress, BufferView bufferView, uint attributeIndex)
{
  if(bufferView.count > 0)
  {
    T* ptr = (T*)(dataBufferAddress + bufferView.offset + attributeIndex * bufferView.byteStride);
    return ptr[0];
  }

  return T(1);  // Error case
}


// Vertex  Shader
[shader("vertex")]
VSout vertexMain(VSin input, uint vertexIndex: SV_VertexID)
{
  int instanceIndex = pushConst.instanceIndex;

  GltfSceneInfo* sceneInfo = pushConst.sceneInfoAddress;

  GltfInstance          instance = sceneInfo.instances[instanceIndex];
  GltfMesh              meshIo   = sceneInfo.meshes[instance.meshIndex];

  // Retrieve the data
  float3 posMesh  = getAttribute<float3>(meshIo.gltfBuffer, meshIo.triMesh.positions, vertexIndex);

  float4 pos = mul(float4(posMesh, 1.0), instance.transform);

  VSout output;
  output.sv_position   = mul(pos, sceneInfo.viewProjMatrix);

  return output;
}

// Fragment Shader
[shader("pixel")]
PSout fragmentMain(VSout stage)
{
  PSout output;

  float volume = (1 - stage.sv_position.z) * (pushConst.aabbMax.z - pushConst.aabbMin.z);
  output.volume = volume;

  // Relative visual color
  //output.color = float4(1.0,stage.sv_position.z,stage.sv_position.z, 1.0);

  // Global visual color
  float volumeScaled = volume / float(pushConst.maxSupportHeight);
  output.color = float4(volumeScaled, 1.0 - volumeScaled, 0.0, 1.0);

  return output;
}
