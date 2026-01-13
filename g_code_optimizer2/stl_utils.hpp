#pragma once

#include "common/gltf_utils.hpp"
#include "stl.h"

namespace nvsamples {
// This is a utility function to load an STL file and return the model data.
std::vector<openstl::Triangle> loadStlResources(const std::filesystem::path& path);
void SaveStlResources(const std::filesystem::path& path, const std::vector<openstl::Triangle> triangles);

// This is a utility function to import the STL data into the scene resource.
void importStlData(GltfSceneResource&             sceneResource,
                   std::vector<openstl::Triangle> triangles,
                   nvvk::StagingUploader&         stagingUploader,
                   bool                           importInstance = false);

std::vector<shaderio::float3> exportVerticesFromStlTriangles(std::vector<openstl::Triangle> triangles);
}  // namespace nvsamples