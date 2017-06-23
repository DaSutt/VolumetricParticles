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
#include <vector>

#include "Passes.h"
#include "..\resources\QueueManager.h"

namespace Renderer
{
  class ShaderManager;
  class ShaderBindingManager;
  class RenderPassManager;
  class Surface;
  class FrameBufferManager;
  class ImageManager;
  class BufferManager;

  class Pass
  {
  public:
    Pass() = delete;
    Pass(const Pass&) = delete;
    Pass& operator= (const Pass&) = delete;

    Pass(ShaderBindingManager* bindingManager, RenderPassManager* renderPassManager);
    virtual ~Pass() {}

    virtual void RequestResources(ImageManager* imageManager, BufferManager* bufferManager, int frameCount) = 0;
    virtual bool Create(VkDevice device, ShaderManager* shaderManager, FrameBufferManager* frameBufferManager, Surface* surface, ImageManager* imageManager) = 0;

    virtual void Update(VkDevice device, Surface* surface, ImageManager* imageManager, BufferManager* bufferManager);
    virtual void Render(Surface* surface, FrameBufferManager* frameBufferManager,
      QueueManager* queueManager, BufferManager* bufferManager,
      std::vector<VkImageMemoryBarrier>& memoryBarrier, VkSemaphore* startSemaphore, int frameIndex) = 0;

    virtual void OnReloadShaders(VkDevice device, ShaderManager* shaderManager);
    virtual void Release(VkDevice device);

    virtual VkSemaphore* GetFinishedSemaphore() = 0;
  protected:
    struct Pipeline
    {
      VkPipeline pipeline;
      VkSemaphore passFinishedSemaphore;
      int shaderBinding;
      Pipeline();
      Pipeline(int binding);
    };

    virtual bool CreateFrameBuffers(VkDevice device, FrameBufferManager* frameBufferManager, Surface* surface);
    bool CreateCommandBuffers(VkDevice device, QueueManager::Queues queue, uint32_t count);
    void DestroyPipelines(VkDevice device, std::vector<Pipeline>& pipelines);
    void DestroySemaphores(VkDevice device, std::vector<Pipeline>& pipelines);

    ShaderBindingManager* bindingManager_;
    RenderPassManager* renderPassManager_;

    std::vector<Pipeline> graphicPipelines_;
    std::vector<Pipeline> computePipelines_;

    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;

    std::vector<int> frameBufferIndices_;

    PassType passType_;
  };
}