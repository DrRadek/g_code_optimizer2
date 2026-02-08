#include "FibonacciPoints.hpp"

void generateFibonacciPoints(Algorithm& algo, int N, std::function<bool(glm::vec3)> callback)
{
  const float goldenRatio = (1.0f + sqrtf(5.0f)) * 0.5f;
  const float goldenAngle = 2.0f * glm::pi<float>() / goldenRatio;

  for(int i = -N; i <= N; ++i)
  {
    float z     = (2.0f * i) / (2.0f * N + 1.0f);
    float phi   = asinf(z);
    float r     = cosf(phi);
    float theta = goldenAngle * i;

    float x = r * cosf(theta);
    float y = r * sinf(theta);

    if(!callback({x, y, z}))
      break;
  }
}
