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

#include "Scene.h"

#include <fstream>
#include <sstream>
#include <PathCch.h>

#include "ResourceLoader.h"
#include "..\utility\Status.h"

using namespace std;

Scene::Scene()
{
  uniformGrid_.SetResolution({ 128, 128, 128 });
}

Scene::~Scene()
{
}

namespace
{
  std::string RemoveFileName(const std::string& filePath)
  {
    const auto pos = filePath.find_last_of("/\\");
    return filePath.substr(0, pos + 1);
  }
}

void Scene::Load(const std::string& fileName)
{
  ifstream sceneFile(fileName);
	if (sceneFile.is_open())
  {
		Status::UpdateScene(fileName);
    std::string path = RemoveFileName(fileName);

		std::vector<int> particleGUIds;

    int guid = 0;
    string line;
    while (getline(sceneFile, line))
    {
      istringstream iss(line);
      string type;
      iss >> type;
      const auto typeMapping = componentMapping.find(type);
      if (typeMapping != componentMapping.end())
      {
        switch (typeMapping->second)
        {
        case COMP_BOUNDING_BOX:
          objectSpaceBoundingBoxes_.push_back(ResourceLoader::LoadBoundingBox(sceneFile));
          break;
        case COMP_TRANSFORM:
          transforms_.push_back(ResourceLoader::LoadTransform(sceneFile));
          break;
        case COMP_MESH:
          meshes_.push_back(ResourceLoader::LoadMesh(path, sceneFile, sceneGeometry_), guid);
          break;
        case COMP_PARTICLE:
        {
          const auto data = ResourceLoader::LoadParticleSystem(sceneFile);
					particleGUIds.push_back(guid);
					guid++;
          //particleSystems_.push_back(std::make_unique<ParticleSystem>(data.count, 20, 20.0f), guid);
        }
        break;
        case COMP_TEXTURE:
        {
          const auto texture = ResourceLoader::LoadTexture(path, sceneFile);
          textures_[texture.path] = texture;
        }break;
        case COMP_SMOKE:
          smokeVolumes_.push_back(ResourceLoader::LoadSmoke(sceneFile), guid);
          break;
        case COMP_MATERIAL:
        {
          MaterialInfo matInfo = ResourceLoader::LoadMaterialInfo(path, sceneFile);
          if (materialMappings_.find(matInfo.name_) == materialMappings_.end())
          {
            materialMappings_[matInfo.name_] = static_cast<int>(materialInfos_.size());
            materialInfos_.push_back(matInfo);
          }
        } break;
        default:
          break;
        }
      }
    }

		for (const auto guid : particleGUIds)
		{
			particleTransforms.push_back(transforms_[guid]);
		}

    worldSpaceBoundingBoxes_.resize(objectSpaceBoundingBoxes_.size());
    for (size_t i = 0; i < transforms_.size(); ++i)
    {
      worldSpaceBoundingBoxes_[i] = ApplyTransform(objectSpaceBoundingBoxes_[i], transforms_[i]);
    }
  }
  else
  {
    printf("Scene file %s not found\n", fileName.c_str());
  }

  boundingBox_ = CalcSceneBounds();

  uniformGrid_.SetBounds(
  { -60, -60, -60 },//boundingBox_.min,
  { 60, 60, 60 }//boundingBox_.max
  );
}

void Scene::Update(InputHandler* inputHandler, float deltaTime)
{
  camera_.UpdateView(inputHandler, deltaTime);
  uniformGrid_.Update();

  const auto viewProj = camera_.GetProj() * camera_.GetView();

  activeObjects_.clear();

  /*for (size_t i = 0; i < particleSystems_.data().size(); ++i)
  {
    particleSystems_.data()[i]->Update(deltaTime);
    uniformGrid_.InsertParticleSystem(particleSystems_.data()[i].get(), static_cast<int>(i));
  }*/
}

void Scene::OnResize(int width, int height)
{
  camera_.UpdateProj(width, height);
}

const std::vector<int> Scene::GetCurrMeshes()
{
  return activeObjects_;
}

AxisAlignedBoundingBox Scene::CalcSceneBounds()
{
  AxisAlignedBoundingBox boundingBox = {};
  for (size_t i = 0; i < worldSpaceBoundingBoxes_.size(); ++i)
  {
    boundingBox = Union(boundingBox, worldSpaceBoundingBoxes_[i]);
  }
  return boundingBox;
}