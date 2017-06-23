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

#include <array>

#include "Pass.h"
#include "..\..\scene\Components.h"

class Scene;

namespace Renderer
{
  class ShadowMap;
  class UniformGrid;
  class RenderScene;

  /*
    Forward rendering of the opaque scene geometry
    Rendered to offscreen render target
  */
  class MeshPass : public Pass
  {
  public:
    MeshPass(ShaderBindingManager* bindingManager, RenderPassManager* renderPassManager);

    void RequestResources(ImageManager* imageManager, BufferManager* bufferManager, int frameCount) override;
    void SetShadowMap(ShadowMap* shadowMap);
    void SetImageIndices(int offscreenImage, int depthImage);
    void SetRenderScene(RenderScene* renderScene);
    bool Create(VkDevice device, ShaderManager* shaderManager, FrameBufferManager* frameBufferManager,
      Surface* surface, ImageManager* imageManager) override;

    void UpdateBufferData(Scene* scene, RenderScene* renderScene);
    void Render(Surface* surface, FrameBufferManager* frameBufferManager,
      QueueManager* queueManager, BufferManager* bufferManager,
      std::vector<VkImageMemoryBarrier>& memoryBarrier, VkSemaphore* startSemaphore, int frameIndex) override;

    void OnReloadShaders(VkDevice device, ShaderManager* shaderManager) override;

    VkSemaphore* GetFinishedSemaphore() override;
  private:
    enum GraphicSubpasses
    {
      GRAPHIC_MESH,
      GRAPHIC_MAX
    };

    struct PerFrameData
    {
      std::array<glm::mat4x4, 4> shadowCascades_;
      DirectionalLight dirLight;
      glm::vec3 cameraPos_;
      float padding;
    };

    bool CreateFrameBuffers(VkDevice device, FrameBufferManager* frameBufferManager, Surface* surface) override;
    bool CreateGraphicPipeline(VkDevice device, ShaderManager* shaderManager);

    std::vector<glm::mat4> perObjectFrameData_;
    PerFrameData frameData_;
    glm::mat4 worldViewProjGrid_;

    int perFrameBuffer_ = -1;
    int perFrameGridBuffer_ = -1;

    int pushConstantIndex_ = -1;
    int perMaterialSceneBinding_ = -1;

    int offscreenImageIndex_;
    int depthImageIndex_;

    ShadowMap* shadowMap_;
    RenderScene* renderScene_;
  };
}