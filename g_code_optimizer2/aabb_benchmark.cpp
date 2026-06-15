#include "aabb_benchmark.hpp"

#include <omp.h>

AABB_Benchmark::Result AABB_Benchmark::process_chunk(size_t start, size_t end)
{
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
  __m256 minXv = _mm256_set1_ps(std::numeric_limits<float>::max());
  __m256 minYv = _mm256_set1_ps(std::numeric_limits<float>::max());
  __m256 minZv = _mm256_set1_ps(std::numeric_limits<float>::max());
  __m256 maxXv = _mm256_set1_ps(std::numeric_limits<float>::lowest());
  __m256 maxYv = _mm256_set1_ps(std::numeric_limits<float>::lowest());
  __m256 maxZv = _mm256_set1_ps(std::numeric_limits<float>::lowest());

  // Calculate
  for(size_t i = start; i < end; i += 8)
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
    // extract first float and return
    return _mm_cvtss_f32(m);
  };

  auto hmax = [](__m256 v) {
    __m128 lo = _mm256_castps256_ps128(v);
    __m128 hi = _mm256_extractf128_ps(v, 1);
    __m128 m  = _mm_max_ps(lo, hi);
    m         = _mm_max_ps(m, _mm_movehl_ps(m, m));
    m         = _mm_max_ps(m, _mm_permute_ps(m, _MM_SHUFFLE(1, 1, 1, 1)));

    return _mm_cvtss_f32(m);
  };

  return Result{{hmin(minXv), hmin(minYv), hmin(minZv)}, {hmax(maxXv), hmax(maxYv), hmax(maxZv)}};
}


void AABB_Benchmark::Start()
{
  // Better waiting - reusing buffer
  AddTest("GPU", [this]() {
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

    auto result = aabbCompute.readResult();
    return Result{result.min, result.max};
  });

  AddTest("GPU_temp_buffer", [this]() {
    auto cmd = m_app.createTempCmdBuffer();
    aabbCompute.runCompute(cmd, projInvMatrix);
    m_app.submitAndWaitTempCmdBuffer(cmd);

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

  AddTest("CPU", [this]() {
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
    maxX = maxY = maxZ = std::numeric_limits<float>::lowest();


    for(size_t i = 0; i < n; ++i)
    {
      // slightly faster
      //const auto vx = vertices[i][0];
      //const auto vy = vertices[i][1];
      //const auto vz = vertices[i][2];

      // Slower (ran the tests with this one)
      const auto vx = vertices_x_8_aligned[i];
      const auto vy = vertices_y_8_aligned[i];
      const auto vz = vertices_z_8_aligned[i];

      // Slower
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

  AddTest("CPU_AVX2", [this]() {
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
    __m256 minXv = _mm256_set1_ps(std::numeric_limits<float>::max());
    __m256 minYv = _mm256_set1_ps(std::numeric_limits<float>::max());
    __m256 minZv = _mm256_set1_ps(std::numeric_limits<float>::max());
    __m256 maxXv = _mm256_set1_ps(std::numeric_limits<float>::lowest());
    __m256 maxYv = _mm256_set1_ps(std::numeric_limits<float>::lowest());
    __m256 maxZv = _mm256_set1_ps(std::numeric_limits<float>::lowest());

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
      // extract first float and return
      return _mm_cvtss_f32(m);
    };

    auto hmax = [](__m256 v) {
      __m128 lo = _mm256_castps256_ps128(v);
      __m128 hi = _mm256_extractf128_ps(v, 1);
      __m128 m  = _mm_max_ps(lo, hi);
      m         = _mm_max_ps(m, _mm_movehl_ps(m, m));
      m         = _mm_max_ps(m, _mm_permute_ps(m, _MM_SHUFFLE(1, 1, 1, 1)));

      return _mm_cvtss_f32(m);
    };

    return Result{{hmin(minXv), hmin(minYv), hmin(minZv)}, {hmax(maxXv), hmax(maxYv), hmax(maxZv)}};
  });

  for(int threads = 2; threads <= 16; threads *= 2)
  {
    AddTest("CPU_OpenMP_Thread=" + std::to_string(threads), [this, threads]() {
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
      maxX = maxY = maxZ = std::numeric_limits<float>::lowest();

#pragma omp parallel for num_threads(threads) schedule(static) reduction(min : minX, minY, minZ) reduction(max : maxX, maxY, maxZ)
      for(int i = 0; i < n; ++i)
      {
        const auto vx = vertices_x_8_aligned[i];
        const auto vy = vertices_y_8_aligned[i];
        const auto vz = vertices_z_8_aligned[i];

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

    AddTest("CPU_AVX2_OpenMP_Threads=" + std::to_string(threads), [this, threads]() {
      const size_t n = vertices_x_8_aligned.size();

      const float* vx_data = vertices_x_8_aligned.data();
      const float* vy_data = vertices_y_8_aligned.data();
      const float* vz_data = vertices_z_8_aligned.data();

      float minX, minY, minZ;
      float maxX, maxY, maxZ;

      minX = minY = minZ = std::numeric_limits<float>::max();
      maxX = maxY = maxZ = std::numeric_limits<float>::lowest();

#pragma omp parallel num_threads(threads) reduction(min : minX, minY, minZ) reduction(max : maxX, maxY, maxZ)
      {
        __m256 r00 = _mm256_set1_ps(projInvMatrix_3x3[0][0]);
        __m256 r01 = _mm256_set1_ps(projInvMatrix_3x3[0][1]);
        __m256 r02 = _mm256_set1_ps(projInvMatrix_3x3[0][2]);

        __m256 r10 = _mm256_set1_ps(projInvMatrix_3x3[1][0]);
        __m256 r11 = _mm256_set1_ps(projInvMatrix_3x3[1][1]);
        __m256 r12 = _mm256_set1_ps(projInvMatrix_3x3[1][2]);

        __m256 r20 = _mm256_set1_ps(projInvMatrix_3x3[2][0]);
        __m256 r21 = _mm256_set1_ps(projInvMatrix_3x3[2][1]);
        __m256 r22 = _mm256_set1_ps(projInvMatrix_3x3[2][2]);

        __m256 minXv = _mm256_set1_ps(std::numeric_limits<float>::max());
        __m256 minYv = _mm256_set1_ps(std::numeric_limits<float>::max());
        __m256 minZv = _mm256_set1_ps(std::numeric_limits<float>::max());

        __m256 maxXv = _mm256_set1_ps(std::numeric_limits<float>::lowest());
        __m256 maxYv = _mm256_set1_ps(std::numeric_limits<float>::lowest());
        __m256 maxZv = _mm256_set1_ps(std::numeric_limits<float>::lowest());

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

        // avx2 reduction
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

        minX = hmin(minXv);
        minY = hmin(minYv);
        minZ = hmin(minZv);

        maxX = hmax(maxXv);
        maxY = hmax(maxYv);
        maxZ = hmax(maxZv);
      }

      return Result{{minX, minY, minZ}, {maxX, maxY, maxZ}};
    });

    AddTest("CPU_AVX2_OpenMP_Improved_Threads=" + std::to_string(threads), [this, threads]() {
      float minX, minY, minZ;
      float maxX, maxY, maxZ;

      minX = minY = minZ = std::numeric_limits<float>::max();
      maxX = maxY = maxZ = std::numeric_limits<float>::lowest();

#pragma omp parallel num_threads(threads)
      {
        int tid      = omp_get_thread_num();
        int nthreads = omp_get_num_threads();

        int chunk = ((n + nthreads - 1) / nthreads + 7) & ~7;  // round up to multiple of 8

        int    start  = tid * chunk;
        int    end    = start + chunk < n ? start + chunk : n;
        Result result = process_chunk(start, end);

#pragma omp critical
        {
          minX = std::min(minX, result.min.x);
          minY = std::min(minY, result.min.y);
          minZ = std::min(minZ, result.min.z);

          maxX = std::max(maxX, result.max.x);
          maxY = std::max(maxY, result.max.y);
          maxZ = std::max(maxZ, result.max.z);
        }
      }

      return Result{{minX, minY, minZ}, {maxX, maxY, maxZ}};
    });
  }

  stats.clear();
  for(int n = 1; n <= 16'777'216; n *= 2)
  {
    SetupTests(n);

    int m = 1'000;
    // Run all functions m times
    RunTests(m);
  }

  // Save the stats
  nlohmann::json j = stats;
  std::ofstream  file("results.json");
  file << j.dump(2);
}

// Run all registered tests m times
void AABB_Benchmark::RunTests(int m)
{
  std::cout << "\n=== Benchmark Results ===\n";
  for(const auto& [name, fn] : tests_)
  {
    // Wait a bit before running next test
    Sleep(300);

    // Warm-up
    for(int i = 0; i < 100; ++i)
    {
      result = fn();
    }

    std::vector<double> times{};
    auto                t0 = std::chrono::high_resolution_clock::now();
    auto                t1 = t0;

    m = 0;

    do
    {
      auto t1_i = std::chrono::high_resolution_clock::now();
      result    = fn();
      auto t2_i = std::chrono::high_resolution_clock::now();
      times.push_back(std::chrono::duration<double, std::milli>(t2_i - t1_i).count());
      //times[i]  = std::chrono::duration<double, std::milli>(t2_i - t1_i).count();
      t1 = std::chrono::high_resolution_clock::now();
      ++m;
    } while(std::chrono::duration<double, std::milli>(t1 - t0).count() <= 2000 && m < 40000);  // 2 seconds or 40000 runs

    auto   total_time = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double avg_time   = total_time / double(m);
    std::sort(times.begin(), times.end());
    double median = times[m / 2];

    std::cout << "  [" << name << "]\n"
              << "    runs  : " << m << "\n"
              << "    n     : " << n << "\n"
              << "    total : " << total_time << " ms\n"
              << "    avg   : " << avg_time << " ms/call\n"
              << "    median: " << median << " ms/call\n"
              << "    min   : (" << result.min.x << ", " << result.min.y << ", " << result.min.z << ")\n"
              << "    max   : (" << result.max.x << ", " << result.max.y << ", " << result.max.z << ")\n"
              << "\n";

    stats.push_back(Stats{name, n, m, avg_time, median, total_time});
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
  static bool first_init = true;
  if(!first_init)
  {
    aabbCompute.deinit();
  }
  else
  {
    first_init = false;
  }
  auto cmd2 = m_app.createTempCmdBuffer();
  aabbCompute.init(cmd2, &alloc, vertices);
  m_app.submitAndWaitTempCmdBuffer(cmd2);
  aabbCompute.cleanupAfterInit();

  // Vulkan Setup
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
