#include "BasicAlgorithm.hpp"

#include <glm/gtc/constants.hpp>
#include "FibonacciPoints.hpp"

AlgoTask UniformPointsAlgorithm::algorithmLogic()
{
  co_await generateFibonacciPoints(*this, config.N, [this](glm::vec3 point) {
    if(currentVolume < bestVolume)
    {
      bestVolume   = currentVolume;
      bestRotation = currentRotation;
    }
  });

  std::cout << "Best volume is:" << bestVolume << "\n";

  // Finish
  co_return AlgoResult(bestVolume, bestRotation);
}
