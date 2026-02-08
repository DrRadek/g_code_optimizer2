#include "Algorithm.hpp"

#include <glm/gtc/constants.hpp>
#include "FibonacciPoints.hpp"

class UniformPointsAlgorithm : public Algorithm
{
  // Algo parameters
  int N = 10;

  void algorithmLogic() override
  {
    generateFibonacciPoints(*this, N, [this](glm::vec3 point) {
      std::cout << "requesting volume" << "\n";
      if(!requestVolumeForPosition(point))
        return false;

      std::cout << "Volume is:" << currentVolume << "\n";
      if(currentVolume < bestVolume)
      {
        bestVolume   = currentVolume;
        bestRotation = currentRotation;
      }

      return true;
    });

    std::cout << "Best volume is:" << bestVolume << "\n";

    finishAlgorithm();
  };

public:
  UniformPointsAlgorithm(SyncInfo& syncInfo, SyncData& syncData)
      : Algorithm(syncInfo, syncData)
  {
  }
};
