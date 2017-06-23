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

#include <vulkan\vulkan.h>
#include <glm\glm.hpp>
#include <vector>

#include "..\ParticleSystemRenderable.h"

class Grid;
class Scene;

namespace Renderer
{
  class ImageManager;
  class ShaderBindingManager;

  class UniformGrid
  {
  public:
    struct PerParticleSystemData
    {
      glm::ivec3 offset;
      int activeParticles;
      glm::ivec3 toWorldOffset;
      float cellSize;
    };

    UniformGrid(Grid* sceneGrid);

    //Create the grid image3d
    bool Init(ImageManager* imageManager);
    //Copy data from particle systems into buffers
    void UpdateData(BufferManager* bufferManager);
    //Update grid with particle data
    void UpdateGrid(VkCommandBuffer& commandBuffer);
    void UpdateGridParticles(VkDevice device, VkCommandBuffer& commandBuffer, ShaderBindingManager* bindingManager,
      BufferManager* bufferManager, int shaderBinding);

    void OnLoadScene(VkDevice device, Grid* sceneGrid, BufferManager* bufferManager, ShaderBindingManager* bindingManager, Scene* scene);

    int GetScatteringExtinctionGrid() const { return inScatteringExtinctionGrid_; }
    int GetEmissionPhaseGrid() const { return emissionPhaseFactorGrid_; }
    const auto& GetTexTransform() const { return texTransform_; }

    bool SceneLoaded() const { return sceneLoaded_; }
  private:
    Grid* sceneGrid_;

    std::vector<ParticleSystemRenderable> particleSystems_;

    int inScatteringExtinctionGrid_ = -1;
    int emissionPhaseFactorGrid_ = -1;

    glm::mat4 texTransform_;

    bool sceneLoaded_ = false;
  };
}