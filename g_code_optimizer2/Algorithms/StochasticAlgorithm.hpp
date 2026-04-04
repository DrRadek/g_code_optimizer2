#pragma once
#include "Algorithm.hpp"

#include "include/json_helpers.hpp"
#include "include/app_config.hpp"

// 1) generate N points
//  - for each point:
//    - optimize locally
//    - store best
// 2) Choose the best one and optimize further
class StochasticAlgorithm : public Algorithm
{
  // Algo parameters
  struct Config
  {
    int   N                      = 10000;  // Maximum N points to generate
    int   K                      = 100;    // end when no improvement after K points
    int   NGlobal                = 20;     // Maximum NGlobal shots
    int   KGlobal                = 5;      // end when no improvement after KGlobal shots
    float differenceFromBestFrac = 0;      // how much can generated point differ

    // Parameters for local optimization of N points
    float KPointsDeltaStart = 0.1f;
    float KPointsDeltaEnd   = 0.03f;
    int   KPointsMaxSteps   = 100;

    // Parameters for local optimization of last point
    float LastPointDeltaStart = 0.03f;
    float LastPointDeltaEnd   = 0.00001f;
    int   LastPointMaxSteps   = 100;
  };
  const Config config;

  // local variables
  int timeSinceLastBest       = 0;
  int timeSinceLastGlobalBest = 0;

public:
  AlgoTask algorithmLogic() override;
  StochasticAlgorithm()
      : Algorithm()
      , config(getJsonConfig<Config>(AppConfig::instance().getAlgorithmsPath() / "stochastic.json"))
  {
  }
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(StochasticAlgorithm::Config,
                                   N,
                                   K,
                                   NGlobal,
                                   KGlobal,
                                   differenceFromBestFrac,
                                   KPointsDeltaStart,
                                   KPointsDeltaEnd,
                                   KPointsMaxSteps,
                                   LastPointDeltaStart,
                                   LastPointDeltaEnd,
                                   LastPointMaxSteps)
