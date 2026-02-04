#pragma once
#include <string>
#include <memory>

#include "SyncInfo.hpp"

#include <iostream>
#include <glm/gtx/quaternion.hpp>

class Algorithm
{
public:
  void run() {
      algorithmLogic();
  }

private:
  bool waitForRenderer() {
    std::unique_lock lock(syncInfo.mtx);
    syncInfo.cv.wait(lock, [&] { return syncInfo.syncState == SyncState::RendererDone || syncInfo.syncState == SyncState::ShuttingDown; });

    currentVolume = syncData.result;
    currentRotation = syncData.resultRotation;

    return syncInfo.syncState != SyncState::ShuttingDown;
  }

  void notifyRendererDone()
  {
    {
      std::lock_guard<std::mutex> lock(syncInfo.mtx);
      syncData.result = bestVolume;
      syncData.resultRotation = bestRotation;
      syncData.state = AlgorithmState::done;
      syncInfo.syncState = SyncState::AlgorithmDone;
    }
    syncInfo.cv.notify_one();
  }

  void notifyRendererNewPos(shaderio::float3 newPosition)
  {
    {
      std::lock_guard<std::mutex> lock(syncInfo.mtx);
      syncData.newPosition   = newPosition;

      syncData.state = AlgorithmState::setPosition;
      syncInfo.syncState = SyncState::AlgorithmDone;
    }
    syncInfo.cv.notify_one();
  }

  void notifyRendererMoveDir(shaderio::float2 moveDirection)
  {
    {
      std::lock_guard<std::mutex> lock(syncInfo.mtx);
      syncData.moveDirection = moveDirection;

      syncData.state = AlgorithmState::move;
      syncInfo.syncState = SyncState::AlgorithmDone;
    }
    syncInfo.cv.notify_one();
  }

protected:
  SyncInfo& syncInfo;
  SyncData& syncData;
  bool      shutting_down = false;

  float currentVolume = 0;
  glm::quat currentRotation;

  float bestVolume = std::numeric_limits<float>::max();
  glm::quat bestRotation;

  Algorithm(SyncInfo& syncInfo, SyncData& syncData)
      : syncInfo(syncInfo)
      , syncData(syncData)
  {

  }

  // Request volume and wait for result
  bool requestVolumeForPosition(shaderio::float3 newPosition)
  {
      notifyRendererNewPos(newPosition);

      return waitForRenderer();
  }

  bool requestVolumeForMove(shaderio::float2 move)
  {
    notifyRendererMoveDir(move);

    return waitForRenderer();
  }

  void finishAlgorithm()
  {
    notifyRendererDone();
  }

  // Loop
  virtual void algorithmLogic() = 0;
};

class TestAlgorithm : public Algorithm
{
  void algorithmLogic() override {
    while(true)
    {
      std::cout << "requesting volume" << "\n";
      if(!requestVolumeForMove({0.1f, 0.1f}))
        break;

      std::cout << "Volume is:" << currentVolume << "\n";
    }
  };

public:
  TestAlgorithm(SyncInfo& syncInfo, SyncData& syncData)
      : Algorithm(syncInfo, syncData) {
  }
};
