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
#include <glm\gtc\quaternion.hpp>

#include <map>
#include <vector>

enum ComponentType
{
  COMP_BOUNDING_BOX = 1 << 0,
  COMP_TRANSFORM    = 1 << 1,
  COMP_MESH         = 1 << 2,
  COMP_PARTICLE     = 1 << 3,
  COMP_TEXTURE      = 1 << 4,
  COMP_SMOKE        = 1 << 5,
  COMP_MATERIAL     = 1 << 6
};

const static std::map<std::string, ComponentType> componentMapping =
{
  {"boundingBox", COMP_BOUNDING_BOX},
  {"transform", COMP_TRANSFORM},
  {"mesh", COMP_MESH},
  {"particles", COMP_PARTICLE},
  {"texture", COMP_TEXTURE},
  {"smoke", COMP_SMOKE},
  {"material", COMP_MATERIAL}
};

struct ComponentInfo
{
  int size;
};

const static std::map<ComponentType, ComponentInfo> componentInfo =
{
  {COMP_BOUNDING_BOX, {2}},
  {COMP_TRANSFORM, {3}},
  {COMP_MESH, {2}},
  {COMP_PARTICLE, {2}},
  {COMP_TEXTURE, {2}},
  {COMP_SMOKE, {1}},
  {COMP_MATERIAL, {4}}
};

struct Transform
{
  glm::quat rot;
  glm::vec3 pos;
  glm::vec3 scale;

  glm::mat4 CalcTransform() const;
};

struct Geometry
{
  std::vector<glm::vec3> vertexPos_;
  std::vector<glm::vec3> vertexNormals_;
  std::vector<glm::vec2> vertexUvs_;
  std::vector<uint32_t> indices;
};

struct Mesh
{
  std::string path;
  std::string materialName;
  size_t indexOffset = 0;
  size_t indexSize = 0;
  size_t vertexOffset = 0;
};

struct Texture
{
  std::string path;
  int tilingX;
  int tilingY;
};

struct ParticleSystemData
{
  int count;
  float lifetime;
  std::string textureFile;
};

struct Smoke
{
  float density;
};

struct DirectionalLight
{
  glm::vec4 lightVector;
  glm::vec3 irradiance;
  float padding;
};