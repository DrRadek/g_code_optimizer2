#include "Algorithm.hpp"

#include "HookeJeeves.hpp"

#include <glm/glm.hpp>
#include <random>
#include <cmath>

// Based on https://github.com/g-truc/glm/blob/e7970a8b26732f1b0df9690f7180546f8c30e48e/glm/gtc/random.inl#L290C5-L290C6
// But better generator
glm::vec3 randomDirection()
{
  static thread_local std::mt19937      gen(std::random_device{}());
  std::uniform_real_distribution<float> angle(0.0f, glm::two_pi<float>());
  std::uniform_real_distribution<float> distance(-1.0f, 1.0f);

  float theta = angle(gen);
  float phi   = std::acos(distance(gen));

  return glm::vec3(std::sin(phi) * std::cos(theta), std::sin(phi) * std::sin(theta), std::cos(phi));
}

// 1) generate N points
//  - for each point:
//    - optimize locally
//    - store best
// 2) Choose the best one and optimize further
class MonteCarloAlgorithm : public Algorithm
{
  // Algo parameters
  int N = 500;  // N points to generate

  // Parameters for local optimization of N points
  float KPointsDeltaStart = 0.1f;
  float KPointsDeltaEnd   = 0.03f;
  int   KPointsMaxSteps   = 100;

  // Parameters for local optimization of last point
  float LastPointDeltaStart = 0.03f;
  float LastPointDeltaEnd   = 0.00001f;
  int   LastPointMaxSteps   = 100;

  void algorithmLogic() override
  {
    std::cout << "Generate and optimize N random candidates...\n";
    for(int i = 0; i < N; ++i)
    {
      // Generate random normalized point
      glm::vec3 point = randomDirection();

      // Set position to the point
      if(!requestVolumeForQuat(point))
        return;

      // Run local optimizer
      HookeJeeves localOptimizer = HookeJeeves(*this, KPointsDeltaStart, KPointsDeltaEnd, KPointsMaxSteps);
      if(!localOptimizer.optimize())
        return;

      float volume = localOptimizer.getBestVolume();

      // Store if better
      if(volume < bestVolume)
      {
        bestVolume   = volume;
        bestRotation = localOptimizer.getBestRotation();
      }
    }

    //// Set position to the point
    std::cout << "Optimizing best candidate...\n";
    if(!requestVolumeForQuat(bestRotation, true))
      return;
    currentVolume   = bestVolume;
    currentRotation = bestRotation;

    // Optimize further
    HookeJeeves localOptimizer = HookeJeeves(*this, LastPointDeltaStart, LastPointDeltaEnd, LastPointMaxSteps);
    if(!localOptimizer.optimize())
      return;

    bestVolume   = localOptimizer.getBestVolume();
    bestRotation = localOptimizer.getBestRotation();

    std::cout << "Best volume is:" << bestVolume << "\n";

    finishAlgorithm();
  };

public:
  MonteCarloAlgorithm(SyncInfo& syncInfo, SyncData& syncData)
      : Algorithm(syncInfo, syncData)
  {
  }
};
