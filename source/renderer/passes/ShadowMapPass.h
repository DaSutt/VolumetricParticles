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

#include <vector>
#include <glm\glm.hpp>

#include "Pass.h"

class Scene;

namespace Renderer
{
  class ShadowMap;
  class RenderScene;

  class ShadowMapPass : public Pass
  {
  public:
    ShadowMapPass(ShaderBindingManager* bindingManager, RenderPassManager* renderPassManager);

    void SetShadowMap(ShadowMap* shadowMap);
    void SetRenderScene(RenderScene* renderScene);
    void RequestResources(ImageManager* imageManager, BufferManager* bufferManager, int frameCount) override;
    bool Create(VkDevice device, ShaderManager* shaderManager, FrameBufferManager* frameBufferManager,
      Surface* surface, ImageManager* imageManager) override;

    void UpdateBufferData(Scene* scene);
    void Render(Surface* surface, FrameBufferManager* frameBufferManager,
      QueueManager* queueManager, BufferManager* bufferManager,
      std::vector<VkImageMemoryBarrier>& memoryBarrier, VkSemaphore* startSemaphore, int frameIndex) override;

    void OnReloadShaders(VkDevice device, ShaderManager* shaderManager) override;

    VkSemaphore* GetFinishedSemaphore() override;
  private:
    enum GraphicSubpass
    {
      GRAPHICS_SHADOW_DIR,
      GRAPHICS_MAX
    };

    bool CreateFrameBuffers(VkDevice device, FrameBufferManager* frameBufferManager, Surface* surface) override;
    bool CreateGraphicPipeline(VkDevice device, ShaderManager* shaderManager);

    ShadowMap* shadowMap_;
    int perObjectPushConstantIndex_ = -1;

    RenderScene* renderScene_;
  };
}