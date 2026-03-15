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
#include "Algorithm.hpp"

#include <coroutine>
#include <iostream>
#include <cassert>
#include <variant>

template <class... Ts>
struct overloaded : Ts...
{
  using Ts::operator()...;
};

enum class AlgorithmType
{
  None,
  Test,
  UniformPoints,
  Deterministic,
  MonteCarlo,
  Python
};

AlgoTask startAlgorithmTask(AlgorithmType algoType, std::unique_ptr<Algorithm>& algoOwner);

class AlgorithmSync
{
public:
  AlgorithmSync() = default;
  ~AlgorithmSync() { stopAlgorithm(); }

  AlgoRequestAny startAlgorithm(AlgorithmType algoType)
  {
    stopAlgorithm();
    task = startAlgorithmTask(algoType, algorithm);

    auto& h = task->h;
    auto& p = h.promise();

    h.resume();
    algorithmRunning = true;

    return p.algo_request.value();
  }

  void stopAlgorithm()
  {
    task.reset();
    algorithm.reset();
    algorithmRunning = false;
  }

  bool       isAlgorithmRunning() { return algorithmRunning; }
  bool       isAlgorithmDone() { return task->h.done(); }
  AlgoResult getAlgorithmResult() { return task->h.promise().algo_result; }

  AlgoRequestAny runAlgorithm(RendererResult result)
  {
    auto& h = task->h;
    auto& p = h.promise();

    p.renderer_result = result;
    p.algo_request.reset();
    p.active.resume();

    if(isAlgorithmDone())
      return AlgoRequestAny{};

    return p.algo_request.value();
  }

private:
  bool                       algorithmRunning = false;
  std::optional<AlgoTask>    task;
  std::unique_ptr<Algorithm> algorithm;
};
