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

#include "..\Components.h"

struct AxisAlignedBoundingBox
{
public:
  AxisAlignedBoundingBox();
  AxisAlignedBoundingBox(const glm::vec3& min, const glm::vec3& max);

  glm::vec3 min;
  glm::vec3 max;
};

AxisAlignedBoundingBox ApplyTransform(const AxisAlignedBoundingBox& box, const Transform& transform);
AxisAlignedBoundingBox ApplyTranslationScale(const AxisAlignedBoundingBox& box, const glm::mat4& transform);
AxisAlignedBoundingBox Union(const AxisAlignedBoundingBox& boxA, const AxisAlignedBoundingBox& boxB);
AxisAlignedBoundingBox Clamp(const AxisAlignedBoundingBox& box, const glm::vec3& min, const glm::vec3& max);
AxisAlignedBoundingBox Floor(const AxisAlignedBoundingBox& box);

AxisAlignedBoundingBox operator-(const AxisAlignedBoundingBox& bb, const glm::vec3& vec);
AxisAlignedBoundingBox operator/(const AxisAlignedBoundingBox& bb, float value);
//world matrix for box center origin
glm::mat4 WorldMatrix(const AxisAlignedBoundingBox& box);