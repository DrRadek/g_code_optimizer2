#include "nvshaders/functions.h.slang"

#include "shaderio.h"

[[vk::push_constant]] ConstantBuffer<VolumeIntegratePushConstant> pushConst;

[[vk::binding(volume_integrate_Binding::vInImage)]] Texture2D<float> inImage; // n*m
[[vk::binding(volume_integrate_Binding::vOutVolume)]] RWStructuredBuffer<float> outVolume; // (n-1)*(m-1)

[shader("compute")]
[numthreads(VOLUME_INTEGRATE_SHADER_WG_SIZE, VOLUME_INTEGRATE_SHADER_WG_SIZE, 1)]
void IntegrateVolume(
    uint3 globalThreadID : SV_DispatchThreadID,
    uint3 groupID        : SV_GroupID,
    uint3 localThreadID  : SV_GroupThreadID,
    uint  groupIndex     : SV_GroupIndex)
{
    uint2 pos = globalThreadID.xy;
    uint x = pos.x;
    uint y = pos.y;

    if (x >= pushConst.image_size.x - 1 || y >= pushConst.image_size.y - 1)
        return;

    float a = inImage.Load(int3(x,     y,     0));
    float b = inImage.Load(int3(x + 1, y,     0));
    float c = inImage.Load(int3(x,     y + 1, 0));
    float d = inImage.Load(int3(x + 1, y + 1, 0));
    // 1/4 * (x_(i+1) + x_(i) + y_(i+1) + y_(i)) * width * height
    float area = 0.25f * (a + b + c + d) * pushConst.area_size.x * pushConst.area_size.y;

    uint index = y * (pushConst.image_size.x - 1) + x;
    outVolume[index] = area;
}
