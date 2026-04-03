#pragma once
#include "Algorithm.hpp"

#include <glm/gtc/constants.hpp>
#include "HookeJeeves.hpp"
#include "FibonacciPoints.hpp"

#include <queue>

#include "include/json_helpers.hpp"
#include <nlohmann/json.hpp>
#include <fstream>


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

  AlgoTask algorithmLogic() override
  {
    // Step: 1 find best K candidates
    std::cout << "Finding best K candidates...\n";
    co_await generateFibonacciPoints(*this, config.N, [this](glm::vec3 point) {
      if(bestKPoints.size() < config.K)
      {
        bestKPoints.push({currentVolume, currentRotation});
      }
      else if(currentVolume < bestKPoints.top().volume)
      {
        bestKPoints.pop();
        bestKPoints.push({currentVolume, currentRotation});
      }
    });

    // Optimize best k
    std::cout << "Optimizing best K candidates...\n";
    while(!bestKPoints.empty())
    {
      PointWithInfo point = bestKPoints.top();
      bestKPoints.pop();

      // Set position to the point
      co_await requestVolumeForQuat(point.rotation, true);

      currentVolume   = point.volume;
      currentRotation = point.rotation;

      // Run local optimizer
      HookeJeeves localOptimizer = HookeJeeves(*this, config.KPointsDeltaStart, config.KPointsDeltaEnd, config.KPointsMaxSteps);
      co_await localOptimizer.optimize();

      float volume = localOptimizer.getBestVolume();

      // Store if better
      if(volume < bestPoint.volume)
      {
        bestPoint = {volume, localOptimizer.getBestRotation()};
      }
    }

    //// Set position to the point
    std::cout << "Optimizing best candidate...\n";
    co_await requestVolumeForQuat(bestPoint.rotation, true);
    currentVolume   = bestPoint.volume;
    currentRotation = bestPoint.rotation;

    // Optimize further
    HookeJeeves localOptimizer = HookeJeeves(*this, config.LastPointDeltaStart, config.LastPointDeltaEnd, config.LastPointMaxSteps);
    co_await localOptimizer.optimize();

    bestVolume   = localOptimizer.getBestVolume();
    bestRotation = localOptimizer.getBestRotation();

    std::cout << "Best volume is:" << bestVolume << "\n";

    // Finish
    co_return AlgoResult(bestVolume, bestRotation);
  };

public:
  DeterministicAlgorithm()
      : Algorithm()
      , config(getJsonConfig<Config>(AppConfig::instance().getAlgorithmsPath() / "deterministic.json"))
  {
  }
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DeterministicAlgorithm::Config, N, K, KPointsDeltaStart, KPointsDeltaEnd, KPointsMaxSteps, LastPointDeltaStart, LastPointDeltaEnd, LastPointMaxSteps)
