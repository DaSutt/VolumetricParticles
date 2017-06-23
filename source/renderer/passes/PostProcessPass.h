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

#include "Pass.h"

#include <glm\glm.hpp>

class Scene;

namespace Renderer
{
  class RenderScene;

  class PostProcessPass : public Pass
  {
  public:
    PostProcessPass(ShaderBindingManager* bindingManager, RenderPassManager* renderPassManager);

    void RequestResources(ImageManager* imageManager, BufferManager* bufferManager, int frameCount) override;
    void SetImageIndices(int meshRenderingIndex, int raymarchingIndex, int depthImageIndex, int noiseImage);
    bool Create(VkDevice device, ShaderManager* shaderManager, FrameBufferManager* frameBufferManager,
      Surface* surface, ImageManager* imageManager) override;

    void UpdateBufferData(Scene* scene, Surface* surface, RenderScene* renderScene);
		void Render(Surface* surface, FrameBufferManager* frameBufferManager,
			QueueManager* queueManager, BufferManager* bufferManager,
			std::vector<VkImageMemoryBarrier>& memoryBarrier, VkSemaphore* startSemaphore, int frameIndex) override;

    void OnReloadShaders(VkDevice device, ShaderManager* shaderManager) override;

    VkSemaphore* GetFinishedSemaphore() override;
  private:
    enum GraphicsSubpasses
    {
      GRAPHICS_BLENDING,
      GRAPHICS_DEBUG,
      GRAPHICS_MAX
    };

    struct DebugData
    {
      glm::mat4 worldViewProj;
      glm::vec4 color;
    };

    struct FrameData
    {
      glm::vec4 randomness;
      glm::vec2 screenSize;
    };
    
    bool CreateFrameBuffers(VkDevice device, FrameBufferManager* frameBufferManager, Surface* surface) override;
    bool CreateGraphicPipeline(VkDevice device, ShaderManager* shaderManager);

    int meshRenderingImageIndex_ = -1;
    int raymarchingImageIndex_ = -1;
    int debugPushConstantIndex_ = -1;
    int noiseImageIndex_ = -1;
    int depthImageIndex_ = -1;

    int frameBuffer_;
    std::vector<DebugData> debugBoundingBoxes_;
    FrameData frameData_;

		ImageManager* imageManager_ = nullptr;
  };
}