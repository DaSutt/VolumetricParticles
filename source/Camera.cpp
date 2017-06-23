/*
MIT License

Copyright(c) 2017 Daniel Suttor

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "Camera.h"

#include <glm\gtc\matrix_transform.hpp>
#include <glm\gtc\quaternion.hpp>

#include "utility\Status.h"

using namespace glm;

Camera::Camera()
{
  view_ = mat4(1);
}

Camera::~Camera()
{}

void Camera::UpdateView(InputHandler* input, float dt)
{
  quat rotatition_ = {};
  if (input->MouseDown(VirtualKey::MS_RB))
  {
    const auto mouseDelta = input->MouseDeltaAxis();
    float yaw = radians(0.25f * mouseDelta.x);
    float pitch = radians(0.25f * mouseDelta.y);

    const auto yawRot = angleAxis(-yaw, up_);
    const auto pitchRot = angleAxis(-pitch, right_);

    rotatition_ = normalize(pitchRot * yawRot);
    forward_ = normalize(rotatition_ * forward_);
    right_ = normalize(rotatition_ * right_);
  }

  vec3 movement;
  if (input->KeyDown(VirtualKey::KB_D)) // right
    movement.x += 1.0f;
  if (input->KeyDown(VirtualKey::KB_A)) // left
    movement.x -= 1.0f;
  if (input->KeyDown(VirtualKey::KB_Q)) // up
    movement.y -= 1.0f;
  if (input->KeyDown(VirtualKey::KB_E)) // down
    movement.y += 1.0f;
  if (input->KeyDown(VirtualKey::KB_W)) // front
    movement.z += 1.0f;
  if (input->KeyDown(VirtualKey::KB_S)) // back
    movement.z -= 1.0f;

  const auto movementLength = length(movement);
  if (movementLength > 0.0f)
    movement /= movementLength;

  if (movement.x != 0.0f)
    position_ += movement.x * speed_ * right_ * dt;
  if (movement.y != 0.0f)
    position_ += movement.y * speed_ * up_ * dt;
  if (movement.z != 0.0f)
    position_ += movement.z * speed_ * forward_ * dt;

  view_ = lookAt(position_, position_ + forward_, up_);

  viewProj_ = proj_ * view_;

	Status::UpdateCameraState(this);
}

void Camera::UpdateProj(int width, int height)
{
  const glm::mat4 clip(1.0f, 0.0f, 0.0f, 0.0f,
    +0.0f, -1.0f, 0.0f, 0.0f,
    +0.0f, 0.0f, 0.5f, 0.0f,
    +0.0f, 0.0f, 0.5f, 1.0f);

	fovHorizontal_ = glm::quarter_pi<float>();
	aspectRatio_ = static_cast<float>(height) / static_cast<float>(width);
  proj_ = clip * perspectiveFov<float>(fovHorizontal_, static_cast<float>(width), static_cast<float>(height), nearZ_, farZ_);
	
	fovVertical_ = 2 * std::atan(std::tan(fovHorizontal_ * 0.5f) * aspectRatio_) * glm::pi<float>();
}

const glm::mat4x4& Camera::GetView() const
{
  return view_;
}

const glm::mat4x4& Camera::GetProj() const
{
  return proj_;
}

const glm::mat4x4& Camera::GetViewProj() const
{
  return viewProj_;
}