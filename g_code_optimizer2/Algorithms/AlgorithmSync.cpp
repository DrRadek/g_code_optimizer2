#include "AlgorithmSync.hpp"

#include "BasicAlgorithm.hpp"
#include "DeterministicAlgorithm.hpp"
#include "StochasticAlgorithm.hpp"
#include "PythonAlgoSync.hpp"

AlgoTask startAlgorithmTask(AlgorithmType algoType, std::unique_ptr<Algorithm>& algoOwner)
{
  algoOwner.reset();

  switch(algoType)
  {
    case AlgorithmType::Test:
      algoOwner = std::make_unique<TestAlgorithm>();
      break;
    case AlgorithmType::UniformPoints:
      algoOwner = std::make_unique<UniformPointsAlgorithm>();
      break;
    case AlgorithmType::Deterministic:
      algoOwner = std::make_unique<DeterministicAlgorithm>();
      break;
    case AlgorithmType::Stochastic:
      algoOwner = std::make_unique<StochasticAlgorithm>();
      break;
    case AlgorithmType::Python:
      algoOwner = std::make_unique<PythonAlgoSync>();
      break;
    default:
      throw std::runtime_error("Algorithm type not found");
  }

  return algoOwner->run();
}

AlgoRequestAny AlgorithmSync::startAlgorithm(AlgorithmType algoType, unsigned int maxEvals)
{
  stopAlgorithm();
  this->maxEvals = maxEvals;
  task           = startAlgorithmTask(algoType, algorithm);

  auto& h = task->h;
  auto& p = h.promise();

  h.resume();
  algorithmRunning = true;
  forceDone        = false;
  iterationCount   = 0;

  return p.algo_request.value();
}

void AlgorithmSync::stopAlgorithm()
{
  if(!isAlgorithmRunning())
    return;

  task.reset();
  algorithm.reset();
  algorithmRunning = false;

  std::cout << "Total algorithm iterations: " << iterationCount << "\n";
}

AlgoRequestAny AlgorithmSync::runAlgorithm(RendererResult result)
{
  if(maxEvals > 0 && iterationCount >= maxEvals)
  {
    forceDone = true;
    return AlgoRequestAny{};
  }

  ++iterationCount;

  auto& h = task->h;
  auto& p = h.promise();

  p.renderer_result = result;
  p.algo_request.reset();
  p.active.resume();

  if(isAlgorithmDone())
    return AlgoRequestAny{};

  return p.algo_request.value();
}
