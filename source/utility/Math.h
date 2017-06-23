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
#include <array>

namespace Math
{
  class F16Matrix
  {
  public:
    void Update(const glm::mat4& matrix);

  private:
    std::array<glm::uint64, 4> data;
  };

  class F16Vector
  {
  public:
    void Update(const glm::vec3& vec);
    void Update(const glm::vec4& vec);
  private:
    glm::uint64 data;
  };

	inline int Index3Dto1D(int x, int y, int z, int resolution)
	{
		return (z * resolution + y) * resolution + x;
	}

	inline glm::ivec3 Index1Dto3D(int index, int resolution)
	{
		glm::ivec3 pos;
		const auto res2 = resolution * resolution;
		pos.z = index / res2;
		pos.y = static_cast<int>(index / resolution) % resolution;
		pos.x = index % resolution;
		return pos;
	}
}