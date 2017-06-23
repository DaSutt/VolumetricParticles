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

#include "Math.h"

#include <glm\gtc\packing.hpp>
#include <glm\gtc\matrix_access.hpp>

using namespace glm;

namespace Math
{
  void F16Matrix::Update(const mat4& matrix)
  {
    for (size_t i = 0; i < data.size(); ++i)
    {
      data[i] = packHalf4x16(column(matrix, static_cast<int>(i)));
    }
  }

  void F16Vector::Update(const glm::vec3& vec)
  {
    data = packHalf4x16(vec4(vec, 1.0f));
  }

  void F16Vector::Update(const glm::vec4& vec)
  {
    data = packHalf4x16(vec);
  }
}