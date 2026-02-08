#pragma once

#include <future>

#include <glm/gtx/quaternion.hpp>

#include "../shaders/shaderio.h"

enum class SyncState
{
  RendererDone,
  AlgorithmDone,
  ShuttingDown
};

struct SyncInfo
{
  // synchronization
  std::condition_variable cv;
  std::mutex              mtx;
  SyncState               syncState;
};

enum class AlgorithmState
{
  setPosition,
  setQuat,
  move,
  done
};

struct SyncData
{
  // set by both
  float          result;
  glm::quat      resultRotation;
  AlgorithmState state;

  // Set by algorithm
  shaderio::float3 newPosition;
  glm::quat        newQuat;
  shaderio::float2 moveDirection;
  bool             skipCalculation = false;
};
