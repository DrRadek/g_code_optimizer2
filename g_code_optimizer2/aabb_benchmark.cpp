#include "aabb_benchmark.hpp"

#include <omp.h>

void SequentialCpuOptimized() {}

void AABB_Benchmark::Start()
{
  // Add tests
  //AddTest("GPU", [this]() {
  //  auto cmd = m_app.createTempCmdBuffer();
  //  aabbCompute.runCompute(cmd, projInvMatrix);
  //  m_app.submitAndWaitTempCmdBuffer(cmd);

  //  auto result = aabbCompute.readResult();
  //  return Result{result.min, result.max};
  //});

  AddTest("GPU better", [this]() {
    //auto cmd = m_app.createTempCmdBuffer();
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);
    aabbCompute.runCompute(cmd, projInvMatrix);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;

    vkQueueSubmit(m_app.getQueue(0).queue, 1, &submitInfo, fence);
    vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &fence);
    //m_app.submitAndWaitTempCmdBuffer(cmd);

    auto result = aabbCompute.readResult();
    return Result{result.min, result.max};
  });

  AddTest("CPU_naive", [this]() {
    // Init min
    Result result;
    result.min = vertices[0];
    result.max = result.min;

    for(size_t i = 1; i < n; ++i)
    {
      const auto& multiplied = vertices[i] * projInvMatrix_3x3;

      result.max[0] = std::max(result.max[0], multiplied[0]);
      result.min[0] = std::min(result.min[0], multiplied[0]);
      result.max[1] = std::max(result.max[1], multiplied[1]);
      result.min[1] = std::min(result.min[1], multiplied[1]);
      result.max[2] = std::max(result.max[2], multiplied[2]);
      result.min[2] = std::min(result.min[2], multiplied[2]);
    }
    return result;
  });

  AddTest("CPU_memory_friendly", [this]() {
    const auto r00 = projInvMatrix_3x3[0][0];
    const auto r01 = projInvMatrix_3x3[0][1];
    const auto r02 = projInvMatrix_3x3[0][2];

    const auto r10 = projInvMatrix_3x3[1][0];
    const auto r11 = projInvMatrix_3x3[1][1];
    const auto r12 = projInvMatrix_3x3[1][2];

    const auto r20 = projInvMatrix_3x3[2][0];
    const auto r21 = projInvMatrix_3x3[2][1];
    const auto r22 = projInvMatrix_3x3[2][2];

    float minX, minY, minZ;
    minX = minY = minZ = std::numeric_limits<float>::max();
    float maxX, maxY, maxZ;
    maxX = maxY = maxZ = std::numeric_limits<float>::min();


    for(size_t i = 0; i < n; ++i)
    {
      const auto vx = vertices_x_8_aligned[i];
      const auto vy = vertices_y_8_aligned[i];
      const auto vz = vertices_z_8_aligned[i];

      //const auto rx = r02 * vz + r01 * vy + r00 * vx;
      //const auto ry = r12 * vz + r11 * vy + r10 * vx;
      //const auto rz = r22 * vz + r21 * vy + r20 * vx;

      float rx = std::fma(r02, vz, std::fma(r01, vy, r00 * vx));
      float ry = std::fma(r12, vz, std::fma(r11, vy, r10 * vx));
      float rz = std::fma(r22, vz, std::fma(r21, vy, r20 * vx));

      minX = std::min(minX, rx);
      maxX = std::max(maxX, rx);
      minY = std::min(minY, ry);
      maxY = std::max(maxY, ry);
      minZ = std::min(minZ, rz);
      maxZ = std::max(maxZ, rz);
    }

    return Result{{minX, minY, minZ}, {maxX, maxY, maxZ}};
  });

  AddTest("CPU_memory_friendly_openmp", [this]() {
    const auto r00 = projInvMatrix_3x3[0][0];
    const auto r01 = projInvMatrix_3x3[0][1];
    const auto r02 = projInvMatrix_3x3[0][2];

    const auto r10 = projInvMatrix_3x3[1][0];
    const auto r11 = projInvMatrix_3x3[1][1];
    const auto r12 = projInvMatrix_3x3[1][2];

    const auto r20 = projInvMatrix_3x3[2][0];
    const auto r21 = projInvMatrix_3x3[2][1];
    const auto r22 = projInvMatrix_3x3[2][2];

    //float minXGlobal, minYGlobal, minZGlobal;
    //minXGlobal = minYGlobal = minZGlobal = std::numeric_limits<float>::max();
    //float maxXGlobal, maxYGlobal, maxZGlobal;
    //maxXGlobal = maxYGlobal = maxZGlobal = std::numeric_limits<float>::min();

    //#pragma omp parallel

    float minX, minY, minZ;
    minX = minY = minZ = std::numeric_limits<float>::max();
    float maxX, maxY, maxZ;
    maxX = maxY = maxZ = std::numeric_limits<float>::min();

//#pragma omp for nowait
#pragma omp parallel for num_threads(8) schedule(static) reduction(min : minX, minY, minZ) reduction(max : maxX, maxY, maxZ)
    for(int i = 0; i < n; ++i)
    {
      //printf("thread %d / %d\n", omp_get_thread_num(), omp_get_num_threads());

      const auto vx = vertices_x_8_aligned[i];
      const auto vy = vertices_y_8_aligned[i];
      const auto vz = vertices_z_8_aligned[i];

      //const auto rx = r02 * vz + r01 * vy + r00 * vx;
      //const auto ry = r12 * vz + r11 * vy + r10 * vx;
      //const auto rz = r22 * vz + r21 * vy + r20 * vx;

      const float rx = r02 * vz + r01 * vy + r00 * vx;
      const float ry = r12 * vz + r11 * vy + r10 * vx;
      const float rz = r22 * vz + r21 * vy + r20 * vx;

      if(rx < minX)
        minX = rx;
      if(rx > maxX)
        maxX = rx;

      if(ry < minY)
        minY = ry;
      if(ry > maxY)
        maxY = ry;

      if(rz < minZ)
        minZ = rz;
      if(rz > maxZ)
        maxZ = rz;

      //float rx = std::fma(r02, vz, std::fma(r01, vy, r00 * vx));
      //float ry = std::fma(r12, vz, std::fma(r11, vy, r10 * vx));
      //float rz = std::fma(r22, vz, std::fma(r21, vy, r20 * vx));

      //minX = std::min(minX, rx);
      //maxX = std::max(maxX, rx);
      //minY = std::min(minY, ry);
      //maxY = std::max(maxY, ry);
      //minZ = std::min(minZ, rz);
      //maxZ = std::max(maxZ, rz);
    }

    return Result{{minX, minY, minZ}, {maxX, maxY, maxZ}};
  });

  AddTest("CPU AVX2", [this]() {
    // 1) projInvMatrix_3x3[0][0] becomes an array of  projInvMatrix_3x3[0][0] * 8
    __m256 r00 = _mm256_set1_ps(projInvMatrix_3x3[0][0]);
    __m256 r01 = _mm256_set1_ps(projInvMatrix_3x3[0][1]);
    __m256 r02 = _mm256_set1_ps(projInvMatrix_3x3[0][2]);

    __m256 r10 = _mm256_set1_ps(projInvMatrix_3x3[1][0]);
    __m256 r11 = _mm256_set1_ps(projInvMatrix_3x3[1][1]);
    __m256 r12 = _mm256_set1_ps(projInvMatrix_3x3[1][2]);

    __m256 r20 = _mm256_set1_ps(projInvMatrix_3x3[2][0]);
    __m256 r21 = _mm256_set1_ps(projInvMatrix_3x3[2][1]);
    __m256 r22 = _mm256_set1_ps(projInvMatrix_3x3[2][2]);

    // 2) Init min/max values
    __m256 minXv = _mm256_set1_ps(+FLT_MAX);
    __m256 minYv = _mm256_set1_ps(+FLT_MAX);
    __m256 minZv = _mm256_set1_ps(+FLT_MAX);
    __m256 maxXv = _mm256_set1_ps(-FLT_MAX);
    __m256 maxYv = _mm256_set1_ps(-FLT_MAX);
    __m256 maxZv = _mm256_set1_ps(-FLT_MAX);

    // Calculate
    for(size_t i = 0; i < vertices_x_8_aligned.size(); i += 8)
    {
      // load 8 values from the X,Y,Z vectors
      __m256 vx = _mm256_load_ps(&vertices_x_8_aligned[i]);
      __m256 vy = _mm256_load_ps(&vertices_y_8_aligned[i]);
      __m256 vz = _mm256_load_ps(&vertices_z_8_aligned[i]);

      // " Fused multiply + add"
      // r02 * vz + r01 * vy + r00 * vx
      __m256 rx = _mm256_fmadd_ps(r02, vz, _mm256_fmadd_ps(r01, vy, _mm256_mul_ps(r00, vx)));

      __m256 ry = _mm256_fmadd_ps(r12, vz, _mm256_fmadd_ps(r11, vy, _mm256_mul_ps(r10, vx)));

      __m256 rz = _mm256_fmadd_ps(r22, vz, _mm256_fmadd_ps(r21, vy, _mm256_mul_ps(r20, vx)));

      // store min/max
      minXv = _mm256_min_ps(minXv, rx);
      maxXv = _mm256_max_ps(maxXv, rx);
      minYv = _mm256_min_ps(minYv, ry);
      maxYv = _mm256_max_ps(maxYv, ry);
      minZv = _mm256_min_ps(minZv, rz);
      maxZv = _mm256_max_ps(maxZv, rz);
    }

    // Horizontal min/max reducers
    auto hmin = [](__m256 v) {
      __m128 lo = _mm256_castps256_ps128(v);    // v[0...3]
      __m128 hi = _mm256_extractf128_ps(v, 1);  // v[4...7]
      __m128 m  = _mm_min_ps(lo, hi);           // m[0..3] = min(v[0..3],v[4..7])

      // https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_movehl_ps&ig_expand=4591
      // _mm_movehl_ps(m,m)... temp[0..3] = [m[2],m[3],m[2],m[3]] ... moves high half to low half
      // m[0..1] = [min(m[0],temp[0]), min(m[1],temp[1])] = [min(m[0],m[2]), min(m[1],m[3])]
      m = _mm_min_ps(m, _mm_movehl_ps(m, m));
      //m         = _mm_min_ps(m, _mm_shuffle_ps(m, m, 0x55));

      // _MM_SHUFFLE(1, 1, 1, 1) => temp[0..3] = m[1,1,1,1]
      // m = min(m[0..3],temp[0..3]) = min(m[0..3],m[1,1,1,1])
      m = _mm_min_ps(m, _mm_permute_ps(m, _MM_SHUFFLE(1, 1, 1, 1)));
      // Cast to float and return
      return _mm_cvtss_f32(m);
    };

    auto hmax = [](__m256 v) {
      __m128 lo = _mm256_castps256_ps128(v);
      __m128 hi = _mm256_extractf128_ps(v, 1);
      __m128 m  = _mm_max_ps(lo, hi);
      m         = _mm_max_ps(m, _mm_movehl_ps(m, m));
      //m         = _mm_max_ps(m, _mm_shuffle_ps(m, m, 0x55));
      m = _mm_max_ps(m, _mm_permute_ps(m, _MM_SHUFFLE(1, 1, 1, 1)));

      return _mm_cvtss_f32(m);
    };

    return Result{{hmin(minXv), hmin(minYv), hmin(minZv)}, {hmax(maxXv), hmax(maxYv), hmax(maxZv)}};
  });

  AddTest("CPU AVX2 OpenMP", [this]() {
    const size_t n = vertices_x_8_aligned.size();

    const float* vx_data = vertices_x_8_aligned.data();
    const float* vy_data = vertices_y_8_aligned.data();
    const float* vz_data = vertices_z_8_aligned.data();

    const __m256 r00 = _mm256_set1_ps(projInvMatrix_3x3[0][0]);
    const __m256 r01 = _mm256_set1_ps(projInvMatrix_3x3[0][1]);
    const __m256 r02 = _mm256_set1_ps(projInvMatrix_3x3[0][2]);

    const __m256 r10 = _mm256_set1_ps(projInvMatrix_3x3[1][0]);
    const __m256 r11 = _mm256_set1_ps(projInvMatrix_3x3[1][1]);
    const __m256 r12 = _mm256_set1_ps(projInvMatrix_3x3[1][2]);

    const __m256 r20 = _mm256_set1_ps(projInvMatrix_3x3[2][0]);
    const __m256 r21 = _mm256_set1_ps(projInvMatrix_3x3[2][1]);
    const __m256 r22 = _mm256_set1_ps(projInvMatrix_3x3[2][2]);

    float minX, minY, minZ;
    float maxX, maxY, maxZ;

    minX = minY = minZ = FLT_MAX;
    maxX = maxY = maxZ = -FLT_MAX;

#pragma omp parallel num_threads(8)
    {
      __m256 minXv = _mm256_set1_ps(FLT_MAX);
      __m256 minYv = _mm256_set1_ps(FLT_MAX);
      __m256 minZv = _mm256_set1_ps(FLT_MAX);

      __m256 maxXv = _mm256_set1_ps(-FLT_MAX);
      __m256 maxYv = _mm256_set1_ps(-FLT_MAX);
      __m256 maxZv = _mm256_set1_ps(-FLT_MAX);

#pragma omp for schedule(static)
      for(int i = 0; i < (int)n; i += 8)
      {
        __m256 vx = _mm256_load_ps(&vx_data[i]);
        __m256 vy = _mm256_load_ps(&vy_data[i]);
        __m256 vz = _mm256_load_ps(&vz_data[i]);

        __m256 rx = _mm256_fmadd_ps(r02, vz, _mm256_fmadd_ps(r01, vy, _mm256_mul_ps(r00, vx)));

        __m256 ry = _mm256_fmadd_ps(r12, vz, _mm256_fmadd_ps(r11, vy, _mm256_mul_ps(r10, vx)));

        __m256 rz = _mm256_fmadd_ps(r22, vz, _mm256_fmadd_ps(r21, vy, _mm256_mul_ps(r20, vx)));

        minXv = _mm256_min_ps(minXv, rx);
        maxXv = _mm256_max_ps(maxXv, rx);

        minYv = _mm256_min_ps(minYv, ry);
        maxYv = _mm256_max_ps(maxYv, ry);

        minZv = _mm256_min_ps(minZv, rz);
        maxZv = _mm256_max_ps(maxZv, rz);
      }

      // ---- thread-local reduction to scalars ----
      auto hmin = [](__m256 v) {
        __m128 lo = _mm256_castps256_ps128(v);
        __m128 hi = _mm256_extractf128_ps(v, 1);
        __m128 m  = _mm_min_ps(lo, hi);
        m         = _mm_min_ps(m, _mm_movehl_ps(m, m));
        m         = _mm_min_ps(m, _mm_shuffle_ps(m, m, 0x55));
        return _mm_cvtss_f32(m);
      };

      auto hmax = [](__m256 v) {
        __m128 lo = _mm256_castps256_ps128(v);
        __m128 hi = _mm256_extractf128_ps(v, 1);
        __m128 m  = _mm_max_ps(lo, hi);
        m         = _mm_max_ps(m, _mm_movehl_ps(m, m));
        m         = _mm_max_ps(m, _mm_shuffle_ps(m, m, 0x55));
        return _mm_cvtss_f32(m);
      };

      float localMinX = hmin(minXv);
      float localMinY = hmin(minYv);
      float localMinZ = hmin(minZv);

      float localMaxX = hmax(maxXv);
      float localMaxY = hmax(maxYv);
      float localMaxZ = hmax(maxZv);

#pragma omp critical
      {
        minX = std::min(minX, localMinX);
        minY = std::min(minY, localMinY);
        minZ = std::min(minZ, localMinZ);

        maxX = std::max(maxX, localMaxX);
        maxY = std::max(maxY, localMaxY);
        maxZ = std::max(maxZ, localMaxZ);
      }
    }

    return Result{{minX, minY, minZ}, {maxX, maxY, maxZ}};
  });

  // Create data of length n
  for(int i = 13; i < 14; ++i)
  {
    int current_n = 10000 * (i + 1);
    //SetupTests(std::pow(10, i));
    SetupTests(current_n);  // std::pow(10, i)


    // Run all functions m times
    RunTests(10000);
  }
}

// Run all registered tests m times
void AABB_Benchmark::RunTests(int m)
{
  std::cout << "\n=== Benchmark Results ===\n";
  for(const auto& [name, fn] : tests_)
  {
    auto t0 = std::chrono::steady_clock::now();
    for(int i = 0; i < m; ++i)
      result = fn();
    auto t1 = std::chrono::steady_clock::now();

    auto   total_time = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    double avg_time   = static_cast<double>(total_time) / m;

    std::cout << "  [" << name << "]\n"
              << "    runs  : " << m << "\n"
              << "    n     : " << n << "\n"
              << "    total : " << total_time << " ms\n"
              << "    avg   : " << avg_time << " ms/call\n"
              << "    min   : (" << result.min.x << ", " << result.min.y << ", " << result.min.z << ")\n"
              << "    max   : (" << result.max.x << ", " << result.max.y << ", " << result.max.z << ")\n"
              << "\n";
  }
}

void AABB_Benchmark::SetupTests(int n)
{
  // Fill in some data to test on
  this->n            = n;
  int padding        = 8 - n % 8;
  int aligned_8_size = n + padding;
  vertices_x_8_aligned.resize(aligned_8_size);
  vertices_y_8_aligned.resize(aligned_8_size);
  vertices_z_8_aligned.resize(aligned_8_size);
  vertices.resize(n);
  vertices_vec4.resize(n);

  for(int i = 0; i < n; ++i)
  {
    int val                 = i / n;
    vertices[i]             = {i / (float)n, (i + 1) / (float)n, (i + 2) / (float)n};
    vertices_vec4[i]        = {i / (float)n, (i + 1) / (float)n, (i + 2) / (float)n, 0};
    vertices_x_8_aligned[i] = vertices[i][0];
    vertices_y_8_aligned[i] = vertices[i][1];
    vertices_z_8_aligned[i] = vertices[i][2];
  }

  // Add padding
  for(int i = n; i < aligned_8_size; ++i)
  {
    vertices_x_8_aligned[i] = vertices_x_8_aligned[i - 1];
    vertices_y_8_aligned[i] = vertices_y_8_aligned[i - 1];
    vertices_z_8_aligned[i] = vertices_z_8_aligned[i - 1];
  }


  vertices_vec4_multiplied.resize(n);
  std::cout << "    min   : (" << vertices[0].x << ", " << vertices[0].y << ", " << vertices[0].z << ")\n"
            << "    max   : (" << vertices[n - 1].x << ", " << vertices[n - 1].y << ", " << vertices[n - 1].z << ")\n";

  // Re-create AABB shader
  aabbCompute.deinit();
  auto cmd2 = m_app.createTempCmdBuffer();
  aabbCompute.init(cmd2, &alloc, vertices);
  m_app.submitAndWaitTempCmdBuffer(cmd2);
  aabbCompute.cleanupAfterInit(&alloc);

  // --- Setup (once) ---
  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.queueFamilyIndex = m_app.getQueue(0).familyIndex;
  poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  VkCommandPool pool;
  vkCreateCommandPool(device, &poolInfo, nullptr, &pool);

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool        = pool;
  allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;

  vkAllocateCommandBuffers(device, &allocInfo, &cmd);


  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  vkCreateFence(device, &fenceInfo, nullptr, &fence);
}
