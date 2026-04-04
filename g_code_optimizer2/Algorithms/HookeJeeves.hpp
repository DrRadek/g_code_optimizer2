#pragma once

#include "Algorithm.hpp"

class HookeJeeves
{
public:
  HookeJeeves(Algorithm& algo, float deltaStep, float tolerance, int maxSteps)
      : algo(algo)
      , deltaStep(deltaStep)
      , tolerance(tolerance)
      , maxSteps(maxSteps)
  {
    baseRotation = algo.getCurrentRotation();
    baseVolume   = algo.getCurrentVolume();
  }

  AlgoTask optimize()
  {
    co_await runProcedure1();
    co_return {};
  }

  float     getBestVolume() { return baseVolume; }
  glm::quat getBestRotation() { return baseRotation; }

private:
  Algorithm& algo;

  float deltaStep;
  float tolerance;
  int   maxSteps;

  glm::quat baseRotation;
  float     baseVolume;

  glm::quat currentRotation{};
  float     currentVolume = 0;


  glm::vec2 directionVector{0, 0};

  bool explorationStepMoved = false;

  // Loop:
  // 1) Make exploratory moves
  // 2) Is value better?
  //	no -> update step size/exit
  //	yes -> run procedure 2
  AlgoTask runProcedure1();

  // Loop:
  // 1) Update base position
  // 2) Make pattern move
  // 3) Make exploratory moves
  // 4) Is value better?
  //	no -> return back to procedure 1
  //	yes -> repeat
  AlgoTask runProcedure2();

  AlgoTask PatternMove();

  AlgoTask Explore();

  // Helpers
  AlgoTask ExploreInAxis(int axis);
};
