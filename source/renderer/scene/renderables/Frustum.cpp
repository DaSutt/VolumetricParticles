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

#include "Frustum.h"

#include <glm\gtc\matrix_access.hpp>

namespace Renderer
{
  constexpr int matrixSize = 4;

  void Frustum::ExtractPlanes(const glm::mat4& matrix)
  {
    std::array<glm::vec4, matrixSize> rows;
    for (int i = 0; i < matrixSize; ++i)
    {
      rows[i] = glm::row(matrix, i);
    }

    planes_[PLANE_LEFT] = rows[3] + rows[0];
    planes_[PLANE_RIGHT] = rows[3] - rows[0];
    planes_[PLANE_BOTTOM] = rows[3] + rows[1];
    planes_[PLANE_TOP] = rows[3] - rows[1];
    planes_[PLANE_NEAR] = rows[3] + rows[2];
    planes_[PLANE_FAR] = rows[3] - rows[2];

    for (auto& p : planes_)
    {
      Normalize(p);
    }
  }

  inline void Frustum::Normalize(glm::vec4& input) const
  {
    float length = glm::length(glm::vec3(input));
    input /= length;
  }
}