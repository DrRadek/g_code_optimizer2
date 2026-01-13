#include "stl_utils.hpp"

#include <span>
#include <algorithm>
#include <functional>

#include <glm/gtc/type_ptr.hpp>
#include <vulkan/vulkan_core.h>
#include <fmt/format.h>

#include "nvutils/timers.hpp"
#include "nvutils/logger.hpp"
#include "nvvk/check_error.hpp"
#include "nvvk/debug_util.hpp"

#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <limits>

std::vector<openstl::Triangle> nvsamples::loadStlResources(const std::filesystem::path& path)
{
  std::ifstream readStream;
  readStream.open(path, std::ios::binary);
  if(!readStream.is_open())
  {
    std::cout << "Vstupni soubor se nepovedlo otevrit.";
    throw "Vstupni soubor se nepovedlo otevrit.";
  }

  std::vector<openstl::Triangle> triangles = openstl::deserializeStl(readStream);
  readStream.close();

  return triangles;
}

void nvsamples::SaveStlResources(const std::filesystem::path& path, const std::vector<openstl::Triangle> triangles)
{
  // Save the rotated STL
  std::ofstream writeStream;
  writeStream.open(path, std::ios::binary);
  openstl::serializeBinaryStl(triangles, writeStream);
  writeStream.close();
}

std::vector<shaderio::float3> nvsamples::exportVerticesFromStlTriangles(std::vector<openstl::Triangle> triangles)
{
  std::vector<shaderio::float3> vertices;

  const size_t   triCount  = triangles.size();
  const uint32_t vertCount = uint32_t(triCount * 3);
  vertices.reserve(triCount);

  for(size_t t = 0; t < triCount; ++t)
  {
    const auto& tri = triangles[t];
    vertices.push_back(tri.v0);
    vertices.push_back(tri.v1);
    vertices.push_back(tri.v2);
  }

  return vertices;
}

void nvsamples::importStlData(GltfSceneResource&             sceneResource,
                              std::vector<openstl::Triangle> triangles,
                              nvvk::StagingUploader&         stagingUploader,
                              bool                           importInstance /*= false*/)
{
  SCOPED_TIMER(__FUNCTION__);

  const uint32_t meshOffset = uint32_t(sceneResource.meshes.size());

  // Upload the scene resource to the GPU
  nvvk::Buffer bGltfData;
  uint32_t     bufferIndex{};
  {
    nvvk::ResourceAllocator* allocator = stagingUploader.getResourceAllocator();

    // Prepare CPU-side packed data: positions, indices, normals, colorVert, tangents, texCoords
    const size_t   triCount   = triangles.size();
    const uint32_t vertCount  = uint32_t(triCount * 3);
    const uint32_t indexCount = vertCount;

    // Buffers for each attribute
    std::vector<glm::vec3> positions;
    positions.reserve(vertCount);
    std::vector<uint32_t> indices;
    indices.reserve(indexCount);
    std::vector<glm::vec3> normals;
    normals.reserve(vertCount);
    std::vector<glm::vec4> colors;
    colors.reserve(vertCount);  // RGBA
    std::vector<glm::vec4> tangents;
    tangents.reserve(vertCount);
    std::vector<glm::vec2> texCoords;
    texCoords.reserve(vertCount);

    // extract triangle vertex positions.
    for(size_t t = 0; t < triCount; ++t)
    {
      const auto& tri = triangles[t];

      glm::vec3 p0 = tri.v0;
      glm::vec3 p1 = tri.v1;
      glm::vec3 p2 = tri.v2;

      // positions (unique per corner)
      positions.push_back(p0);
      positions.push_back(p1);
      positions.push_back(p2);

      // indices are sequential
      uint32_t base = uint32_t(t * 3);
      indices.push_back(base + 0);
      indices.push_back(base + 1);
      indices.push_back(base + 2);

      // compute flat face normal and assign to each corner (non-shared normals)
      glm::vec3 fn = glm::normalize(glm::cross(p1 - p0, p2 - p0));
      if(!glm::isnan(fn.x) && !glm::isnan(fn.y) && !glm::isnan(fn.z))
      {
        normals.push_back(fn);
        normals.push_back(fn);
        normals.push_back(fn);
      }
      else
      {
        // fallback normal
        normals.push_back(glm::vec3(0.0f, 1.0f, 0.0f));
        normals.push_back(glm::vec3(0.0f, 1.0f, 0.0f));
        normals.push_back(glm::vec3(0.0f, 1.0f, 0.0f));
      }


      colors.emplace_back(1.0f, 1.0f, 1.0f, 1.0f);
      colors.emplace_back(1.0f, 1.0f, 1.0f, 1.0f);
      colors.emplace_back(1.0f, 1.0f, 1.0f, 1.0f);

      tangents.emplace_back(1.0f, 0.0f, 0.0f, 1.0f);
      tangents.emplace_back(1.0f, 0.0f, 0.0f, 1.0f);
      tangents.emplace_back(1.0f, 0.0f, 0.0f, 1.0f);

      texCoords.emplace_back(0.0f, 0.0f);
      texCoords.emplace_back(0.0f, 0.0f);
      texCoords.emplace_back(0.0f, 0.0f);
    }

    // Pack attribute arrays into a contiguous GPU buffer
    const size_t positionsBytes = positions.size() * sizeof(glm::vec3);
    const size_t indicesBytes   = indices.size() * sizeof(uint32_t);
    const size_t normalsBytes   = normals.size() * sizeof(glm::vec3);
    const size_t colorsBytes    = colors.size() * sizeof(glm::vec4);
    const size_t tangentsBytes  = tangents.size() * sizeof(glm::vec4);
    const size_t texBytes       = texCoords.size() * sizeof(glm::vec2);

    const size_t totalBytes = positionsBytes + indicesBytes + normalsBytes + colorsBytes + tangentsBytes + texBytes;

    NVVK_CHECK(allocator->createBuffer(bGltfData, totalBytes,
                                       VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT
                                           | VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR));  // #RT

    // build CPU blob
    std::vector<unsigned char> blob;
    blob.resize(totalBytes);
    size_t offset = 0;

    auto writeToBlob = [&](const void* src, size_t sz) {
      std::memcpy(blob.data() + offset, src, sz);
      offset += sz;
    };

    // positions
    writeToBlob(positions.data(), positionsBytes);
    const size_t posOffset = 0;

    // indices
    writeToBlob(indices.data(), indicesBytes);
    const size_t idxOffset = posOffset + positionsBytes;

    // normals
    writeToBlob(normals.data(), normalsBytes);
    const size_t nrmOffset = idxOffset + indicesBytes;

    // colors
    writeToBlob(colors.data(), colorsBytes);
    const size_t colOffset = nrmOffset + normalsBytes;

    // tangents
    writeToBlob(tangents.data(), tangentsBytes);
    const size_t tanOffset = colOffset + colorsBytes;

    // texCoords
    writeToBlob(texCoords.data(), texBytes);
    const size_t texOffset = tanOffset + tangentsBytes;

    NVVK_CHECK(stagingUploader.appendBuffer(
        bGltfData, 0, std::span<const unsigned char>(reinterpret_cast<const unsigned char*>(blob.data()), blob.size())));
    bufferIndex = static_cast<uint32_t>(sceneResource.bGltfDatas.size());
    sceneResource.bGltfDatas.push_back(bGltfData);

    // Fill GltfMesh describing layout
    shaderio::GltfMesh mesh{};
    mesh.gltfBuffer = (uint8_t*)bGltfData.address;
    mesh.indexType  = VK_INDEX_TYPE_UINT32;

    // positions
    mesh.triMesh.positions.offset     = uint32_t(posOffset);
    mesh.triMesh.positions.count      = uint32_t(positions.size());
    mesh.triMesh.positions.byteStride = uint32_t(sizeof(glm::vec3));

    // indices
    mesh.triMesh.indices.offset     = uint32_t(idxOffset);
    mesh.triMesh.indices.count      = uint32_t(indices.size());
    mesh.triMesh.indices.byteStride = uint32_t(sizeof(uint32_t));

    // normals
    mesh.triMesh.normals.offset     = uint32_t(nrmOffset);
    mesh.triMesh.normals.count      = uint32_t(normals.size());
    mesh.triMesh.normals.byteStride = uint32_t(sizeof(glm::vec3));

    // colorVert (vec4)
    mesh.triMesh.colorVert.offset     = uint32_t(colOffset);
    mesh.triMesh.colorVert.count      = uint32_t(colors.size());
    mesh.triMesh.colorVert.byteStride = uint32_t(sizeof(glm::vec4));

    // tangents (vec4)
    mesh.triMesh.tangents.offset     = uint32_t(tanOffset);
    mesh.triMesh.tangents.count      = uint32_t(tangents.size());
    mesh.triMesh.tangents.byteStride = uint32_t(sizeof(glm::vec4));

    // texCoords (vec2)
    mesh.triMesh.texCoords.offset     = uint32_t(texOffset);
    mesh.triMesh.texCoords.count      = uint32_t(texCoords.size());
    mesh.triMesh.texCoords.byteStride = uint32_t(sizeof(glm::vec2));

    sceneResource.meshes.emplace_back(mesh);
    sceneResource.meshToBufferIndex.push_back(bufferIndex);
  }
}