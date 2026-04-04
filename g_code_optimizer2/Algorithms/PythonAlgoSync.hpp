#pragma once

#include "algorithm.hpp"

#include <windows.h>

class PythonAlgoSync : public Algorithm
{
  struct SharedData
  {
    // Request from python
    int       requestType;
    glm::vec3 pos;
    glm::vec2 moveDir;
    // Response from C++
    float result;
  };

  HANDLE      hMapFile{};
  SharedData* data{};
  HANDLE      sem_request{};
  HANDLE      sem_response{};

public:
  PythonAlgoSync();

  ~PythonAlgoSync()
  {
    UnmapViewOfFile(data);
    CloseHandle(hMapFile);
    CloseHandle(sem_request);
    CloseHandle(sem_response);
  }

private:
  AlgoTask algorithmLogic();
};
