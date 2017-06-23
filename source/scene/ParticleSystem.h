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

#include <vector>
#include <glm\glm.hpp>

#include "components\AABoundingBox.h"

class ParticleSystem
{
public:
  ParticleSystem(int particleCount, int emissionsPerSecond, float particleLifetime);

  void Update(float dt);
  void ApplyTransform(const Transform& transform);

  int GetAliveCount() const { return aliveCount_; }
  const std::vector<glm::vec3>& GetPositions() const { return positions_; }
  const auto& GetBoundingBox() const { return boundingBox_; }
  const int GetParticleCount() const { return particleCount_; }
private:
  int Spawn(int emitCount);
  void Advect(float dt);
  void Kill();

  void UpdateBounds(const glm::vec3 pos);
  void CheckBounds();

  std::vector<glm::vec3> positions_;
  std::vector<float> lifetime_;

  std::vector<int> deadParticles_;

  const int particleCount_;
  const int emissionsPerSecond_;
  const float particleLifetime_;
  const float emitRadius_ = 1.0f;
  const float particleRadius_ = 1.0f;

  int aliveCount_ = 0;
  float emissionsPerFrame_ = 0.0f;

  glm::vec3 origin_ = glm::vec3(0.0f);

  AxisAlignedBoundingBox boundingBox_;
};