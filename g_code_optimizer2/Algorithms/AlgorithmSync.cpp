#include "AlgorithmSync.hpp"

#include "BasicAlgorithm.hpp"
#include "DeterministicAlgorithm.hpp"
#include "MonteCarloAlgorithm.hpp"
#include "PythonAlgoSync.hpp"

AlgoTask startAlgorithmTask(AlgorithmType algoType, std::unique_ptr<Algorithm>& algoOwner)
{
  algoOwner.reset();

  switch(algoType)
  {
    case AlgorithmType::Test:
      //return TestAlgorithm>();
      algoOwner = std::make_unique<TestAlgorithm>();
      break;
    case AlgorithmType::UniformPoints:
      algoOwner = std::make_unique<UniformPointsAlgorithm>();
      break;
    case AlgorithmType::Deterministic:
      algoOwner = std::make_unique<DeterministicAlgorithm>();
      break;
    case AlgorithmType::MonteCarlo:
      algoOwner = std::make_unique<MonteCarloAlgorithm>();
      break;
    case AlgorithmType::Python:
      algoOwner = std::make_unique<PythonAlgoSync>();
      break;
    default:
      throw std::runtime_error("Algorithm type not found");
  }

  return algoOwner->run();
}
