
#include "algorithmRun.hpp"

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
    default:
      break;
  }

  algo->run();
}
