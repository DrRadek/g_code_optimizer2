#pragma once

#include <iostream>
#include <windows.h>
#include <thread>
#include <chrono>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

class PythonVolumeForwarder
{
  static constexpr uint32_t    RING_SIZE          = 32;
  static constexpr const char* SHM_NAME           = "g_code_optimizer2_volume_forward";
  static constexpr float       DEFAULT_POINT_SIZE = 5;

  struct ShmHeader
  {
    uint32_t  write_idx;  // Set by C++
    uint32_t  read_idx;   // Set by Python
    glm::vec3 cam_front;  // Set by C++
    glm::vec3 cam_up;     // Set by C++
    float     point_size;
    uint32_t  clear;
  };

  struct ShmEntry
  {
    glm::vec3 pos;     // normalized
    float     volume;  // normalized, between 0 and 1
  };

  struct ShmData
  {
    ShmHeader header;
    ShmEntry  entries[RING_SIZE];
  };

  HANDLE   hMapFile{};
  ShmData* data{};

  bool initializedWithError = false;

  std::optional<glm::quat> last_quat;
  float                    min_angle_difference = 0.1f;


public:
  PythonVolumeForwarder();

  void RunStep(glm::quat quat,
               float     normalized_volume /*between 0 and 1*/,
               bool      forward_volume,
               bool      forward_position,
               float     point_size = 5,
               bool      clear      = false);

  void SendSettings(float point_size, bool clear)
  {
    data->header.point_size = point_size;
    if(clear && data->header.clear == 0)
    {
      data->header.clear = 1;
    }
  }

  void Clear()
  {
    std::memset(data, 0, sizeof(ShmData));
    data->header.point_size = DEFAULT_POINT_SIZE;
  }

  ~PythonVolumeForwarder()
  {
    UnmapViewOfFile(data);
    CloseHandle(hMapFile);
  }
};
