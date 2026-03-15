#include "Algorithm.hpp"

#include <glm/gtc/constants.hpp>
#include "FibonacciPoints.hpp"

class UniformPointsAlgorithm : public Algorithm
{
  // Algo parameters
  int N = 10000;

  AlgoTask algorithmLogic() override
  {
    co_await generateFibonacciPoints(*this, N, [this](glm::vec3 point) {
      if(currentVolume < bestVolume)
      {
        bestVolume   = currentVolume;
        bestRotation = currentRotation;
      }
    });

    std::cout << "Best volume is:" << bestVolume << "\n";

    // Finish
    co_return AlgoResult(bestVolume, bestRotation);
  };

public:
  UniformPointsAlgorithm()
      : Algorithm()
  {
  }
};
