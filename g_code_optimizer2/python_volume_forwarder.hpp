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
  float                    min_angle_difference = 0.1;


public:
  PythonVolumeForwarder()
  {
    // Mapping
    hMapFile = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(ShmData), SHM_NAME);
    if(!hMapFile)
    {
      std::string error = "CreateFileMappingA failed:" + GetLastError();
      std::cerr << error;
      throw std::runtime_error(error);
    }

    data = static_cast<ShmData*>(MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(ShmData)));
    if(!data)
    {
      std::string error = "MapViewOfFile failed: " + GetLastError();
      std::cerr << error;
      CloseHandle(hMapFile);
      throw std::runtime_error(error);
    }

    // Clear data
    Clear();
  }

  void RunStep(glm::quat quat,
               float     normalized_volume /*between 0 and 1*/,
               bool      forward_volume,
               bool      forward_position,
               float     point_size = 5,
               bool      clear      = false)
  {
    if(last_quat.has_value())
    {
      float d      = glm::dot(quat, *last_quat);
      float ad     = glm::abs(d);
      float ac     = glm::acos(glm::clamp(ad, 0.0f, 1.0f));
      float angle2 = glm::degrees(2.0f * ac);


      float angle = glm::degrees(2.0f * glm::acos(glm::abs(glm::dot(quat, *last_quat))));
      if(angle2 < min_angle_difference)
      {
        SendSettings(point_size, clear);
        // skip small difference
        std::cout << "skip\n";
        return;
      }
      else
      {
        printf("dot=%f abs=%f acos=%f angle=%f\n", d, ad, ac, angle2);
        printf("quat:      %.10f %.10f %.10f %.10f\n", quat.w, quat.x, quat.y, quat.z);
        printf("last_quat: %.10f %.10f %.10f %.10f\n", last_quat->w, last_quat->x, last_quat->y, last_quat->z);
        //std::cout << last_quat->x << " " << last_quat->y << " " << last_quat->z << "\n";
        //std::cout << quat.x << " " << quat.y << " " << quat.z << "\n";
        std::cout << "angle is: " << angle << "\n";
      }
    }

    last_quat           = quat;
    glm::vec3 cam_front = quat * glm::vec3(0.0f, 0.0f, 1.0f);
    //glm::vec3 cam_up    = glm::vec3(0, 0, 1);
    glm::vec3 cam_up = quat * glm::vec3(1.0f, 0.0f, 0.0f);

    //glm::vec3 world_up  = glm::abs(cam_front.y) < 0.99f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
    //glm::vec3 right     = glm::normalize(glm::cross(world_up, cam_front));
    //glm::vec3 cam_up    = glm::normalize(glm::cross(cam_front, right));

    //glm::vec3 cam_front = quat * glm::vec3(0.0f, 0.0f, 1.0f);

    //// Derive a roll-free up vector from forward alone
    //glm::vec3 world_up = glm::vec3(0.0f, 1.0f, 0.0f);
    //glm::vec3 right    = glm::normalize(glm::cross(world_up, cam_front));
    //glm::vec3 cam_up   = glm::normalize(glm::cross(cam_front, right));

    // Wait for buffer to be empty
    uint32_t waited = 0;
    while((data->header.write_idx - data->header.read_idx) >= RING_SIZE)
    {
      std::this_thread::sleep_for(std::chrono::microseconds(100));
      ++waited;
    }
    if(waited > 0)
      std::cout << "  [waited " << waited << " ticks for Python to consume]\n";

    std::cout << "forwarding: " << normalized_volume << "\n";

    // Forward data
    if(forward_volume)
    {

      ShmEntry& e = data->entries[data->header.write_idx % RING_SIZE];
      e.pos       = cam_front * normalized_volume;
      e.volume    = normalized_volume;
    }

    // Forward camera
    if(forward_position)
    {
      data->header.cam_front = cam_front;
      data->header.cam_up    = cam_up;
    }

    SendSettings(point_size, clear);

    if(forward_volume)
    {
      _WriteBarrier();  // entry data must be visible before write_idx advances

      // Forward write id
      data->header.write_idx++;
    }
  }

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
