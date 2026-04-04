#pragma once
#include "Algorithm.hpp"

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
  Stochastic,
  Python
};

AlgoTask startAlgorithmTask(AlgorithmType algoType, std::unique_ptr<Algorithm>& algoOwner);

class AlgorithmSync
{
public:
  AlgorithmSync() = default;
  ~AlgorithmSync() { stopAlgorithm(); }

  AlgoRequestAny startAlgorithm(AlgorithmType algoType);

  void stopAlgorithm();

  bool       isAlgorithmRunning() { return algorithmRunning; }
  bool       isAlgorithmDone() { return task->h.done(); }
  AlgoResult getAlgorithmResult() { return task->h.promise().algo_result; }

  AlgoRequestAny runAlgorithm(RendererResult result);

private:
  bool                       algorithmRunning = false;
  std::optional<AlgoTask>    task;
  std::unique_ptr<Algorithm> algorithm;

  int iterationCount = 0;
};
