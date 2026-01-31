
#pragma once

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <nvutils/camera_manipulator.hpp>

#include <nvapp/application.hpp> 
#include <glm/gtx/quaternion.hpp>

namespace nvapp {

class CustomCamera : public nvapp::IAppElement
{
  // Manual mouse movement
  float stepSize = 0.01f;
  bool  dragging = false;
  glm::vec2 lastMousePos;

  // Camera info
  glm::quat rotation;
  glm::vec3 defaultForward = {0, 0, -1};

public:
  CustomCamera();

  void onUIRender() override;
  void move(glm::vec2 direction);
  void roll(float amount);
  void setPositionOnSphere(glm::vec3 position);

  glm::quat convertPositionToQuat(glm::vec3 position);
  glm::quat getQuatNoRoll(glm::quat quat);

  glm::mat4x4 getViewMatrix();
  glm::mat4x4 getViewMatrixNoRoll();
  float getRoll();
};

}  // namespace nvapp
