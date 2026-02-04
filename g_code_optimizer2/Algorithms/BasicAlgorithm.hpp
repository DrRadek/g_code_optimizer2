#include "Algorithm.hpp"

#include <glm/gtc/constants.hpp>

class UniformPointsAlgorithm : public Algorithm
{
  int N = 10;

  void algorithmLogic() override
  {

    const float goldenRatio = (1.0f + sqrtf(5.0f)) * 0.5f;
    const float goldenAngle = 2.0f * glm::pi<float>() / goldenRatio;

    float bestVolume = std::numeric_limits<float>::max();
    glm::quat bestRotation;

    for(int i = -N; i <= N; ++i)
    {
      float z     = (2.0f * i) / (2.0f * N + 1.0f);
      float phi   = asinf(z);         // latitude
      float r     = cosf(phi);        // radius in XY plane
      float theta = goldenAngle * i;  // azimuth

      float x = r * cosf(theta);
      float y = r * sinf(theta);

      std::cout << "requesting volume" << "\n";
      if(!requestVolumeForPosition({x, y, z}))
        break;

      std::cout << "Volume is:" << currentVolume << "\n";
      if(currentVolume < bestVolume)
      {
        bestVolume = currentVolume;
        bestRotation = currentRotation;
      }
    }

    std::cout << "Best volume is:" << bestVolume << "\n";
    this->bestVolume = bestVolume;
    this->bestRotation = bestRotation;

    finishAlgorithm();
  };

public:
  UniformPointsAlgorithm(SyncInfo& syncInfo, SyncData& syncData)
      : Algorithm(syncInfo, syncData)
  {
  }
};