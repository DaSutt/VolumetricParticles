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
#include <map>

namespace Wrapper
{
  class MemoryAllocator
  {
  public:
    enum MappingState
    {
      MEMORY_MAPPED,
      MEMORY_MAP_FINISHED
    };

    MemoryAllocator();
    MemoryAllocator(uint32_t allocationSizeMB);

    //Tries to bind image to memory, if not enough space is available allocate a new block
    //Returns the size of allocated memory
    VkDeviceSize BindImage(VkPhysicalDevice physicalDevice, VkDevice device, VkImage image);
    bool BindBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkBuffer buffer,
      VkMemoryPropertyFlags memoryProperties, int bufferIndex);
    //Allocates a single block of memory the size of an image
    bool AllocateAndBindImage(VkPhysicalDevice physicalDevice, VkDevice device, VkImage image);
    bool AllocateAndBindBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkBuffer buffer,
      VkMemoryPropertyFlags memoryProperties, int bufferIndex);
    void Release(VkDevice device);

    void* MapData(VkDevice device, int resourceIndex);
    MappingState Unmap(VkDevice device, int resourceIndex);
  private:
    struct AllocationInfo
    {
      VkDeviceSize offset;
      int memoryBlock;
    };

    bool AllocateMemory(VkPhysicalDevice physicalDevice, VkDevice device, VkMemoryRequirements memoryRequirements,
      VkMemoryPropertyFlags memoryProperties);

    VkDeviceSize allocationSize_;

    std::vector<VkDeviceMemory> memory_;
    std::vector<int> mapCount_;
    std::vector<void*> dataPtr_;
    std::map<int, AllocationInfo> resourceMapping_;

    VkDeviceSize offset_ = 0;
    VkDeviceSize availableMemory_ = 0;
  };
}