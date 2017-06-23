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

#include <string>
#include <vector>

#include "components\AABoundingBox.h"
#include "components\MaterialInfo.h"
#include "Components.h"
#include "ComponentVector.h"
#include "..\Camera.h"
#include "grid\Grid.h"

#undef min
#undef max

class InputHandler;

class Scene
{
public:
  Scene();
  ~Scene();

  void Load(const std::string& fileName);
  void Update(InputHandler* inputHandler, float deltaTime);

  void OnResize(int width, int height);

  const std::vector<int> GetCurrMeshes();

  const auto& GetMeshes()const { return meshes_; }
  const auto& GetBoundingBoxes()const { return worldSpaceBoundingBoxes_; }
  const auto& GetGeometry()const { return sceneGeometry_; }
  const auto& GetTextures() const { return textures_; }
  const auto& GetSmokeVolumes() const { return smokeVolumes_; }

  const auto& GetCamera()const { return camera_; }
	
  const auto& GetTransforms() const { return transforms_; }
  const auto& GetMaterialInfos() const { return materialInfos_; }
  const auto& GetMaterialMappings() const { return materialMappings_; }
	const auto& GetParticleTransforms() const { return particleTransforms; }

  Grid* GetGrid() { return &uniformGrid_; }

  const auto& GetSceneBoundingBox() const { return boundingBox_; }
private:
  AxisAlignedBoundingBox CalcSceneBounds();

  std::vector<AxisAlignedBoundingBox> objectSpaceBoundingBoxes_;
  std::vector<AxisAlignedBoundingBox> worldSpaceBoundingBoxes_;
  std::vector<Transform> transforms_;
  std::vector<int> activeObjects_;
  ComponentVector<Mesh> meshes_;
  ComponentVector<Smoke> smokeVolumes_;
  std::map<std::string, Texture> textures_;
  Geometry sceneGeometry_;

  Grid uniformGrid_;

  AxisAlignedBoundingBox boundingBox_;

	std::vector<Transform> particleTransforms;

  Camera camera_;

  std::map<std::string, int> materialMappings_;
  std::vector<MaterialInfo> materialInfos_;
};