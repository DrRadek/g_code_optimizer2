#include "Algorithm.hpp"

enum class AlgorithmType
{
  Test,
  UniformPoints,
  Deterministic,
  MonteCarlo
};

void runAlgorithm(SyncInfo& syncInfo, SyncData& syncData, AlgorithmType algoType);
