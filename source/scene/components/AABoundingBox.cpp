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

#include "AABoundingBox.h"

#include <glm\gtc\matrix_transform.hpp>

AxisAlignedBoundingBox::AxisAlignedBoundingBox() :
  min{0.0f},
  max{0.0f}
{}

AxisAlignedBoundingBox::AxisAlignedBoundingBox(const glm::vec3& min, const glm::vec3& max) :
  min{min},
  max{max}
{}

/*
Transforming Axis-Aligned Bounding Boxes
by Jim Arvo
from "Graphics Gems", Academic Press, 1990
*/
AxisAlignedBoundingBox ApplyTransform(const AxisAlignedBoundingBox& box, const Transform& transform)
{
  AxisAlignedBoundingBox updatedBox = { transform.pos, transform.pos };

  glm::mat3 m = glm::mat3(glm::scale(glm::mat4(), transform.scale)) * glm::inverse(glm::mat3_cast(transform.rot));

  for (int i = 0; i < 3; ++i)
  {
    for (int j = 0; j < 3; ++j)
    {
      const float a = m[i][j] * box.min[j];
      const float b = m[i][j] * box.max[j];
      if (a < b)
      {
        updatedBox.min[i] += a;
        updatedBox.max[i] += b;
      }
      else
      {
        updatedBox.min[i] += b;
        updatedBox.max[i] += a;
      }
    }
  }

  return updatedBox;
}

AxisAlignedBoundingBox ApplyTranslationScale(const AxisAlignedBoundingBox& box, const glm::mat4& transform)
{
  AxisAlignedBoundingBox updatedBox;
  updatedBox.min = transform * glm::vec4(box.min, 1.0f);
  updatedBox.max = transform * glm::vec4(box.max, 1.0f);
  return updatedBox;
}

AxisAlignedBoundingBox Union(const AxisAlignedBoundingBox& boxA, const AxisAlignedBoundingBox& boxB)
{
  return {
    glm::min(boxA.min, boxB.min),
    glm::max(boxA.max, boxB.max)
  };
}

AxisAlignedBoundingBox Clamp(const AxisAlignedBoundingBox& box, const glm::vec3& min, const glm::vec3& max)
{
  AxisAlignedBoundingBox result;
  result.min = glm::clamp(box.min, min, max);
  result.max = glm::clamp(box.max, min, max);
  return result;
}

AxisAlignedBoundingBox Floor(const AxisAlignedBoundingBox& box)
{
  AxisAlignedBoundingBox result;
  result.min = glm::floor(box.min);
  result.max = glm::floor(box.max);
  return result;
}

AxisAlignedBoundingBox operator-(const AxisAlignedBoundingBox& bb, const glm::vec3& vec)
{
  AxisAlignedBoundingBox result;
  result.min = bb.min - vec;
  result.max = bb.max - vec;
  return result;
}

AxisAlignedBoundingBox operator/(const AxisAlignedBoundingBox& bb, float value)
{
  AxisAlignedBoundingBox result;
  result.min = bb.min / value;
  result.max = bb.max / value;
  return result;
}

glm::mat4 WorldMatrix(const AxisAlignedBoundingBox& box)
{
  const auto scale = box.max - box.min;
  const auto center = box.min + scale * 0.5f;
  return glm::scale(glm::translate(glm::mat4(1.0f), center), scale);
}