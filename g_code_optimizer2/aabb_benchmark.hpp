#pragma once

//#define GLM_FORCE_ALIGNED_GENTYPES
//#define GLM_FORCE_AVX2
//#define GLM_FORCE_INTRINSICS
//#define GLM_FORCE_PURE
#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

#include <nvapp/application.hpp>

#include "aabb_compute.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class AABB_Benchmark
{
private:
  struct Result
  {
    glm::vec3 min;
    glm::vec3 max;
  } result{};


public:
  AABB_Benchmark(nvapp::Application& m_app, nvvk::ResourceAllocator& alloc, nvshaders::AABBCompute& aabbCompute)
      : aabbCompute(aabbCompute)
      , alloc(alloc)
      , m_app(m_app)
  {
    device = m_app.getDevice();
  }

  ~AABB_Benchmark() { aabbCompute.deinit(); }

  void Start();

private:
  // Tests
  Result GPU()
  {
    auto cmd = m_app.createTempCmdBuffer();
    aabbCompute.runCompute(cmd, projInvMatrix);
    m_app.submitAndWaitTempCmdBuffer(cmd);

    auto result = aabbCompute.readResult();
    return {result.min, result.max};
  }

  // Register a test: name, function, and its arguments
  void AddTest(std::string name, std::function<Result()> fn) { tests_.push_back({std::move(name), std::move(fn)}); }

  // Run all registered tests m times
  void RunTests(int m = 100);

  void SetupTests(int n = 0);

  // Tests to run
  struct Test
  {
    std::string             name;
    std::function<Result()> fn;
  };
  std::vector<Test> tests_;

  // Helpers for shader preparations
  nvshaders::AABBCompute&  aabbCompute;
  nvvk::ResourceAllocator& alloc;
  nvapp::Application&      m_app;
  VkDevice                 device;

  // Temporary
  VkCommandBuffer   cmd{};
  VkFenceCreateInfo fenceInfo{};
  VkFence           fence{};


  // Data to test on
  int                    n = 0;
  std::vector<glm::vec3> vertices{};
  std::vector<glm::vec4> vertices_vec4{};
  std::vector<glm::vec4> vertices_vec4_multiplied{};
  std::vector<float>     vertices_x_8_aligned{};
  std::vector<float>     vertices_y_8_aligned{};
  std::vector<float>     vertices_z_8_aligned{};

  glm::mat4   projInvMatrix                = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(1, 0, 0));
  glm::mat3x3 projInvMatrix_3x3            = (glm::mat3x3)projInvMatrix;
  glm::mat3x3 projInvMatrix_3x3_transposed = glm::transpose((glm::mat3x3)projInvMatrix);
};
