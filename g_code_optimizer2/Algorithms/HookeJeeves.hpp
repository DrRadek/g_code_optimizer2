#pragma once

#include "Algorithm.hpp"

class HookeJeeves
{
public:
  HookeJeeves(Algorithm& algo, float deltaStep, float tolerance, float maxSteps)
      : algo(algo)
      , deltaStep(deltaStep)
      , tolerance(tolerance)
      , maxSteps(maxSteps)
  {
    baseRotation    = algo.getCurrentRotation();
    baseVolume      = algo.getCurrentVolume();
  }

  bool optimize() { return runProcedure1(); }
  float getBestVolume() { return baseVolume; }
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
  bool runProcedure1() {
    while(true)
    {
	  // Start at base position
      currentRotation = baseRotation;
      currentVolume   = baseVolume;
      if(!algo.requestVolumeForQuat(baseRotation, false))
        return false;

      if(!Explore())
      {
        return false;
      }

      if(currentVolume < baseVolume)
      {
        if(!runProcedure2())
          return false;
      }
      else
      {
		// Update step
        if(deltaStep <= tolerance)
        {
          // Trigger calculation to prevent freeze
          if(!algo.requestVolumeForQuat(baseRotation))
            return false;
          return true;
		}

        deltaStep *= 0.5;
		std::cout << "delta step is:" << deltaStep << "\n";
      }
    }
  }

  // Loop:
  // 1) Update base position
  // 2) Make pattern move
  // 3) Make exploratory moves
  // 4) Is value better?
  //	no -> return back to procedure 1
  //	yes -> repeat
  bool runProcedure2() {
    while(true)
    {
	  // Update base position
      baseRotation = currentRotation;
      baseVolume   = currentVolume;

	  // Pattern move
      if(!PatternMove())
        return false;

	  // Explore
	  if(!Explore())
        return false;

	  if(currentVolume >= baseVolume)
      {
		// Not better than base
		// return back to procedure 1
		return true;
	  }
    }
  }

  bool PatternMove() {
    if(!algo.requestVolumeForMove(directionVector))
      return false;

	currentRotation = algo.getCurrentRotation();
	currentVolume = algo.getCurrentVolume();

	return true;
  }

  bool Explore()
  {
    explorationStepMoved = false;
    directionVector      = {0, 0};

    for(int i = 0; i < 2; i++)
    {
      if(!ExploreInAxis(i))
        return false;
	}

	return true;
  }

  // Helpers
  // returns false in case of abort from main thread
  bool ExploreInAxis(int axis)
  {
    //auto      bestVolume   = data.currentVolume;
    glm::vec2 bestDir{0, 0};
    glm::vec2 testingDir{0,0};

    // move in direction
    testingDir[axis] = deltaStep;
    if(!algo.requestVolumeForMove(testingDir))
      return false;
    if(algo.getCurrentVolume() >= currentVolume)
    {  // Result is worse or same, try other direction

      // reset
      if(!algo.requestVolumeForQuat(currentRotation, true))
        return false;

      // move in the opposite direction
      testingDir[axis] = -deltaStep;
      if(!algo.requestVolumeForMove(testingDir))
        return false;
      if(algo.getCurrentVolume() >= currentVolume)
      {  // Result is worse in both axis, not moving
        // Reset to original position
        if(!algo.requestVolumeForQuat(currentRotation, true))
          return false;
        return true;
      }
    }

    // Move to the best option
    currentVolume             = algo.getCurrentVolume();
    currentRotation      = algo.getCurrentRotation();
    directionVector[axis]     = testingDir[axis];

	return true;
  }
};
