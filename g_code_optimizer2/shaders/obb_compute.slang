#include "shaderio.h"

#include "nvshaders/functions.h.slang"


groupshared float3 sMin[OBB_SHADER_WG_SIZE];
groupshared float3 sMax[OBB_SHADER_WG_SIZE];

[[vk::push_constant]] ConstantBuffer<obb_Params> params;

[[vk::binding(obb_Binding::Vertices)]]     StructuredBuffer<float3> vertices;
[[vk::binding(obb_Binding::PartialObb)]]  RWStructuredBuffer<OBB> partialObb;
[[vk::binding(obb_Binding::OutObb)]]      RWStructuredBuffer<OBB> outObb;

// Called with x WGs
[shader("compute")]
[numthreads(OBB_SHADER_WG_SIZE, 1, 1)]
void CalculateOBB_Pass1(
    uint3 globalThreadID : SV_DispatchThreadID,
    uint3 groupID        : SV_GroupID,
    uint3 localThreadID  : SV_GroupThreadID,
    uint  groupIndex     : SV_GroupIndex)
{
    uint gid = groupID.x;
    uint lid = localThreadID.x;

    // Initialize this thread's partial OBB
    float3 localMin = float3(float.maxValue);
    float3 localMax = float3(float.minValue);

    // Each thread processes triangles with stride OBB_SHADER_WG_SIZE
    for(uint i = lid + gid * OBB_SHADER_WG_SIZE; i < params.vertCount; i += OBB_SHADER_WG_SIZE * params.groupSize)
    {
        float3 t = mul((float3x3)params.projInvMatrix, vertices[i]);
        //float3 t = mul(params.projInvMatrix, float4(vertices[i], 0)).xyz;
        localMin = min(localMin, t);
        localMax = max(localMax, t);
    }

    // Store local results into shared memory
    sMin[lid] = localMin;
    sMax[lid] = localMax;

  GroupMemoryBarrierWithGroupSync();
   Reduce(lid, gid, true);
}

// Called with 1 final WG
[shader("compute")]
[numthreads(OBB_SHADER_WG_SIZE, 1, 1)]
void CalculateOBB_Pass2(
    uint3 globalThreadID : SV_DispatchThreadID,
    uint3 groupID        : SV_GroupID,
    uint3 localThreadID  : SV_GroupThreadID,
    uint  groupIndex     : SV_GroupIndex
)
{
    uint gid = groupID.x;
    uint lid = localThreadID.x;

    float3 localMin = float3(float.maxValue);
    float3 localMax = float3(float.minValue);

    // Reduce all partial OBBs
    for (uint i = lid; i < params.groupSize; i += OBB_SHADER_WG_SIZE)
    {
        OBB a = partialObb[i];
        localMin = min(localMin, a.min);
        localMax = max(localMax, a.max);
    }

    sMin[lid] = localMin;
    sMax[lid] = localMax;

  GroupMemoryBarrierWithGroupSync();
  Reduce(lid, gid, false);
}

void Reduce(uint lid, uint gid, bool writeToPartial){
 // Parallel reduction
    uint stride = OBB_SHADER_WG_SIZE >> 1;
    while(stride > 0)
    {
        if (lid < stride)
        {
            sMin[lid] = min(sMin[lid], sMin[lid + stride]);
            sMax[lid] = max(sMax[lid], sMax[lid + stride]);
        }
        GroupMemoryBarrierWithGroupSync();
        stride >>= 1;
    }

    // Thread 0 writes OBB
    if (lid == 0)
    {
        OBB outVal;
        if (params.vertCount == 0)
        {
            outVal.min = float3(float.maxValue,  float.maxValue,  float.maxValue);
            outVal.max = float3(float.minValue, float.minValue, float.minValue);
        }
        else
        {
            outVal.min = sMin[0];
            outVal.max = sMax[0];
        }
        if(writeToPartial)
            partialObb[gid] = outVal;
        else
            outObb[gid] = outVal;
    }
}
