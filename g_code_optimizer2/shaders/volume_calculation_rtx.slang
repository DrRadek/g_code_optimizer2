/*
 * Copyright (c) 2023-2025, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025, NVIDIA CORPORATION.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "shaderio.h"

#include "nvshaders/constants.h.slang"

#include "common/shaders/pbr.h.slang"

#define MISS_DEPTH 1000


// clang-format off
// Push constants containing scene information, camera data, and material overrides
[[vk::push_constant]]                           ConstantBuffer<RtxPushConstant> pushConst;
// Top-level acceleration structure containing the scene geometry hierarchy
[[vk::binding(BindingPoints::eTlas, 1)]]        RaytracingAccelerationStructure topLevelAS;
// Output image where the final rendered result will be stored
[[vk::binding(BindingPoints::eOutImage, 1)]]    RWTexture2D<float4> outImage;
// Output volume array
[[vk::binding(BindingPoints::eOutVolume, 1)]]   RWTexture2D<float> outVolume;
// clang-format on


// Ray payload structure - carries data through the ray tracing pipeline
struct HitPayload
{
  float  accumDistance; // Accumulated distance according to rules
  float3 hitPos;        // Position of the most recent hit (world space)
  int    hit;           // 1 if this TraceRay returned a hit, 0 if miss
  int    depth;         // Used to determine which type of miss this was
};

// Generic function to retrieve vertex attributes from GLTF buffer data
// T: Type of attribute (float, float2, float3, etc.)
// dataBufferAddress: Base address of the GLTF buffer
// bufferView: Buffer view containing offset, stride, and count information
// attributeIndex: Index of the vertex attribute to retrieve
__generic<T : IFloat> T getAttribute(uint8_t* dataBufferAddress, BufferView bufferView, uint attributeIndex)
{
  if(bufferView.count > 0)
  {
    // Calculate pointer to the specific attribute using offset and stride
    T* ptr = (T*)(dataBufferAddress + bufferView.offset + attributeIndex * bufferView.byteStride);
    return ptr[0];
  }

  return T(1);  // Error case - return default value
}

// Retrieve triangle vertex indices from the mesh index buffer
// Supports both 16-bit and 32-bit index formats as per GLTF specification
int3 getTriangleIndices(uint8_t* dataBufferAddress, const TriangleMesh mesh, int primitiveID)
{
  if(mesh.indices.byteStride == sizeof(int16_t))
  {
    // 16-bit indices (uint16_t) - more memory efficient for smaller meshes
    int16_t3* indices = (int16_t3*)(dataBufferAddress + mesh.indices.offset);
    return indices[primitiveID];
  }
  else if(mesh.indices.byteStride == sizeof(int32_t))
  {
    // 32-bit indices (uint32_t) - supports larger meshes with more vertices
    int3* indices = (int3*)(dataBufferAddress + mesh.indices.offset);
    return indices[primitiveID];
  }

  return int3(-1);  // Error case - invalid index format
}

// Interpolate vertex attributes across a triangle using barycentric coordinates
// This performs smooth interpolation of attributes like position, normal, UV coordinates
// T: Type of attribute to interpolate (float, float2, float3, etc.)
// attributeIndex: Indices of the three triangle vertices
// barycentrics: Barycentric coordinates (weights for each vertex)
__generic<T : IFloat> T getTriangleAttribute(uint8_t* dataBufferAddress, BufferView bufferView, uint3 attributeIndex, float3 barycentrics)
{
  // Get the attribute value for each of the three triangle vertices
  T attr0 = getAttribute<T>(dataBufferAddress, bufferView, attributeIndex.x);
  T attr1 = getAttribute<T>(dataBufferAddress, bufferView, attributeIndex.y);
  T attr2 = getAttribute<T>(dataBufferAddress, bufferView, attributeIndex.z);
  
  // Interpolate using barycentric coordinates (weights sum to 1.0)
  return T(barycentrics.x) * attr0 + T(barycentrics.y) * attr1 + T(barycentrics.z) * attr2;
}


//-----------------------------------------------------------------------
// RAY GENERATION SHADER - Entry point for each pixel in the output image
//-----------------------------------------------------------------------
[shader("raygeneration")]
void rgenMain()
{
  // Get the current pixel coordinates and image dimensions
  int2 launchID   = DispatchRaysIndex().xy;     // Current pixel (x,y)
  int2 launchSize = DispatchRaysDimensions().xy; // Image size (width,height)

  // Retrieve scene information from push constants
  GltfSceneInfo* sceneInfo = pushConst.sceneInfoAddress;

  float3 aabbMin = pushConst.aabbMin;
  float3 aabbMax = pushConst.aabbMax;
  float2 launchIDCorrected = int2(launchID.x, launchSize.y - launchID.y);
  float2 pixelOffset = pushConst.aabbMin.xy + (pushConst.aabbMax.xy - pushConst.aabbMin.xy) * (launchIDCorrected / launchSize);

  // Set ray tracing flags (0 = no special flags)
  const uint   rayFlags   = 0;
  
  // Convert pixel coordinates to normalized device coordinates (NDC)
  // Range: [-1,1] for both x and y
  const float2 clipCoords = launchID / launchSize * 2.0 - 1.0;
  
  // Transform to view space using inverse projection matrix
  const float4 viewCoords = mul(float4(clipCoords, 1.0, 1.0), sceneInfo.projInvMatrix);


	// Construct view-space points (camera at origin, looking along +Z in view space)
	float3 topView    = float3(pixelOffset.x, pixelOffset.y, pushConst.aabbMax.z);
	float3 bottomView = float3(pixelOffset.x, pixelOffset.y, pushConst.aabbMin.z);

	// Transform by camera rotation only.
	// by using w=0 in the mul with a float4 which ignores translation component.
	float3 topWorld    = mul(float4(topView, 0.0), sceneInfo.viewInvMatrix).xyz;
	float3 bottomWorld = mul(float4(bottomView, 0.0), sceneInfo.viewInvMatrix).xyz;

  // Direction from top to bottom
  float3 worldDir = normalize(bottomWorld - topWorld);


  // Accumulator and counters
  float accumDistance = 0.0;
  int   hitCount = 0;

  // Starting origin slightly offset from top to avoid immediate self-intersection
  float3 origin = topWorld + worldDir * 0.001;
  // Track previous hit position
  float3 prevHitPos = topWorld;
  float  remainingT = length(bottomWorld - origin);

  // Loop up to a reasonable max number of interactions to avoid infinite loops
  const int MAX_STEPS = 64;
	HitPayload payload;
	payload.accumDistance = 0.0;
    payload.depth = 0;
	    payload.hitPos = bottomWorld;

  for(int step = 0; step < MAX_STEPS; ++step)
  {
    RayDesc ray;
    ray.Origin    = origin;
    ray.Direction = worldDir;
    ray.TMin      = 0.001;
    ray.TMax      = remainingT;

    payload.hit = 0;

    TraceRay(topLevelAS, rayFlags, 0xff, 0, 0, 0, ray, payload);
    if (payload.depth >= MISS_DEPTH){
      if(payload.depth == MISS_DEPTH){

      }
      else{
		payload.accumDistance += length(bottomWorld - payload.hitPos);
      }
	  		break;
    }

    float3 hitPos = payload.hit != 0 ? payload.hitPos : bottomWorld;

    // Distance between this hit and the previous hit position
    float   d = length(hitPos - prevHitPos);

    // Skip accumulating for hitCount == 0, then for subsequent hits, accumulate only when hitCount is odd
    if(hitCount > 0 && (hitCount % 2) == 0)
    {
      accumDistance += d;
    }

    // Move origin just past the hit point to continue tracing towards bottom
    float3 newOrigin = hitPos + worldDir * 0.001;
    // Update remaining distance and origin
    float newRemaining = length(bottomWorld - newOrigin);

    // Prepare for next iteration
    origin = newOrigin;
    remainingT = newRemaining;
    hitCount += 1;
    // Update previous hit position for next distance computation
    prevHitPos = hitPos;
  }

  // black: accumulated == distance between top and bottom
  // white: accumulated == 0
  float maxDist = length(bottomWorld - topWorld);
  float t = saturate((payload.accumDistance) / maxDist); // 0..1
  float gray = 1.0 - t;
  float3 finalColor = float3(gray, gray, gray);

  outImage[launchID] = float4(finalColor, 1.0);
  outVolume[launchID] = payload.accumDistance;
}

//-----------------------------------------------------------------------
// CLOSEST HIT SHADER - Called when ray hits geometry (performs shading)
//-----------------------------------------------------------------------
[shader("closesthit")]
void rchitMain(inout HitPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
  // Convert built-in barycentric coordinates to full barycentric coordinates
  // Built-in provides (u,v) where u+v+w=1, so w = 1-u-v
  float3 barycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);

  
  // Get instance and primitive information from ray tracing built-ins
  uint instanceID = InstanceIndex();  // Which instance was hit
  uint triID      = PrimitiveIndex(); // Which triangle within the mesh was hit

    GltfSceneInfo         sceneInfo = pushConst.sceneInfoAddress[0];
    GltfInstance          instance  = sceneInfo.instances[instanceID];      // Instance data
    GltfMesh              mesh      = sceneInfo.meshes[instance.meshIndex]; // Mesh data


  // Interpolate vertex attributes across the hit triangle
  int3   indices       = getTriangleIndices(mesh.gltfBuffer, mesh.triMesh, triID);
  float3 pos           = getTriangleAttribute<float3>(mesh.gltfBuffer, mesh.triMesh.positions, indices, barycentrics);
  float3 nrm           = getTriangleAttribute<float3>(mesh.gltfBuffer, mesh.triMesh.normals, indices, barycentrics);
  float3 worldNormal   = normalize(mul(WorldToObject4x3(), nrm).xyz);
  float3 N = normalize(worldNormal);  // Surface normal

    // Transform from object space to world space
  float3 worldPos      = float3(mul(float4(pos, 1.0), ObjectToWorld4x3()));
  

  float3 rd = WorldRayDirection();

  if (dot(rd, N) < 0.0f && payload.depth != 0) {
    // Opposite direction and not first hit, calculate distance
	payload.accumDistance += length(payload.hitPos - worldPos);
    
  }
  else{
    // Same direction or first hit
  }

  // Hit
  payload.hit = 1;
  payload.hitPos = worldPos;
  payload.depth++;
}

//-----------------------------------------------------------------------
// MISS SHADER - Called when ray doesn't hit any geometry (background/sky)
//-----------------------------------------------------------------------
[shader("miss")]
void rmissMain(inout HitPayload payload)
{
  // No previous hit => MISS_DEPTH
  // Any previous hit => MISS_DEPTH + 1
  payload.depth = MISS_DEPTH + (payload.depth > 0 ? 1 : 0); //  + payload.depth > 0 ? 1 : 0
}
