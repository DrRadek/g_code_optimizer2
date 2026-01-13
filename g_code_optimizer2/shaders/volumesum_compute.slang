#include "nvshaders/functions.h.slang"

#include "shaderio.h"

groupshared float sum[VOLUMESUM_SHADER_WG_SIZE];

layout(push_constant) ConstantBuffer<volume_Params> params;

layout(binding = volumesum_Binding::PartialVolume) StructuredBuffer<float> src;
layout(binding = volumesum_Binding::OutVolume)    RWStructuredBuffer<float> dst;

[shader("compute")]
[numthreads(VOLUMESUM_SHADER_WG_SIZE, 1, 1)]
void CalculateVolume(
    uint3 globalThreadID : SV_DispatchThreadID,
    uint3 groupID        : SV_GroupID,
    uint3 localThreadID  : SV_GroupThreadID,
    uint  groupIndex     : SV_GroupIndex)
{
    const uint WG = VOLUMESUM_SHADER_WG_SIZE;
    uint gid = groupID.x;
    uint lid = localThreadID.x;

    uint itemsPerGroup = WG * params.groupSize;
    uint start = gid * itemsPerGroup + lid;

    // Accumulate up to K items per thread
    float localSum = 0.0f;
    for(uint k = 0; k < params.groupSize; ++k)
    {
        uint idx = start + k * VOLUMESUM_SHADER_WG_SIZE;
        if(idx < params.arraySize)
            localSum += src[idx];
    }

    // Store into shared memory
    sum[lid] = localSum;
    GroupMemoryBarrierWithGroupSync();

    // Reduce
    uint stride = WG >> 1;
    while(stride > 0)
    {
        if(lid < stride)
            sum[lid] += sum[lid + stride];
        GroupMemoryBarrierWithGroupSync();
        stride >>= 1;
    }

    // Thread 0 writes result
    if(lid == 0)
    {
        dst[gid] = sum[0];
    }
}
