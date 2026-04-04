#include "HookeJeeves.hpp"

// Loop:
// 1) Make exploratory moves
// 2) Is value better?
//	no -> update step size/exit
//	yes -> run procedure 2
AlgoTask HookeJeeves::runProcedure1()
{
  while(true)
  {
    // Start at base position
    currentRotation = baseRotation;
    currentVolume   = baseVolume;
    co_await algo.requestVolumeForQuat(baseRotation, false);

    co_await Explore();

    if(currentVolume < baseVolume)
    {
      co_await runProcedure2();
    }
    else
    {
      // Is required accuracy achieved?
      if(deltaStep <= tolerance)
      {
        // Trigger calculation to prevent freeze
        // Finish
        co_await algo.requestVolumeForQuat(baseRotation);
        co_return {};
      }

      // Update step
      deltaStep *= 0.5;
      // std::cout << "delta step is:" << deltaStep << "\n";
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
AlgoTask HookeJeeves::runProcedure2()
{
  while(true)
  {
    // Update base position
    baseRotation = currentRotation;
    baseVolume   = currentVolume;

    // Pattern move
    co_await PatternMove();

    // Explore
    co_await Explore();

    if(currentVolume >= baseVolume)
    {
      // Not better than base
      // return back to procedure 1
      break;
    }
  }
  co_return {};
}

AlgoTask HookeJeeves::PatternMove()
{
  co_await algo.requestVolumeForMove(directionVector);

  currentRotation = algo.getCurrentRotation();
  currentVolume   = algo.getCurrentVolume();

  co_return {};
}

AlgoTask HookeJeeves::Explore()
{
  explorationStepMoved = false;
  directionVector      = {0, 0};

  for(int i = 0; i < 2; i++)
  {
    co_await ExploreInAxis(i);
  }
  co_return {};
}

// Helpers
AlgoTask HookeJeeves::ExploreInAxis(int axis)
{
  //auto      bestVolume   = data.currentVolume;
  glm::vec2 bestDir{0, 0};
  glm::vec2 testingDir{0, 0};

  // move in direction
  testingDir[axis] = deltaStep;
  co_await algo.requestVolumeForMove(testingDir);
  if(algo.getCurrentVolume() >= currentVolume)
  {  // Result is worse or same, try other direction

    // reset
    co_await algo.requestVolumeForQuat(currentRotation, true);

    // move in the opposite direction
    testingDir[axis] = -deltaStep;
    co_await algo.requestVolumeForMove(testingDir);
    if(algo.getCurrentVolume() >= currentVolume)
    {  // Result is worse in both axis, not moving
      // Reset to original position
      co_await algo.requestVolumeForQuat(currentRotation, true);
      co_return {};
    }
  }

  // Move to the best option
  currentVolume         = algo.getCurrentVolume();
  currentRotation       = algo.getCurrentRotation();
  directionVector[axis] = testingDir[axis];
}
