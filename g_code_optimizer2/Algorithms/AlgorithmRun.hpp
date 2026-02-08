#include "Algorithm.hpp"

enum class AlgorithmType
{
  Test,
  UniformPoints,
  Deterministic
};

void runAlgorithm(SyncInfo& syncInfo, SyncData& syncData, AlgorithmType algoType);