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

#include <memory>
#include <vulkan\vulkan.h>
#include <map>
#include <vector>
#include <glm\glm.hpp>

#include "renderables\MeshData.h"
#include "renderables\Frustum.h"
#include "renderables\Material.h"

#include "adaptiveGrid\AdaptiveGrid.h"

#include "..\passes\Passes.h"

class Scene;

namespace Renderer
{
  class BufferManager;
  class ImageManager;
  class QueueManager;
  class ShaderBindingManager;
  class ShadowMap;
  class Surface;

  class RenderScene
  {
  public:
    RenderScene();
    //Need to be done before the pass manager
    void RequestResources(BufferManager* bufferManager, ImageManager* imageManager, int bufferCount);
    //Creates vertex, index bufer per mesh, allocates storage buffer for mesh instances
    bool OnLoadScene(Scene* scene, BufferManager* bufferManager, ImageManager* imageManager, QueueManager* queueManager, int frameCount);
    void UpdateBindings(VkDevice device, ShaderBindingManager* bindingManager, ImageManager* imageManager);
    //Performs culling, create data for world matrices
    void Update(Scene* scene, BufferManager* bufferManager, ImageManager* imageManager, Surface* surface, int frameIndex);

    void BindMeshData(BufferManager* bufferManager, VkCommandBuffer commandBuffer);
    void BindMeshMaterial(VkCommandBuffer commandBuffer, ShaderBindingManager* bindingManager, int index);

    const std::vector<int>& GetMeshIndices(SubPassType subpass) const;
    const MeshData& GetMeshData() const;

    ShadowMap* GetShadowMap() { return shadowMap_.get(); }
    AdaptiveGrid* GetAdaptiveGrid() { return adaptiveGrid_.get(); }

  private:
    void UpdatePerFrameBuffers(BufferManager* bufferManager, int frameIndex);
    std::vector<VkDeviceSize> ResizeMeshBuffers(BufferManager* bufferManager);
    void ResizeTransformBuffers(BufferManager* bufferManager);

    MeshData meshData_;
    Frustum cameraFrustum_;

    std::map<SubPassType, std::vector<int>> activeMeshes_;
    std::vector<Material> materials_;

    std::vector<glm::mat4> world_;
    std::array<std::vector<glm::mat4>, MeshData::BUFFER_MAX> bufferData_;

    std::shared_ptr<ShadowMap> shadowMap_;
    std::shared_ptr<AdaptiveGrid> adaptiveGrid_;

    bool sceneLoaded_ = false;
    bool transformsChanged_ = false;
  };
}