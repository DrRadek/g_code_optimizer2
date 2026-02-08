#include "Algorithm.hpp"

#include <glm/gtc/constants.hpp>
#include "HookeJeeves.hpp"
#include "FibonacciPoints.hpp"

#include <queue>

// 1) find best K candidates
// 2) optimize best K
// 3) Choose the best one and optimize further
class DeterministicAlgorithm : public Algorithm
{
  // Algo parameters
  int N = 2000;  // N points to generate
  int K = 60;  // K points to choose

  // Parameters for local optimization of K points
  float KPointsDeltaStart = 0.1f;
  float KPointsDeltaEnd   = 0.03f;
  int KPointsMaxSteps   = 100;

  // Parameters for local optimization of last point
  float LastPointDeltaStart = 0.03f;
  float LastPointDeltaEnd   = 0.00001f;
  int   LastPointMaxSteps   = 100;

  //

  struct PointWithInfo
  {
    float     volume;
    glm::quat rotation;

    bool operator<(const PointWithInfo& other) const { return volume < other.volume; }
    bool operator>(const PointWithInfo& other) const { return volume > other.volume; }
  };

  std::priority_queue<PointWithInfo> bestKPoints;
  PointWithInfo                      bestPoint{std::numeric_limits<float>::max(), glm::quat(1, 0, 0, 0)};

  void algorithmLogic() override
  {
    bool endAlgorithm = false;

	// Step: 1 find best K candidates
    std::cout << "Finding best K candidates...\n";
    generateFibonacciPoints(*this, N, [this, &endAlgorithm](glm::vec3 point) {
      if(!requestVolumeForPosition(point))
      {
        endAlgorithm = true;
		return false;
	  }

      // Save if lowest than currently highest value
      if(bestKPoints.size() < K)
      {
        bestKPoints.push({currentVolume, currentRotation});
      }
      else if(currentVolume < bestKPoints.top().volume)
      {
        bestKPoints.pop();
        bestKPoints.push({currentVolume, currentRotation});
      }

      return true;
    });

	if(endAlgorithm)
      return;

	// Optimize best k
    std::cout << "Optimizing best K candidates...\n";
    while(!bestKPoints.empty())
    {
      PointWithInfo point = bestKPoints.top();
      bestKPoints.pop();

	  // Set position to the point
      if(!requestVolumeForQuat(point.rotation, true))
        return;
      currentVolume = point.volume;
      currentRotation = point.rotation;

	  // Run local optimizer
      HookeJeeves localOptimizer = HookeJeeves(*this, KPointsDeltaStart, KPointsDeltaEnd, KPointsMaxSteps);
      if(!localOptimizer.optimize())
        return;

	  float volume = localOptimizer.getBestVolume();

	  // Store if better
      if(volume < bestPoint.volume)
      {
        bestPoint    = {volume, localOptimizer.getBestRotation()};
	  }
    }

	//// Set position to the point
    std::cout << "Optimizing best candidate...\n";
    if(!requestVolumeForQuat(bestPoint.rotation, true))
      return;
    currentVolume   = bestPoint.volume;
    currentRotation = bestPoint.rotation;

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
  DeterministicAlgorithm(SyncInfo& syncInfo, SyncData& syncData)
      : Algorithm(syncInfo, syncData)
  {
  }
};
