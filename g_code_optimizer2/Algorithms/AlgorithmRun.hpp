#include "Algorithm.hpp"
#include "BasicAlgorithm.hpp"

enum class AlgorithmType
{
  Test,
  UniformPoints
};

void runAlgorithm(SyncInfo& syncInfo, SyncData& syncData, AlgorithmType algoType);