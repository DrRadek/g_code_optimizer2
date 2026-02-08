
#include "algorithmRun.hpp"
#include "BasicAlgorithm.hpp"
#include "DeterministicAlgorithm.hpp"
#include "MonteCarloAlgorithm.hpp"

void runAlgorithm(SyncInfo& syncInfo, SyncData& syncData, AlgorithmType algoType)
{
  std::unique_ptr<Algorithm> algo;

  switch(algoType)
  {
    case AlgorithmType::Test:
      algo = std::make_unique<TestAlgorithm>(syncInfo, syncData);
      break;
    case AlgorithmType::UniformPoints:
      algo = std::make_unique<UniformPointsAlgorithm>(syncInfo, syncData);
      break;
    case AlgorithmType::Deterministic:
      algo = std::make_unique<DeterministicAlgorithm>(syncInfo, syncData);
      break;
    case AlgorithmType::MonteCarlo:
      algo = std::make_unique<MonteCarloAlgorithm>(syncInfo, syncData);
      break;
    default:
      return;
  }

  algo->run();
}
