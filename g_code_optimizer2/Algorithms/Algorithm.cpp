#include "Algorithm.hpp"

AlgoTask TestAlgorithm::algorithmLogic()
{
  for(int i = 0; i < 100; ++i)
  {
    std::cout << "requesting volume" << "\n";
    co_await requestVolumeForMove({0.1f, 0.1f});

    std::cout << "Volume is:" << currentVolume << "\n";
    if(currentVolume < bestVolume)
    {
      bestVolume   = currentVolume;
      bestRotation = currentRotation;
    }
  }

  // Finish
  co_return AlgoResult(bestVolume, bestRotation);
}
