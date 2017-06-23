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

#include "MemoryAllocator.h"

#include "PhysicalDevice.h"

namespace Wrapper
{
  MemoryAllocator::MemoryAllocator() :
    allocationSize_{0}
  {

  }

  MemoryAllocator::MemoryAllocator(uint32_t allocationSizeMB) :
    allocationSize_{allocationSizeMB << 20}
  {

  }

  namespace
  {
    VkDeviceSize CalcMemoryAlignedSize(VkDeviceSize size, VkDeviceSize alignment)
    {
      VkDeviceSize remainder = size % alignment;
      if (remainder == 0)
      {
        return size;
      }
      else
      {
        return size - remainder + alignment;
      }
    }
  }

  VkDeviceSize MemoryAllocator::BindImage(VkPhysicalDevice physicalDevice, VkDevice device, VkImage image)
  {
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    if (!AllocateMemory(physicalDevice, device, memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
    {
      return 0;
    }

    const auto alignedOffset = CalcMemoryAlignedSize(offset_, memRequirements.alignment);
    availableMemory_ -= (alignedOffset - offset_);
    offset_ = alignedOffset;

    const auto result = vkBindImageMemory(device, image, memory_.back(), offset_);
    if (result != VK_SUCCESS)
    {
      printf("Failed binding image memory, %d\n", result);
      return 0;
    }

    VkDeviceSize size = CalcMemoryAlignedSize(memRequirements.size, memRequirements.alignment);
    offset_ += size;
    availableMemory_ -= size;

    return size;
  }

  bool MemoryAllocator::BindBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkBuffer buffer,
    VkMemoryPropertyFlags memoryProperties, int bufferIndex)
  {
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    if (!AllocateMemory(physicalDevice, device, memRequirements, memoryProperties))
    {
      return false;
    }

		const auto alignedOffset = CalcMemoryAlignedSize(offset_, memRequirements.alignment);
		availableMemory_ -= (alignedOffset - offset_);
		offset_ = alignedOffset;

		const auto result = vkBindBufferMemory(device, buffer, memory_.back(), offset_);
    if (result != VK_SUCCESS)
    {
      printf("Failed binding buffer memory, %d\n", result);
      return false;
    }

    resourceMapping_[bufferIndex] = { offset_, static_cast<int>(memory_.size() - 1 )};

    VkDeviceSize size = CalcMemoryAlignedSize(memRequirements.size, memRequirements.alignment);
    offset_ += size;
    availableMemory_ -= size;

    return true;
  }

  bool MemoryAllocator::AllocateAndBindImage(VkPhysicalDevice physicalDevice, VkDevice device, VkImage image)
  {
    if (allocationSize_ != 0)
    {
      printf("Trying to allocate single resource from allocator with fixed block size\n");
      return false;
    }
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);
    allocationSize_ = CalcMemoryAlignedSize(memRequirements.size, memRequirements.alignment);

    return BindImage(physicalDevice, device, image) != 0;
  }

  bool MemoryAllocator::AllocateAndBindBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkBuffer buffer,
    VkMemoryPropertyFlags memoryProperties, int bufferIndex)
  {
    if (allocationSize_ != 0)
    {
      printf("Trying to allocate single resource from allocator with fixed block size\n");
      return false;
    }
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
    allocationSize_ = CalcMemoryAlignedSize(memRequirements.size, memRequirements.alignment);

    return BindBuffer(physicalDevice, device, buffer, memoryProperties, bufferIndex);
  }

  bool MemoryAllocator::AllocateMemory(VkPhysicalDevice physicalDevice, VkDevice device,
    VkMemoryRequirements memoryRequirements, VkMemoryPropertyFlags memoryProperties)
  {
    const auto size = CalcMemoryAlignedSize(memoryRequirements.size, memoryRequirements.alignment);
    if (size < availableMemory_)
    {
      return true;
    }

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.memoryTypeIndex = PhysicalDevice::GetMemoryType(physicalDevice,
      memoryRequirements.memoryTypeBits, memoryProperties);

    //if memory is larger then possible allocated size create single memory pool for that resource
    if (size > allocationSize_)
    {
      allocInfo.allocationSize = size;

      VkDeviceMemory memory = VK_NULL_HANDLE;
      auto result = vkAllocateMemory(device, &allocInfo, nullptr, &memory);
      if (result != VK_SUCCESS)
      {
        printf("Failed allocating memory, %d\n", result);
        return false;
      }

      memory_.push_back(memory);
      mapCount_.push_back(0);
      dataPtr_.push_back(nullptr);

      availableMemory_ = 0;
      offset_ = 0;
    }
    //if size is bigger create a new memory pool
    else if (size > availableMemory_)
    {
      allocInfo.allocationSize = allocationSize_;

      VkDeviceMemory memory = VK_NULL_HANDLE;
      auto result = vkAllocateMemory(device, &allocInfo, nullptr, &memory);
      if (result != VK_SUCCESS)
      {
        printf("Failed allocating memory, %d\n", result);
        return false;
      }
      memory_.push_back(memory);
      mapCount_.push_back(0);
      dataPtr_.push_back(nullptr);

      availableMemory_ = allocationSize_;
      offset_ = 0;
    }

    return true;
  }


  void MemoryAllocator::Release(VkDevice device)
  {
    for (auto& memory : memory_)
    {
      vkFreeMemory(device, memory, nullptr);
    }
    memory_.clear();
    offset_ = 0;
    availableMemory_ = 0;
  }

  void* MemoryAllocator::MapData(VkDevice device, int resourceIndex)
  {
    const auto& resourceMapping = resourceMapping_.find(resourceIndex);
    if (resourceMapping == resourceMapping_.end())
    {
      printf("Trying to map invalid resource %d", resourceIndex);
      return nullptr;
    }

    const auto memoryBlock = resourceMapping->second.memoryBlock;
    if (mapCount_[resourceMapping->second.memoryBlock] == 0)
    {
      vkMapMemory(device, memory_[memoryBlock], 0, VK_WHOLE_SIZE, 0,
        &dataPtr_[memoryBlock]);
    }
    ++mapCount_[memoryBlock];

    return reinterpret_cast<char*>(dataPtr_[memoryBlock]) + resourceMapping->second.offset;
  }

  MemoryAllocator::MappingState MemoryAllocator::Unmap(VkDevice device, int resourceIndex)
  {
    const auto& resourceMapping = resourceMapping_[resourceIndex];
    --mapCount_[resourceMapping.memoryBlock];

    if (mapCount_[resourceMapping.memoryBlock] == 0)
    {
      vkUnmapMemory(device, memory_[resourceMapping.memoryBlock]);
      dataPtr_[resourceMapping.memoryBlock] = nullptr;
      return MEMORY_MAP_FINISHED;
    }

    return MEMORY_MAPPED;
  }
}