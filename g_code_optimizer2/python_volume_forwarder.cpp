#include "python_volume_forwarder.hpp"

PythonVolumeForwarder::PythonVolumeForwarder()
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

void PythonVolumeForwarder::RunStep(glm::quat quat, float normalized_volume, bool forward_volume, bool forward_position, float point_size, bool clear)
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
      std::cout << "angle is: " << angle << "\n";
    }
  }

  // Note:
  //  It would be better to use camera object here.
  //  Possibility of breaking when forward vector changes.
  last_quat           = quat;
  glm::vec3 cam_front = quat * glm::vec3(0.0f, 0.0f, 1.0f);
  glm::vec3 cam_up    = quat * glm::vec3(1.0f, 0.0f, 0.0f);

  // Wait for buffer to have free space
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
