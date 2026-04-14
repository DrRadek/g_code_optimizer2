#include "StochasticAlgorithm.hpp"

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

  return glm::normalize(glm::vec3(std::sin(phi) * std::cos(theta), std::sin(phi) * std::sin(theta), std::cos(phi)));
}

AlgoTask StochasticAlgorithm::algorithmLogic()
{
  glm::quat globalBestRotation{};
  float     globalBestbestVolume = std::numeric_limits<float>::max();

  for(int iGlobal = 0; iGlobal < config.NGlobal; ++iGlobal)
  {
    timeSinceLastBest = 0;
    bestVolume        = std::numeric_limits<float>::max();
    // std::cout << "Generate and optimize N random candidates...\n";
    for(int i = 0; i < config.N; ++i)
    {
      // Generate random normalized point
      glm::vec3 point = randomDirection();

      // Set position to the point
      co_await requestVolumeForPosition(point);

      if(currentVolume < bestVolume * (1 + config.differenceFromBestFrac))
      {
        // Run local optimizer
        HookeJeeves localOptimizer = HookeJeeves(*this, config.KPointsDeltaStart, config.KPointsDeltaEnd, config.KPointsMaxSteps);
        co_await localOptimizer.optimize();

        float volume = localOptimizer.getBestVolume();

        // Store if better
        if(volume < bestVolume)
        {
          bestVolume        = volume;
          bestRotation      = localOptimizer.getBestRotation();
          timeSinceLastBest = 0;
          continue;
        }
      }
      else
      {
        // Skip random search of more points
        break;
      }

      ++timeSinceLastBest;
      if(timeSinceLastBest > config.K)
      {
        break;
      }
    }

    // Set position to the point
    // std::cout << "Optimizing best candidate...\n";
    co_await requestVolumeForQuat(bestRotation, true);
    currentVolume   = bestVolume;
    currentRotation = bestRotation;

    if(globalBestbestVolume > currentVolume)
    {
      globalBestbestVolume    = currentVolume;
      globalBestRotation      = currentRotation;
      timeSinceLastGlobalBest = 0;
      continue;
    }

    ++timeSinceLastGlobalBest;
    if(timeSinceLastGlobalBest > config.KGlobal)
    {
      break;
    }
  }

  co_await requestVolumeForQuat(globalBestRotation, true);

  // Optimize further
  HookeJeeves localOptimizer = HookeJeeves(*this, config.LastPointDeltaStart, config.LastPointDeltaEnd, config.LastPointMaxSteps);
  co_await localOptimizer.optimize();

  bestVolume   = localOptimizer.getBestVolume();
  bestRotation = localOptimizer.getBestRotation();

  std::cout << "Best volume is:" << bestVolume << "\n";

  // Finish
  co_return AlgoResult(bestVolume, bestRotation);
}
