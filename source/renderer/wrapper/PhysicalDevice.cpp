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

#include "PhysicalDevice.h"

namespace PhysicalDevice
{
  namespace
  {
    uint32_t FindMostSpecificQueue(const std::vector<VkQueueFamilyProperties>& properties, std::vector<uint32_t> usableQueues)
    {
      VkQueueFlags minFlags = VK_QUEUE_FLAG_BITS_MAX_ENUM;
      uint32_t queueIndex = UINT32_MAX;
      for (auto i : usableQueues)
      {
        if (minFlags > properties[i].queueFlags)
        {
          minFlags = properties[i].queueFlags;
          queueIndex = static_cast<uint32_t>(i);
        }
      }
      return queueIndex;
    }
  }

  uint32_t GetQueueFamilyIndex(VkPhysicalDevice physicalDevice, VkQueueFlags flags, uint32_t& maxQueues)
  {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    std::vector<uint32_t> usableQueueIndices;
    for (size_t i = 0; i < queueFamilies.size(); ++i)
    {
      const auto& queueFamily = queueFamilies[i];
      if (queueFamily.queueCount > 0 && queueFamily.queueFlags & flags)
      {
        usableQueueIndices.push_back(static_cast<uint32_t>(i));
      }
    }

    uint32_t queueIndex = UINT32_MAX;
    if (usableQueueIndices.empty())
    {
      printf("Queue not found with flags: %d\n", flags);
    }
    else
    {
      queueIndex = FindMostSpecificQueue(queueFamilies, usableQueueIndices);
      maxQueues = queueFamilies[queueIndex].queueCount;
    }

    return queueIndex;
  }

  uint32_t GetMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeBits, VkMemoryPropertyFlags propertyFlags) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t memoryType = 0; memoryType < memProperties.memoryTypeCount; memoryType++)
    {
      if (((typeBits & 1) == 1) && ((memProperties.memoryTypes[memoryType].propertyFlags & propertyFlags) == propertyFlags))
      {
        return memoryType;
      }
      typeBits >>= 1;
    }

    printf("No matching memory found\n");
    return UINT32_MAX;
  }

  VkFormat GetDepthImageFormat(VkPhysicalDevice physicalDevice, VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags targetFlags)
  {
    VkFormatProperties properties;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
    if ((properties.optimalTilingFeatures & targetFlags) == targetFlags)
    {
      return format;
    }
    return VkFormat::VK_FORMAT_UNDEFINED;
  }
}