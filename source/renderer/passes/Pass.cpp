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

#include "Pass.h"

namespace Renderer
{
  Pass::Pipeline::Pipeline() :
    pipeline{ VK_NULL_HANDLE },
    passFinishedSemaphore{ VK_NULL_HANDLE },
    shaderBinding{ -1 }
  {}
  Pass::Pipeline::Pipeline(int binding) :
    pipeline{ VK_NULL_HANDLE },
    passFinishedSemaphore{ VK_NULL_HANDLE },
    shaderBinding{ binding }
  {}

  Pass::Pass(ShaderBindingManager* bindingManager, RenderPassManager* renderPassManager) :
    bindingManager_{ bindingManager },
    renderPassManager_{ renderPassManager }
  {}

  bool Pass::CreateFrameBuffers(VkDevice device, FrameBufferManager* frameBufferManager, Surface* surface)
  {
    return true;
  }

  bool Pass::CreateCommandBuffers(VkDevice device, QueueManager::Queues queue, uint32_t count)
  {
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = QueueManager::GetQueueFamilyIndex(queue);
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    auto result = vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool_);
    if (result != VK_SUCCESS)
    {
      printf("Failed creating command pool, %d\n", result);
      return false;
    }

    commandBuffers_.resize(count, VK_NULL_HANDLE);

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers_.size());

    std::vector<VkCommandBuffer> buffers(allocInfo.commandBufferCount);
    result = vkAllocateCommandBuffers(device, &allocInfo, buffers.data());
    if (result != VK_SUCCESS)
    {
      printf("Failed creating command buffers, %d\n", result);
      return false;
    }

    for (size_t i = 0; i < commandBuffers_.size(); ++i)
    {
      commandBuffers_[i] = buffers[i];
    }
    return true;
  }

  void Pass::DestroyPipelines(VkDevice device, std::vector<Pipeline>& pipelines)
  {
    for (auto p : pipelines)
    {
      if (p.pipeline != VK_NULL_HANDLE)
      {
        vkDestroyPipeline(device, p.pipeline, nullptr);
        p.pipeline = VK_NULL_HANDLE;
      }
    }
  }

  void Pass::DestroySemaphores(VkDevice device, std::vector<Pipeline>& pipelines)
  {
    for (auto p : pipelines)
    {
      if (p.passFinishedSemaphore != VK_NULL_HANDLE)
      {
        vkDestroySemaphore(device, p.passFinishedSemaphore, nullptr);
        p.passFinishedSemaphore = VK_NULL_HANDLE;
      }
    }
  }

  void Pass::Update(VkDevice device, Surface* surface, ImageManager* imageManager, BufferManager* bufferManager)
  {

  }

  void Pass::OnReloadShaders(VkDevice device, ShaderManager* shaderManager)
  {
    DestroyPipelines(device, graphicPipelines_);
    DestroyPipelines(device, computePipelines_);
  }

  void Pass::Release(VkDevice device)
  {
    DestroyPipelines(device, graphicPipelines_);
    DestroyPipelines(device, computePipelines_);

    DestroySemaphores(device, graphicPipelines_);
    DestroySemaphores(device, computePipelines_);

    vkDestroyCommandPool(device, commandPool_, nullptr);
  }
}