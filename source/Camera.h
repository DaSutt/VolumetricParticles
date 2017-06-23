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

#pragma once

#include <glm\mat4x4.hpp>
#include "input\InputHandler.h"
class Camera
{
public:
  Camera();
  ~Camera();

  void UpdateView(InputHandler* input, float dt);
  void UpdateProj(int width, int height);

  const glm::mat4x4& GetView() const;
  const glm::mat4x4& GetProj() const;
  const glm::mat4x4& GetViewProj() const;

  const glm::vec3& GetForward() const { return forward_; }
  const glm::vec3& GetRight() const { return right_; }
  const glm::vec3& GetUp() const { return up_; }
  const glm::vec3& GetPosition() const { return position_; }
  const float GetAspectRatio() const { return aspectRatio_; }
  const float GetFOVHorizontal() const { return fovHorizontal_; }
	const float GetFOVVertical() const { return fovVertical_; }

  const float nearZ_ = 1.0f;
  const float farZ_ = 1000.0f;
private:
  glm::mat4x4 view_;
  glm::mat4x4 proj_;
  glm::mat4x4 viewProj_;

  glm::vec3 forward_ = glm::vec3(0.0f, 0.0f, 1.0f);
  glm::vec3 right_ = glm::vec3(1.0f, 0.0f, 0.0f);
  glm::vec3 up_ = glm::vec3(0.0f, -1.0f, 0.0f);
  glm::vec3 position_ = glm::vec3(0.0f, 0.0f, 0.0f);

  float speed_ = 10.0f;
  float aspectRatio_ = 0.0f;
  float fovHorizontal_ = 0.0f;
	float fovVertical_ = 0.0f;
};