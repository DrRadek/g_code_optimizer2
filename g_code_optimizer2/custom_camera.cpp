#include "custom_camera.hpp"
#include <iostream>
namespace nvapp {

CustomCamera::CustomCamera()
{
  setPositionOnSphere(-defaultForward);
}

void CustomCamera::onUIRender()
{


  nvutils::CameraManipulator::Inputs inputs;  // Mouse and keyboard inputs

  if(!interactiveCameraEnabled)
    return;

  float wheelScrollAmount = ImGui::GetIO().MouseWheel;
  if(wheelScrollAmount != 0.0f)
  {
    roll(wheelScrollAmount * 0.1f);
  }

  bool      lmb       = ImGui::IsMouseDown(ImGuiMouseButton_Left);
  ImVec2    mousePos2 = ImGui::GetMousePos();
  glm::vec2 mousePos  = {mousePos2.x, mousePos2.y};

  if(!lmb)
  {
    dragging = false;
    return;
  }

  if(!dragging)
  {
    dragging     = true;
    lastMousePos = mousePos;
    return;
  }

  glm::vec2 mouseDelta = (mousePos - lastMousePos) * stepSize;
  move(mouseDelta);
  lastMousePos = mousePos;
}

void CustomCamera::move(glm::vec2 direction)
{
  rotation = glm::angleAxis(direction.x, glm::vec3(0, 1, 0)) * glm::angleAxis(direction.y, glm::vec3(1, 0, 0)) * rotation;
}

void CustomCamera::roll(float amount)
{
  rotation = glm::angleAxis(amount, glm::vec3(0, 0, 1)) * rotation;
}

void CustomCamera::setPositionOnSphere(glm::vec3 position)
{
  rotation = convertPositionToQuat(position);
}

glm::quat CustomCamera::convertPositionToQuat(glm::vec3 position)
{
  glm::vec3 pos     = glm::normalize(position);
  glm::vec3 forward = -pos;

  return glm::rotation(defaultForward, forward);
}

glm::quat CustomCamera::getQuatNoRoll(glm::quat quat)
{
  glm::vec3 forward = glm::normalize(quat * defaultForward);
  return glm::rotation(defaultForward, forward);
}

glm::mat4x4 CustomCamera::getViewMatrix()
{
  return glm::toMat4(rotation);
}

glm::mat4x4 CustomCamera::getViewMatrixNoRoll()
{
  auto rotationNew = glm::angleAxis(getRoll(), glm::vec3(0, 0, 1)) * rotation;
  return glm::toMat4(rotationNew);
}

float CustomCamera::getRoll()
{
  glm::quat quatNoRoll = getQuatNoRoll(rotation);
  glm::quat diff       = rotation * glm::conjugate(quatNoRoll);
  float     angle      = glm::angle(diff);

  if(dot(glm::axis(diff), {0, 0, 1}) > 0)
  {
    return -angle;
  }
  else
  {
    return angle;
  }
}

}  // namespace nvapp
