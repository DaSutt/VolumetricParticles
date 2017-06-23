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
#include "Components.h"
#include "components\AABoundingBox.h"
#include "components\MaterialInfo.h"

#include <fstream>

namespace ResourceLoader
{
  AxisAlignedBoundingBox LoadBoundingBox(std::ifstream& file);
  Transform LoadTransform(std::ifstream& file);
  Mesh LoadMesh(const std::string& dir, std::ifstream& file, Geometry& geometry);
  ParticleSystemData LoadParticleSystem(std::ifstream& file);
  Texture LoadTexture(const std::string& dir, std::ifstream& file);
  Smoke LoadSmoke(std::ifstream& file);
  MaterialInfo LoadMaterialInfo(const std::string& dir, std::ifstream& file);
}