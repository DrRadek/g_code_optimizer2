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

#pragma once

#include "common/io_gltf.h"

NAMESPACE_SHADERIO_BEGIN()

// Binding Points (raster, rtx)
enum BindingPoints
{
  eTextures = 0,  // Binding point for textures
  eOutImage,      // Binding point for output image
  eTlas,          // Top-level acceleration structure
  eOutVolume,	  // Output volume
};

// Rtx
struct RtxPushConstant
{
  GltfSceneInfo* sceneInfoAddress;  // Address of the scene information buffer
  float3         aabbMin;           // start of bounding box for volume calculation
  float3         aabbMax;           // end of bounding box for volume calculation
};


// Raster
struct TutoPushConstant
{
  float3x3       normalMatrix;
  int            instanceIndex;              // Instance index for the current draw call
  GltfSceneInfo* sceneInfoAddress;           // Address of the scene information buffer
  float3         aabbMin;                    // start of bounding box for volume calculation
  float3         aabbMax;                    // end of bounding box for volume calculation
};


// AABB
#define AABB_SHADER_WG_SIZE 256

struct AABB
{
  float3 min;
  float3 max;
};

enum aabb_Binding
{
  Vertices    = 0,
  PartialAabb = 1,
  OutAabb     = 2
};

struct aabb_Params
{
  uint     vertCount;
  uint     groupSize;
  float4x4 projInvMatrix;
};

// Volume calculation
#define VOLUMESUM_SHADER_WG_SIZE 256

enum volumesum_Binding
{
  PartialVolume = 0,
  OutVolume     = 1
};

struct volume_Params
{
  uint arraySize;
  uint groupSize;
};

// Integration
#define VOLUME_INTEGRATE_SHADER_WG_SIZE 16

enum volume_integrate_Binding
{
  vInImage = 0,
  vOutVolume = 1
};

struct VolumeIntegratePushConstant{
  uint2 image_size;
  float2 area_size;
};

NAMESPACE_SHADERIO_END()
