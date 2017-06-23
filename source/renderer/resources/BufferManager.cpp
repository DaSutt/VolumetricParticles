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

#include "BufferManager.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm\gtx\euler_angles.hpp>

#include "..\..\scene\Scene.h"
#include "..\MeshRenderable.h"

#include "QueueManager.h"

#include "..\wrapper\Instance.h"
#include "..\wrapper\PhysicalDevice.h"
#include "..\wrapper\Surface.h"
#include "..\wrapper\Synchronization.h"

#include "..\passResources\ShaderBindingManager.h"

namespace Renderer
{
	BufferManager::BarrierInfo::BarrierInfo() :
		index{ -1 },
		frameIndex{ -1 },
		srcQueueFamilyIndex{ VK_QUEUE_FAMILY_IGNORED },
		dstQueueFamilyIndex{ VK_QUEUE_FAMILY_IGNORED },
		type{ BARRIER_MAX },
		offset{ 0 },
		size{ VK_WHOLE_SIZE }
	{}

  VkMemoryPropertyFlags BufferManager::GetMemoryFlags(BufferMemoryPool memoryPool) const
  {
    switch (memoryPool)
    {
    case MEMORY_CONSTANT:
    case MEMORY_GRID:
      return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    case MEMORY_SCENE:
      return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    case MEMORY_STAGING:
    case MEMORY_TEMP:
      return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    default:
      printf("Memory pool not yet supported\n");
      return 0;
    }
  }

  BufferManager::BufferManager(Instance* instance) :
    instance_{instance}
  {
    memoryPools_[MEMORY_CONSTANT] = std::make_unique<Wrapper::MemoryAllocator>(64);
    memoryPools_[MEMORY_SCENE] = std::make_unique<Wrapper::MemoryAllocator>(256);
    memoryPools_[MEMORY_STAGING] = std::make_unique<Wrapper::MemoryAllocator>(256);
    memoryPools_[MEMORY_GRID] = std::make_unique<Wrapper::MemoryAllocator>(64);
    memoryPools_[MEMORY_TEMP] = std::make_unique<Wrapper::MemoryAllocator>(64);
  }

  bool BufferManager::Create(Surface* surface)
  {
    const auto device = instance_->GetDevice();

    const auto imageCount = surface->GetImageCount();
    if (!CreateCommandBuffers(device, surface->GetImageCount()))
    {
      printf("BufferManager: command buffer creation failed\n");
      return false;
    }

    transferFences_.resize(imageCount, VK_NULL_HANDLE);
    for (auto& fence : transferFences_)
    {
      if (!Wrapper::CreateFence(device, VK_FENCE_CREATE_SIGNALED_BIT, &fence))
      {
        return false;
      }
    }

    return true;
  }

  void BufferManager::SetActiveBuffer(int index)
  {
    activeBuffer_ = index;
  }

  void BufferManager::BindBuffers(BufferMemoryPool memoryPool)
  {
    const auto device = instance_->GetDevice();
    const auto physicalDevice = instance_->GetPhysicalDevice();

    std::vector<std::vector<std::pair<BufferMemoryPool, int>>> bufferIndices(3);
    for (const auto& bufferInfo : ref_BufferInfos_)
      {
        for (int i = 0; i < bufferInfo.bufferingCount; ++i)
        {
        bufferIndices[i].push_back({ bufferInfo.pool, bufferInfo.bufferIndices_[i] });
        }
        if (bufferInfo.typeBits & BUFFER_STAGING_BIT)
        {
          bufferIndices[0].push_back({ MEMORY_STAGING, bufferInfo.bufferIndices_[bufferInfo.bufferingCount] });
        }
      }

    for (size_t frameIndex = 0; frameIndex < bufferIndices.size(); ++frameIndex)
    {
      for (const auto& bufferIndex : bufferIndices[frameIndex])
      {
        if (bufferIndex.first == memoryPool)
        {
          const auto buffer = ref_buffers_[bufferIndex.second];
          memoryPools_[bufferIndex.first]->BindBuffer(physicalDevice, device, buffer,
          GetMemoryFlags(bufferIndex.first), bufferIndex.second);
        }
      }
    }

    ref_descriptorInfos_.clear();
    for (const auto& buffer : ref_buffers_)
    {
      VkDescriptorBufferInfo bufferInfo = {};
      bufferInfo.buffer = buffer;
      bufferInfo.offset = 0;
      bufferInfo.range = VK_WHOLE_SIZE;
      ref_descriptorInfos_.push_back(bufferInfo);
    }
  }

  void BufferManager::UpdateConstantBuffers(int frameIndex)
  {
    for (int i = 0; i < static_cast<int>(bufferInfos_.size()); ++i)
    {
      if (bufferInfos_[i].typeBits & BUFFER_CONSTANT_BIT)
      {
        auto data = Map(i, BUFFER_CONSTANT_BIT);
        memcpy(data, bufferInfos_[i].data, bufferInfos_[i].size);
        Unmap(i, BUFFER_CONSTANT_BIT);
      }
    }

    for (int i = 0; i < static_cast<int>(ref_BufferInfos_.size()); ++i)
    {
      if (ref_BufferInfos_[i].typeBits & BUFFER_CONSTANT_BIT && ref_BufferInfos_[i].data != nullptr)
      {
        const auto index = ref_BufferInfos_[i].bufferIndices_[frameIndex];
        auto data = Map(index, BUFFER_CONSTANT_BIT);
        if (ref_BufferInfos_[i].bufferData_.empty())
        {
          memcpy(data, ref_BufferInfos_[i].data, ref_BufferInfos_[i].size);
        }
        else
        {
          memcpy(data, ref_BufferInfos_[i].bufferData_[frameIndex], ref_BufferInfos_[i].size);
        }
        Unmap(index, BUFFER_CONSTANT_BIT);
      }
    }
  }

  void BufferManager::Ref_UpdateConstantBuffer(int index, int frameIndex, void* data, VkDeviceSize size)
  {
    const auto bufferIndex = ref_BufferInfos_[index].bufferIndices_[frameIndex];
    auto bufferData = Map(bufferIndex, BUFFER_CONSTANT_BIT);
    assert(size <= ref_BufferInfos_[index].size);
    memcpy(bufferData, data, size);
    Unmap(bufferIndex, BUFFER_CONSTANT_BIT);
  }

  void* BufferManager::Ref_Map(int index, int frameIndex, int typeBits)
  {
    const auto bufferIndex = ref_BufferInfos_[index].bufferIndices_[frameIndex];
    return Map(bufferIndex, typeBits);
  }

  void BufferManager::Ref_Unmap(int index, int frameIndex, int typeBits)
  {
    const auto bufferIndex = ref_BufferInfos_[index].bufferIndices_[frameIndex];
    Unmap(bufferIndex, typeBits);
  }

  int BufferManager::RequestBuffer(const BufferInfo& bufferInfo)
  {
    int index = 0;
    if (bufferInfo.typeBits & BUFFER_TEMP_BIT)
    {
      index = static_cast<int>(tempBuffers_.size());
    }
    else
    {
      index = static_cast<int>(bufferInfos_.size());
    }

    VkBuffer buffer = CreateBuffer(bufferInfo, index);
    if(buffer == VK_NULL_HANDLE)
    {
      printf("Buffer creation failed\n");
      return -1;
    }

    if (bufferInfo.typeBits & BUFFER_TEMP_BIT)
    {
      tempBuffers_.push_back(buffer);
    }
    else
    {
      buffers_.push_back(buffer);
      bufferInfos_.push_back(bufferInfo);

      VkDescriptorBufferInfo bufferInfo = {};
      bufferInfo.buffer = buffers_.back();
      bufferInfo.offset = 0;
      bufferInfo.range = VK_WHOLE_SIZE;
      descriptorInfos_.push_back(bufferInfo);
    }

    if (bufferInfo.typeBits & BUFFER_STAGING_BIT && bufferInfo.pool != MEMORY_STAGING)
    {
      BufferInfo stagingInfo;
      stagingInfo.size = bufferInfo.size;
      stagingInfo.pool = MEMORY_STAGING;
      stagingInfo.typeBits = BUFFER_STAGING_BIT;
      stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
      bufferStagingMapping_[index] = index + 1;
      if (RequestBuffer(stagingInfo) == -1) { return -1; }
    }

    return index;
  }

  int BufferManager::Ref_RequestBuffer(const BufferInfo& bufferInfo)
  {
    assert(bufferInfo.bufferingCount > 0);
    int index = 0;
    if (bufferInfo.typeBits & BUFFER_TEMP_BIT)
    {
      index = static_cast<int>(tempBuffers_.size());
    }
    else
    {
      index = static_cast<int>(ref_BufferInfos_.size());
    }
    ref_BufferInfos_.push_back(bufferInfo);

    if (!Ref_CreateBuffer(ref_BufferInfos_.back()))
    {
      printf("Buffer creation failed\n");
      return -1;
    }

    return index;
  }

  void BufferManager::Release(VkDevice device)
  {
    vkWaitForFences(device, 1, &transferFences_[activeBuffer_], VK_TRUE, UINT64_MAX);
    for (auto& f : transferFences_)
    {
      vkDestroyFence(device, f, nullptr);
    }

    for (auto& buffer : ref_buffers_)
    {
      vkDestroyBuffer(device, buffer, nullptr);
    }

    for (size_t i = 0; i < bufferInfos_.size(); ++i)
    {
      vkDestroyBuffer(device, buffers_[i], nullptr);
    }
    for (auto& b : tempBuffers_)
    {
      vkDestroyBuffer(device, b, nullptr);
    }
    for (auto& memPool : memoryPools_)
    {
      memPool->Release(device);
    }

    vkDestroyCommandPool(device, commandPool_, nullptr);
  }

  void BufferManager::ReleaseTempBuffer(int index)
  {
    //if (index == tempBuffers_.size() - 1)
    //{
    //  vkDestroyBuffer(instance_->GetDevice(), tempBuffers_[index], nullptr);
    //  tempBuffers_.pop_back();
    //}
    //else
    //{
    //  printf("Temp buffer not released because not the last\n");
    //}
  }

  void* BufferManager::Map(int index, int typeBits)
  {
    if (typeBits & BUFFER_STAGING_BIT)
    {
      return memoryPools_[MEMORY_STAGING]->MapData(instance_->GetDevice(), bufferStagingMapping_[index]);
    }
    else if(typeBits & BUFFER_TEMP_BIT)
    {
      return memoryPools_[MEMORY_TEMP]->MapData(instance_->GetDevice(), index);
    }
    else if (typeBits & BUFFER_CONSTANT_BIT)
    {
      return memoryPools_[MEMORY_CONSTANT]->MapData(instance_->GetDevice(), index);
    }
    else if (typeBits & BUFFER_GRID_BIT)
    {
      return memoryPools_[MEMORY_GRID]->MapData(instance_->GetDevice(), index);
    }
    else
    {
      printf("Trying to map buffer with invalid typeBits, %d", typeBits);
      return nullptr;
    }
  }

  void BufferManager::Unmap(int index, int typeBits)
  {
    if (typeBits & BUFFER_STAGING_BIT)
    {
      memoryPools_[MEMORY_STAGING]->Unmap(instance_->GetDevice(), bufferStagingMapping_[index]);
    }
    else if (typeBits & BUFFER_TEMP_BIT)
    {
      memoryPools_[MEMORY_TEMP]->Unmap(instance_->GetDevice(), index);
    }
    else if (typeBits & BUFFER_CONSTANT_BIT)
    {
      memoryPools_[MEMORY_CONSTANT]->Unmap(instance_->GetDevice(), index);
    }
    else if (typeBits & BUFFER_GRID_BIT)
    {
      memoryPools_[MEMORY_GRID]->Unmap(instance_->GetDevice(), index);
    }
    else
    {
      printf("Trying to unmap buffer with invalid typeBits, %d", typeBits);
    }
  }

  void BufferManager::Bind(VkCommandBuffer commandBuffer, int index)
  {
    const auto usage = bufferInfos_[index].usage;
    if (usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
    {
      VkDeviceSize offsets[] = { 0 };
      vkCmdBindVertexBuffers(commandBuffer, 0, 1, &buffers_[index], offsets);
    }
    else if (usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
    {
      vkCmdBindIndexBuffer(commandBuffer, buffers_[index], 0, VK_INDEX_TYPE_UINT16);
    }
    else
    {
      printf("Unable to bind buffer with usage %d\n", usage);
    }
  }

  void BufferManager::RequestBufferCopy(int index, int typeBits)
  {
    BeginCopyBuffer();

    VkBufferCopy bufferCopy = {};
    bufferCopy.size = bufferInfos_[index].size;
    bufferCopy.srcOffset = 0;
    bufferCopy.dstOffset = 0;

    vkCmdCopyBuffer(transferBuffers_[activeBuffer_],
      buffers_[index + 1],
      buffers_[index],
      1, &bufferCopy);
  }

  void BufferManager::RequestTempBufferCopy(int tempIndex, int dstIndex, VkBufferCopy copy)
  {
    BeginCopyBuffer();

    auto copyRegion = copy;

    vkCmdCopyBuffer(transferBuffers_[activeBuffer_],
      tempBuffers_[tempIndex],
      buffers_[dstIndex],
      1, &copyRegion);
  }

  void BufferManager::RequestImageCopy(int tempIndex, VkImage dst, VkImageLayout dstLayout, VkBufferImageCopy copy)
  {
    BeginCopyBuffer();

    vkCmdCopyBufferToImage(transferBuffers_[activeBuffer_], tempBuffers_[tempIndex],
      dst, dstLayout, 1, &copy);
  }

  void BufferManager::PerformCopies(QueueManager* queueManager)
  {
    if (commandRecording_[activeBuffer_])
    {
      vkEndCommandBuffer(transferBuffers_[activeBuffer_]);

      VkSubmitInfo submitInfo = {};
      submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
      submitInfo.commandBufferCount = 1;
      submitInfo.pCommandBuffers = &transferBuffers_[activeBuffer_];

      vkQueueSubmit(queueManager->GetQueue(QueueManager::QUEUE_TRANSFER), 1, &submitInfo, transferFences_[activeBuffer_]);

      commandRecording_[activeBuffer_] = false;
    }
  }

  void BufferManager::OnLoadScene()
  {
    const auto device = instance_->GetDevice();

    vkWaitForFences(device, 1,
      &transferFences_[activeBuffer_], VK_TRUE, UINT64_MAX);

    memoryPools_[MEMORY_CONSTANT]->Release(device);
    memoryPools_[MEMORY_SCENE]->Release(device);
    memoryPools_[MEMORY_STAGING]->Release(device);

    for (int i = 0; i < static_cast<int>(bufferInfos_.size()); ++i)
    {
      if (bufferInfos_[i].typeBits & BUFFER_SCENE_BIT)
      {
        vkDestroyBuffer(device, buffers_[i], nullptr);
        vkDestroyBuffer(device, buffers_[bufferStagingMapping_[i]], nullptr);
      }
    }

    for (int i = 0; i < ref_BufferInfos_.size(); ++i)
    {
      if (ref_BufferInfos_[i].typeBits & BUFFER_SCENE_BIT)
      {
        for (const auto index : ref_BufferInfos_[i].bufferIndices_)
        {
          vkDestroyBuffer(device, ref_buffers_[index], nullptr);
        }
      }
    }
  }

  void BufferManager::ResizeSceneBuffers()
  {
    for (int i = 0; i < static_cast<int>(bufferInfos_.size()); ++i)
    {
      if (bufferInfos_[i].typeBits & BUFFER_SCENE_BIT)
      {
        buffers_[i] = CreateBuffer(bufferInfos_[i], i);
        descriptorInfos_[i].buffer = buffers_[i];

        const auto stagingIndex = bufferStagingMapping_[i];
        buffers_[stagingIndex] = CreateBuffer(bufferInfos_[stagingIndex], stagingIndex);
        descriptorInfos_[stagingIndex].buffer = buffers_[stagingIndex];
      }
    }

    for (int i = 0; i < ref_BufferInfos_.size(); ++i)
    {
      if (ref_BufferInfos_[i].typeBits & BUFFER_SCENE_BIT)
      {
        Ref_CreateBuffer(ref_BufferInfos_[i]);
      }
    }
    BindBuffers(MEMORY_CONSTANT);
  }

  void BufferManager::RequestBufferResize(int index, VkDeviceSize newSize)
  {
    bufferInfos_[index].size = newSize;
    if (bufferInfos_[index].typeBits & BUFFER_STAGING_BIT)
    {
      bufferInfos_[bufferStagingMapping_[index]].size = newSize;
    }
  }

  void BufferManager::Ref_RequestBufferResize(int index, VkDeviceSize newSize)
  {
    ref_BufferInfos_[index].size = newSize;
  }

  void BufferManager::Ref_ResizeBuffers(const std::vector<ResourceResize>& indices, BufferMemoryPool memoryPool)
  {
    const auto device = instance_->GetDevice();
    //TODO: remove with correct fences
    vkDeviceWaitIdle(device);
    memoryPools_[memoryPool]->Release(device);

    for (size_t i = 0; i < indices.size(); ++i)
    {
      const auto index = indices[i].index;
      ref_BufferInfos_[index].size = indices[i].size;

      for (const auto bufferIndex : ref_BufferInfos_[index].bufferIndices_)
      {
        vkDestroyBuffer(device, ref_buffers_[bufferIndex], nullptr);
      }
      Ref_CreateBuffer(ref_BufferInfos_[index]);
    }
    BindBuffers(memoryPool);
  }

	namespace
	{
		struct AccessMasks
		{
			VkAccessFlags src;
			VkAccessFlags dst;
		};

		AccessMasks GetAccessMasks(BufferManager::BarrierType type)
		{
			switch (type)
			{
			case BufferManager::BARRIER_WRITE_READ:
				return { VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT };
			case BufferManager::BARRIER_READ_WRITE:
				return { VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT };
			case BufferManager::BARRIER_WRITE_WRITE:
				return { VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT };
			default:
				return {};
			}
		}
	}

	std::vector<VkBufferMemoryBarrier> BufferManager::Barrier(const std::vector<BarrierInfo>& barrierInfos)
	{
		std::vector<VkBufferMemoryBarrier> barriers;
		for (const auto info : barrierInfos)
		{
			VkBufferMemoryBarrier barrier = {};
			barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;

			const auto accessMasks = GetAccessMasks(info.type);
			barrier.srcAccessMask = accessMasks.src;
			barrier.dstAccessMask = accessMasks.dst;
			barrier.srcQueueFamilyIndex = info.srcQueueFamilyIndex;
			barrier.dstQueueFamilyIndex = info.dstQueueFamilyIndex;
			barrier.buffer = ref_buffers_[ref_BufferInfos_[info.index].bufferIndices_[info.frameIndex]];
			barrier.offset = info.offset;
			barrier.size = info.size;

			barriers.push_back(barrier);
		}
		return barriers;
	}

  VkDescriptorBufferInfo* BufferManager::Ref_GetDescriptorInfo(int index, int offset)
  {
    const int bufferIndex = ref_BufferInfos_[index].bufferIndices_[offset];
    return &ref_descriptorInfos_[bufferIndex];
  }

  VkBuffer BufferManager::Ref_GetBuffer(int index, int typeBits, int offset)
  {
    const int bufferIndex = ref_BufferInfos_[index].bufferIndices_[offset];
    return ref_buffers_[bufferIndex];
  }

  VkDescriptorBufferInfo* BufferManager::GetDescriptorInfo(int index)
  {
    return &descriptorInfos_[index];
  }

  VkBuffer BufferManager::GetBuffer(int index, int typeBits)
  {
    if (typeBits & BUFFER_TEMP_BIT)
    {
      return tempBuffers_[index];
    }
    else
    {
      return buffers_[index];
    }
  }

  bool BufferManager::CreateCommandBuffers(VkDevice device, uint32_t bufferCount)
  {
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = QueueManager::GetQueueFamilyIndex(QueueManager::QUEUE_TRANSFER);
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    auto result = vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool_);
    if (result != VK_SUCCESS)
    {
      printf("Failed creating command pool, %d\n", result);
      return false;
    }

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = bufferCount;

    transferBuffers_.resize(bufferCount, VK_NULL_HANDLE);
    result = vkAllocateCommandBuffers(device, &allocInfo, transferBuffers_.data());
    if (result != VK_SUCCESS)
    {
      printf("Failed creating command buffers, %d\n", result);
      return false;
    }
    commandRecording_.resize(bufferCount, false);
    activeBuffer_ = 0;

    return true;
  }

  VkBuffer BufferManager::CreateBuffer(const BufferInfo& bufferInfo, int index)
  {
    const auto device = instance_->GetDevice();
    const auto physicalDevice = instance_->GetPhysicalDevice();

    auto buffer = CreateBufferObject(device, bufferInfo);
    if (buffer == VK_NULL_HANDLE)
    {
      return VK_NULL_HANDLE;
    }

    if (!memoryPools_[bufferInfo.pool]->BindBuffer(physicalDevice, device, buffer,
      GetMemoryFlags(bufferInfo.pool), index))
    {
      printf("Failed binding buffer\n");
      return VK_NULL_HANDLE;
    }

    return buffer;
  }

  bool BufferManager::Ref_CreateBuffer(BufferInfo& bufferInfo)
  {
    const auto device = instance_->GetDevice();

    auto bufferCount = bufferInfo.bufferingCount;
    bool append = bufferInfo.bufferIndices_.empty();
    // add additional staging buffer
    if (append && bufferInfo.typeBits & BUFFER_STAGING_BIT)
    {
      ++bufferCount;
    }

    for (size_t i = 0; i < bufferCount; ++i)
    {
      const auto buffer = CreateBufferObject(device, bufferInfo);
      if (buffer == VK_NULL_HANDLE)
      {
        return false;
      }

      if (append)
      {
        bufferInfo.bufferIndices_.push_back(static_cast<int>(ref_buffers_.size()));
        ref_buffers_.push_back(buffer);
      }
      else
      {
        ref_buffers_[bufferInfo.bufferIndices_[i]] = buffer;
      }
    }

    return true;
  }

  VkBuffer BufferManager::CreateBufferObject(VkDevice device, const BufferInfo& bufferInfo)
  {
    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = bufferInfo.size;
    bufferCreateInfo.usage = bufferInfo.usage;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer = VK_NULL_HANDLE;
    const auto result = vkCreateBuffer(device, &bufferCreateInfo, nullptr, &buffer);
    if (result != VK_SUCCESS)
    {
      printf("Create buffer failed, %d", result);
      return VK_NULL_HANDLE;
    }
    return buffer;
  }

  void BufferManager::BeginCopyBuffer()
  {
    if (!commandRecording_[activeBuffer_])
    {
      auto device = instance_->GetDevice();
      vkWaitForFences(device, 1, &transferFences_[activeBuffer_], VK_TRUE, UINT64_MAX);
      vkResetFences(device, 1, &transferFences_[activeBuffer_]);

      VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
      beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      vkBeginCommandBuffer(transferBuffers_[activeBuffer_], &beginInfo);
      commandRecording_[activeBuffer_] = true;
    }
  }

  void BufferManager::ReleaseScene(VkDevice device)
  {
    for (int i = 0; i < static_cast<int>(bufferInfos_.size()); ++i)
    {
      vkDestroyBuffer(device, buffers_[i], nullptr);
      if (bufferInfos_[i].typeBits & BUFFER_STAGING_BIT)
      {
        vkDestroyBuffer(device, buffers_[bufferStagingMapping_[i]], nullptr);
      }
    }
    memoryPools_[MEMORY_SCENE]->Release(device);
    memoryPools_[MEMORY_SCENE] = std::make_unique<Wrapper::MemoryAllocator>(256);
  }
}