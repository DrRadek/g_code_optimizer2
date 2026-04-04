#include "DeterministicAlgorithm.hpp"

#include <glm/gtc/constants.hpp>
#include "HookeJeeves.hpp"
#include "FibonacciPoints.hpp"

#include <fstream>

AlgoTask DeterministicAlgorithm::algorithmLogic()
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
}
