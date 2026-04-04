#pragma once
#include "Algorithm.hpp"

#include <queue>

#include "include/json_helpers.hpp"
#include "include/app_config.hpp"


// 1) find best K candidates
// 2) optimize best K
// 3) Choose the best one and optimize further
class DeterministicAlgorithm : public Algorithm
{
  struct Config
  {
    // Algo parameters
    int N = 2000;  // N points to generate
    int K = 10;    // K points to choose

    // Parameters for local optimization of K points
    float KPointsDeltaStart = 0.1f;
    float KPointsDeltaEnd   = 0.03f;
    int   KPointsMaxSteps   = 100;

    // Parameters for local optimization of last point
    float LastPointDeltaStart = 0.03f;
    float LastPointDeltaEnd   = 0.00001f;
    int   LastPointMaxSteps   = 100;
  };
  const Config config;

  struct PointWithInfo
  {
    float     volume;
    glm::quat rotation;

    bool operator<(const PointWithInfo& other) const { return volume < other.volume; }
    bool operator>(const PointWithInfo& other) const { return volume > other.volume; }
  };

  std::priority_queue<PointWithInfo> bestKPoints;
  PointWithInfo                      bestPoint{std::numeric_limits<float>::max(), glm::quat(1, 0, 0, 0)};

public:
  AlgoTask algorithmLogic() override;
  DeterministicAlgorithm()
      : Algorithm()
      , config(getJsonConfig<Config>(AppConfig::instance().getAlgorithmsPath() / "deterministic.json"))
  {
  }
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DeterministicAlgorithm::Config, N, K, KPointsDeltaStart, KPointsDeltaEnd, KPointsMaxSteps, LastPointDeltaStart, LastPointDeltaEnd, LastPointMaxSteps)
