#pragma once
#include "Algorithm.hpp"

#include <glm/gtc/constants.hpp>
#include "FibonacciPoints.hpp"

#include "include/json_helpers.hpp"
#include <nlohmann/json.hpp>

class UniformPointsAlgorithm : public Algorithm
{
  // Algo parameters
  struct Config
  {
    int N = 10000;
  };
  const Config config;

  AlgoTask algorithmLogic() override
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
  };

public:
  UniformPointsAlgorithm()
      : Algorithm()
      , config(getJsonConfig<Config>(AppConfig::instance().getAlgorithmsPath() + "\\basic.json"))
  {
  }
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UniformPointsAlgorithm::Config, N)
