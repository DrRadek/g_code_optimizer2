#include "PythonAlgoSync.hpp"

PythonAlgoSync::PythonAlgoSync()
{
  // Map shared memory
  hMapFile = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(SharedData), "g_code_optimizer2_shm");
  if(hMapFile == nullptr)
  {
    std::cerr << "CreateFileMappingA failed: " << GetLastError() << std::endl;
    return;
  }

  // Map the view of the file into the address space of the process
  data = (SharedData*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));
  if(data == nullptr)
  {
    std::cerr << "MapViewOfFile failed: " << GetLastError() << std::endl;
    CloseHandle(hMapFile);
    return;
  }

  // Create request semaphore
  sem_request = CreateSemaphoreA(nullptr, 0, 1, "sem_request");
  if(sem_request == nullptr)
  {
    std::cerr << "CreateSemaphoreA (request) failed: " << GetLastError() << std::endl;
    return;
  }

  // Create response semaphore
  sem_response = CreateSemaphoreA(nullptr, 0, 1, "sem_response");
  if(sem_response == nullptr)
  {
    std::cerr << "CreateSemaphoreA (response) failed: " << GetLastError() << std::endl;
    CloseHandle(sem_request);
    return;
  }

  std::cout << "C++ worker ready..." << std::endl;
}

inline AlgoTask PythonAlgoSync::algorithmLogic()
{
  while(true)
  {
    // Wait for request
    if(WaitForSingleObject(sem_request, 16) == WAIT_TIMEOUT)
    {
      // Give control to the main thread when waiting for too long
      co_await requestVolumeForMove({0, 0});
    }

    // Read shared memory and write to shared memory
    bool requestedVolume = false;
    switch(data->requestType)
    {
      case 1:  // Pos
        co_await requestVolumeForPosition(data->pos);
        requestedVolume = true;
        break;
      case 2:  // Move
        co_await requestVolumeForMove(data->moveDir);
        requestedVolume = true;
        break;
      case 3:
        // TODO: save result
        co_return {};
      default:
        break;
    }

    // Reset request
    data->requestType = 0;

    // Let python know about the result
    if(requestedVolume)
    {
      data->result = currentVolume;
      ReleaseSemaphore(sem_response, 1, nullptr);
    }
  }
}
