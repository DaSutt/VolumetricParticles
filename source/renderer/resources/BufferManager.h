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
#include <array>
#include <glm\glm.hpp>
#include <map>
#include <memory>

#include "..\wrapper\MemoryAllocator.h"
#include "..\scene\adaptiveGrid\AdaptiveGridData.h"

class Scene;

namespace Renderer
{
  class Instance;
  class QueueManager;
  class Surface;

  class BufferManager
  {
  public:
    enum BufferTypeBits
    {
      BUFFER_CONSTANT_BIT = 1 << 0,
      BUFFER_SCENE_BIT    = 1 << 1,
      BUFFER_STAGING_BIT  = 1 << 2,
      BUFFER_GRID_BIT     = 1 << 3,
      BUFFER_TEMP_BIT     = 1 << 4
    };

    enum BufferMemoryPool
    {
      MEMORY_CONSTANT,
      MEMORY_SCENE,
      MEMORY_STAGING,
      MEMORY_GRID,
      MEMORY_TEMP,
      MEMORY_MAX
    };

		enum BarrierType
		{
			BARRIER_WRITE_READ,
			BARRIER_READ_WRITE,
			BARRIER_WRITE_WRITE,
			BARRIER_MAX
		};

		struct BarrierInfo
		{
			int index;
			int frameIndex;
			uint32_t srcQueueFamilyIndex;
			uint32_t dstQueueFamilyIndex;
			BarrierType type;
			VkDeviceSize offset;
			VkDeviceSize size;
			BarrierInfo();
		};

    struct BufferInfo
    {
      uint32_t typeBits;
      BufferMemoryPool pool;
      VkDeviceSize size;
      VkBufferUsageFlags usage;
      void* data;
      int bufferingCount;
      std::vector<int> bufferIndices_;
      std::vector<void*> bufferData_;
    };

    BufferManager(Instance* instance);
    bool Create(Surface* surface);
    void SetActiveBuffer(int index);
    void UpdateConstantBuffers(int frameIndex);
    void BindBuffers(BufferMemoryPool memoryPool);

    int RequestBuffer(const BufferInfo& bufferInfo);
    int Ref_RequestBuffer(const BufferInfo& bufferInfo);

    void Release(VkDevice device);
    void ReleaseTempBuffer(int index);

    void* Map(int index, int typeBits);
    void Unmap(int index, int typeBits);
    void Bind(VkCommandBuffer commandBuffer, int index);
    void RequestBufferCopy(int index, int typeBits);
    void RequestTempBufferCopy(int tempIndex, int dstIndex, VkBufferCopy copy);
    void RequestImageCopy(int tempIndex, VkImage dst, VkImageLayout dstLayout, VkBufferImageCopy copy);
    void PerformCopies(QueueManager* queueManager);

    void OnLoadScene();
    void ResizeSceneBuffers();
    void RequestBufferResize(int index, VkDeviceSize newSize);
    void Ref_RequestBufferResize(int index, VkDeviceSize newSize);
    //Unbinds memory and recreates the buffer indices
    void Ref_ResizeBuffers(const std::vector<ResourceResize>& indices, BufferMemoryPool memoryPool);

		std::vector<VkBufferMemoryBarrier> Barrier(const std::vector<BarrierInfo>& barrierInfos);

    VkDescriptorBufferInfo* GetDescriptorInfo(int index);
    VkDescriptorBufferInfo* Ref_GetDescriptorInfo(int index, int offset);
    VkBuffer GetBuffer(int index, int typeBits);
    VkBuffer Ref_GetBuffer(int index, int typeBits, int offset);

    void BufferManager::Ref_UpdateConstantBuffer(int index, int frameIndex, void* data, VkDeviceSize size);
    void* BufferManager::Ref_Map(int index, int frameIndex, int typeBits);
    void BufferManager::Ref_Unmap(int index, int frameIndex, int typeBits);
  private:
    bool CreateCommandBuffers(VkDevice device, uint32_t bufferCount);
    VkBuffer CreateBuffer(const BufferInfo& bufferInfo, int index);
    bool Ref_CreateBuffer(BufferInfo& bufferInfo);
    VkBuffer CreateBufferObject(VkDevice device, const BufferInfo& bufferInfo);
    void BeginCopyBuffer();
    void ReleaseScene(VkDevice device);
    VkMemoryPropertyFlags GetMemoryFlags(BufferMemoryPool memoryPool) const;

    std::vector<BufferInfo> bufferInfos_;
    std::vector<BufferInfo> ref_BufferInfos_;
    std::vector<VkBuffer> buffers_;
    std::vector<VkBuffer> ref_buffers_;
    std::vector<VkDescriptorBufferInfo> descriptorInfos_;
    std::vector<VkDescriptorBufferInfo> ref_descriptorInfos_;
    std::vector<VkBuffer> tempBuffers_;
    std::map<int, int> bufferStagingMapping_;
    std::array<std::unique_ptr<Wrapper::MemoryAllocator>, MEMORY_MAX> memoryPools_;

    Instance* instance_ = nullptr;

    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> transferBuffers_;
    std::vector<VkFence> transferFences_;
    std::vector<bool> commandRecording_;
    int activeBuffer_ = -1;
  };
}
