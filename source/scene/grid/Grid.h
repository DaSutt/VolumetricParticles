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

#include <glm\glm.hpp>
#include "..\ParticleSystem.h"

class Grid
{
public:
  Grid();

  void SetResolution(const glm::ivec3& resolution);
  void SetBounds(const glm::vec3& min, const glm::vec3& max);

  void InsertParticleSystem(ParticleSystem* particleSystem, int index);
  void Update();

  const auto& GetResolution() const { return resolution_; }
  const auto& GetMin() const { return min_; }
  const auto& GetMax() const { return max_; }
  const auto& GetCellSize() const { return cellSize_; }

  const auto& GetParticleSystemInfo() const { return particles_; }
private:
  struct GridParticleInfo
  {
    int particleSystemId;
    glm::ivec3 min;
    glm::ivec3 max;
  };

  glm::u32vec3 resolution_;
  glm::vec3 cellSize_;

  glm::vec3 min_;
  glm::vec3 max_;

  std::vector<GridParticleInfo> particles_;
};