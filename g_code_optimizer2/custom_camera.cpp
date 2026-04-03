#include "custom_camera.hpp"
#include <iostream>

#include <nvgui/window.hpp>

namespace nvapp {

CustomCamera::CustomCamera()
{
  setPositionOnSphere(-defaultForward);
}

void CustomCamera::onUIRender()
{
  if(!interactiveCameraEnabled || !nvgui::isWindowHovered(ImGui::FindWindowByName("Viewport")))
    return;

  nvutils::CameraManipulator::Inputs inputs;  // Mouse and keyboard inputs

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
  glm::vec3 local_right = rotation * glm::vec3(1.0f, 0.0f, 0.0f);
  glm::vec3 local_up    = rotation * glm::vec3(0.0f, 1.0f, 0.0f);
  rotation = glm::normalize(glm::angleAxis(-direction.x, local_up) * glm::angleAxis(-direction.y, local_right) * rotation);
}

void CustomCamera::roll(float amount)
{
  glm::vec3 local_forward = rotation * glm::vec3(0.0f, 0.0f, -1.0f);
  rotation                = glm::normalize(glm::angleAxis(amount, local_forward) * rotation);
}

void CustomCamera::setPositionOnSphere(glm::vec3 position)
{
  rotation = glm::normalize(convertPositionToQuat(position));
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
  return glm::toMat4(glm::conjugate(rotation));
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
