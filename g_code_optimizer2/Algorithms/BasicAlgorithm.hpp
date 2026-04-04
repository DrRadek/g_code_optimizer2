#pragma once
#include "Algorithm.hpp"

#include "include/json_helpers.hpp"
#include "include/app_config.hpp"

class UniformPointsAlgorithm : public Algorithm
{
  // Algo parameters
  struct Config
  {
    int N = 10000;
  };
  const Config config;

public:
  AlgoTask algorithmLogic() override;
  UniformPointsAlgorithm()
      : Algorithm()
      , config(getJsonConfig<Config>(AppConfig::instance().getAlgorithmsPath() / "basic.json"))
  {
  }
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UniformPointsAlgorithm::Config, N)
