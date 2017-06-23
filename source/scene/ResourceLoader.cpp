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

#include "ResourceLoader.h"

#include <sstream>
#include <rply-1.1.4\rply.h>
#include <glm\gtc\matrix_transform.hpp>
#include <glm\gtc\quaternion.hpp>

namespace ResourceLoader
{
  glm::vec3 ReadVector(std::istringstream& line)
  {
    glm::vec3 vec;
    line >> vec.x >> vec.y >> vec.z;
    return vec;
  }

  glm::quat ReadQuaternion(std::istringstream& line)
  {
    glm::quat quat;
    line >> quat.x >> quat.y >> quat.z >> quat.w;
    return quat;
  }

  AxisAlignedBoundingBox LoadBoundingBox(std::ifstream& file)
  {
    AxisAlignedBoundingBox bb;
    int compCount = 0;
    std::string line;
    while (compCount < componentInfo.at(COMP_BOUNDING_BOX).size && std::getline(file, line))
    {
      std::istringstream iss(line);
      std::string value;
      iss >> value;
      if (strcmp(value.c_str(), "min") == 0)
      {
        bb.min = ReadVector(iss);
      }
      else if (strcmp(value.c_str(), "max") == 0)
      {
        bb.max = ReadVector(iss);
      }
      else
      {
        break;
      }
      ++compCount;
    }
    return bb;
  }

  Transform LoadTransform(std::ifstream& file)
  {
    Transform transform;
    int compCount = 0;
    std::string line;
    while (compCount < componentInfo.at(COMP_TRANSFORM).size && std::getline(file, line))
    {
      std::istringstream iss(line);
      std::string value;
      iss >> value;
      if (strcmp(value.c_str(), "pos") == 0)
      {
        transform.pos = ReadVector(iss);
      }
      else if (strcmp(value.c_str(), "rotation") == 0)
      {
        transform.rot = ReadQuaternion(iss);
      }
      else if (strcmp(value.c_str(), "scaling") == 0)
      {
        transform.scale = ReadVector(iss);
      }
      else
      {
        break;
      }
      ++compCount;
    }

    return transform;
  }

  static int vector_cb(p_ply_argument argument)
  {
    long id;
    std::vector<glm::vec3>* vector;

    ply_get_argument_user_data(argument, (void**)&vector, &id);

    if (id == 0 && vector != nullptr)
    {
      vector->push_back(glm::vec3(0.0f));
    }

    switch (id)
    {
    case 0:
      vector->back().x = static_cast<float> (ply_get_argument_value(argument));
      break;
    case 1:
      vector->back().y = static_cast<float> (ply_get_argument_value(argument));
      break;
    case 2:
      vector->back().z = static_cast<float> (ply_get_argument_value(argument));
      break;
    }

    return 1;
  }

  static int uv_cb(p_ply_argument argument)
  {
    long id;
    std::vector<glm::vec2>* vector;

    ply_get_argument_user_data(argument, (void**)&vector, &id);

    if (id == 0 && vector != nullptr)
    {
      vector->push_back(glm::vec2(0.0f));
    }

    switch (id)
    {
    case 0:
      vector->back().x = static_cast<float> (ply_get_argument_value(argument));
      break;
    case 1:
      vector->back().y = static_cast<float> (ply_get_argument_value(argument));
      break;
    }

    return 1;
  }

  static int face_cb(p_ply_argument argument)
  {
    long length, value_index;
    std::vector<uint32_t>* indices;
    ply_get_argument_user_data(argument, (void**)&indices, NULL);
    ply_get_argument_property(argument, NULL, &length, &value_index);
    switch (value_index)
    {
    case 0:
    case 1:
    case 2:
      indices->push_back(static_cast<uint32_t> (ply_get_argument_value(argument)));
    default:
      break;
    }
    return 1;
  }

  void LoadGeometry(const std::string& meshPath, Geometry& geometry)
  {
    uint32_t nvertices, ntriangles;
    p_ply ply = ply_open(meshPath.c_str(), NULL, 0, NULL);
    if (!ply || !ply_read_header(ply))
    {
      return;
    }

    nvertices = ply_set_read_cb(ply, "vertex", "x", vector_cb, &geometry.vertexPos_, 0);
    ply_set_read_cb(ply, "vertex", "y", vector_cb, &geometry.vertexPos_, 1);
    ply_set_read_cb(ply, "vertex", "z", vector_cb, &geometry.vertexPos_, 2);
    ply_set_read_cb(ply, "vertex", "nx", vector_cb, &geometry.vertexNormals_, 0);
    ply_set_read_cb(ply, "vertex", "ny", vector_cb, &geometry.vertexNormals_, 1);
    ply_set_read_cb(ply, "vertex", "nz", vector_cb, &geometry.vertexNormals_, 2);
    const auto nuvs = ply_set_read_cb(ply, "vertex", "s", uv_cb, &geometry.vertexUvs_, 0);
    ply_set_read_cb(ply, "vertex", "t", uv_cb, &geometry.vertexUvs_, 1);
    ntriangles = ply_set_read_cb(ply, "face", "vertex_indices", face_cb, &geometry.indices, 0);
    if (!ply_read(ply))
    {
      return;
    }

    ply_close(ply);

    for (auto& uv : geometry.vertexUvs_)
    {
      uv.y = 1.0f - uv.y;
    }
    if (nuvs == 0)
    {
      for (int i = 0; i < static_cast<int>(nvertices); ++i)
      {
        geometry.vertexUvs_.push_back({ 0.0f, 0.0f });
      }
    }
  }

  Mesh LoadMesh(const std::string& dir, std::ifstream& file, Geometry& geometry)
  {
    Mesh mesh;
    int compCount = 0;
    std::string line;
    while (compCount < componentInfo.at(COMP_MESH).size && std::getline(file, line))
    {
      std::istringstream iss(line);
      std::string value;
      iss >> value;
      if (strcmp(value.c_str(), "path") == 0)
      {
        iss >> mesh.path;
        mesh.indexOffset = geometry.indices.size();
        mesh.vertexOffset = geometry.vertexPos_.size();
        LoadGeometry(dir + mesh.path, geometry);
        mesh.indexSize = geometry.indices.size() - mesh.indexOffset;
      }
      else if (strcmp(value.c_str(), "materialName") == 0)
      {
        iss >> mesh.materialName;
      }
      else
      {
        break;
      }
      ++compCount;
    }
    return mesh;
  }

  Texture LoadTexture(const std::string& dir, std::ifstream& file)
  {
    Texture texture;

    int compCount = 0;
    std::string line;
    while (compCount < componentInfo.at(COMP_TEXTURE).size && std::getline(file, line))
    {
      std::istringstream iss(line);
      std::string value;
      iss >> value;
      if (strcmp(value.c_str(), "filepath") == 0)
      {
        iss >> texture.path;
        texture.path = dir + texture.path;
      }
      else if (strcmp(value.c_str(), "tiling") == 0)
      {
        iss >> texture.tilingX >> texture.tilingY;
      }
      else
      {
        break;
      }
      ++compCount;
    }

    return texture;
  }

  ParticleSystemData LoadParticleSystem(std::ifstream& file)
  {
    ParticleSystemData particleSystem;
    int compCount = 0;
    std::string line;
    while (compCount < componentInfo.at(COMP_PARTICLE).size && std::getline(file, line))
    {
      std::istringstream iss(line);
      std::string value;
      iss >> value;
      if (strcmp(value.c_str(), "count") == 0)
      {
        iss >> particleSystem.count;
      }
      else if (strcmp(value.c_str(), "lifetime") == 0)
      {
        iss >> particleSystem.lifetime;
      }
      else if (strcmp(value.c_str(), "textureFile") == 0)
      {
        iss >> particleSystem.textureFile;
      }
      else
      {
        break;
      }
      ++compCount;
    }
    return particleSystem;
  }

  Smoke LoadSmoke(std::ifstream& file)
  {
    Smoke smoke;
    int compCount = 0;
    std::string line;
    while (compCount < componentInfo.at(COMP_SMOKE).size && std::getline(file, line))
    {
      std::istringstream iss(line);
      std::string value;
      iss >> value;
      if (strcmp(value.c_str(), "density") == 0)
      {
        iss >> smoke.density;
      }
      else
      {
        break;
      }
      ++compCount;
    }
    return smoke;
  }

  MaterialInfo LoadMaterialInfo(const std::string& dir, std::ifstream& file)
  {
    MaterialInfo matInfo;
    int compCount = 0;
    std::string line;
    while (compCount < componentInfo.at(COMP_MATERIAL).size && std::getline(file, line))
    {
      std::istringstream iss(line);
      std::string value;
      iss >> value;
      if (strcmp(value.c_str(), "name") == 0)
      {
        iss >> matInfo.name_;
      }
      else if (strcmp(value.c_str(), "baseColorTexture") == 0)
      {
        iss >> matInfo.baseColorTexture_;
        matInfo.baseColorTexture_ = dir + matInfo.baseColorTexture_;
      }
      else if (strcmp(value.c_str(), "roughness") == 0)
      {
        iss >> matInfo.roughness;
      }
      else if (strcmp(value.c_str(), "metalMask") == 0)
      {
        iss >> matInfo.metalMask;
      }
      else
      {
        break;
      }
      ++compCount;
    }
    return matInfo;
  }
}