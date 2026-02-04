#pragma once
#include "AlgorithmSync.hpp"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include <glm/gtx/quaternion.hpp>

#include "AlgorithmRun.hpp"

class AlgorithmSync
{
public:
  AlgorithmSync() = default;
  ~AlgorithmSync() {
      stopAlgorithm();
  }

  void startAlgorithm() {
    stopAlgorithm();

    syncInfo.syncState == SyncState::RendererDone;
    algorithm_thread = std::thread(runAlgorithm, std::ref(syncInfo), std::ref(syncData), AlgorithmType::UniformPoints);
    algorithmRunning = true;
  }

  void stopAlgorithm() {
    if(algorithm_thread.joinable())
    {
      {
        std::lock_guard<std::mutex> lock(syncInfo.mtx);
        syncInfo.syncState = SyncState::ShuttingDown;
      }
      syncInfo.cv.notify_one();
      algorithm_thread.join();
    }
    algorithmRunning = false;
  }

  bool isAlgorithmRunning() {
      return algorithmRunning;
  }

  // returns done/not done
  SyncData waitForAlgorithm()
  {
    std::unique_lock lock(syncInfo.mtx);
    syncInfo.cv.wait(lock, [&] { return syncInfo.syncState == SyncState::AlgorithmDone; });
    return syncData;
  }

  void notifyAlgorithm(float resultVolume, glm::quat resultRotation) {
    {
      std::lock_guard<std::mutex> lock(syncInfo.mtx);
      syncData.result    = resultVolume;
      syncData.resultRotation = resultRotation;
      syncInfo.syncState = SyncState::RendererDone;
    }
    syncInfo.cv.notify_one();
  }

private:
  // algorithm thread
  std::thread       algorithm_thread;
  bool        algorithmRunning = false;

  SyncInfo syncInfo{};
  SyncData syncData{};
};