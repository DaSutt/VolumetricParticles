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

#include "QueueManager.h"

#include "..\wrapper\PhysicalDevice.h"
#include "..\wrapper\Instance.h"

#include <map>

namespace Renderer
{
  const std::vector<VkQueueFlagBits> QueueManager::queueFlags =
  {
    VK_QUEUE_GRAPHICS_BIT,
    VK_QUEUE_COMPUTE_BIT,
    VK_QUEUE_TRANSFER_BIT
  };

  std::vector<uint32_t> QueueManager::queueFamilyIndices_;
  std::vector<uint32_t> QueueManager::queueIndices_;

  std::vector<VkDeviceQueueCreateInfo> QueueManager::GetQueueInfo(VkPhysicalDevice physicalDevice)
  {
    const static float queuePriorities[] = { 1.0f, 1.0f, 1.0f };
    queueFamilyIndices_.resize(QUEUE_MAX, UINT32_MAX);
    queueIndices_.resize(QUEUE_MAX, UINT32_MAX);

    std::vector<VkDeviceQueueCreateInfo> createInfos;
    for (int i = 0; i < QUEUE_MAX; ++i)
    {
      uint32_t maxQueues = 0;
      const auto familyIndex = PhysicalDevice::GetQueueFamilyIndex(physicalDevice, queueFlags[i], maxQueues);
      queueFamilyIndices_[i] = familyIndex;
      //search if queue already exists
      bool newFamily = true;
      for (auto& createInfo : createInfos)
      {
        if (createInfo.queueFamilyIndex == familyIndex)
        {
          if (createInfo.queueCount < maxQueues)
          {
            queueIndices_[i] = createInfo.queueCount;
            createInfo.queueCount++;
          }
          else
          {
            queueIndices_[i] = createInfo.queueCount - 1;
          }
          newFamily = false;
        }
      }

      //add create info for queue family
      if (newFamily)
      {
        VkDeviceQueueCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        createInfo.queueCount = 1;
        createInfo.pQueuePriorities = queuePriorities;
        createInfo.queueFamilyIndex = familyIndex;
        createInfos.push_back(createInfo);
        queueIndices_[i] = 0;
      }
    }

    return createInfos;
  }

  uint32_t QueueManager::GetQueueFamilyIndex(Queues type)
  {
    if (!queueFamilyIndices_.empty())
    {
      return queueFamilyIndices_[type];
    }
    else
    {
      printf("Queue indices not initialized\n");
      return UINT32_MAX;
    }
  }

  bool QueueManager::Create(Instance* instance)
  {
    if (instance == nullptr)
    {
      printf("Instance needs to be created before QueueManager\n");
      return false;
    }

    queues_.resize(queueIndices_.size());
    if (queues_.empty())
    {
      printf("No queue indices found, aquiring queue info\n");
      GetQueueInfo(instance->GetPhysicalDevice());
    }

    const auto& device = instance->GetDevice();
    std::map<std::pair<int,int>, VkQueue> createdQueues;
    for (size_t i = 0; i < queues_.size(); ++i)
    {
      VkQueue queue = VK_NULL_HANDLE;
      //check if this combination already was created
      const auto it = createdQueues.find({ queueFamilyIndices_[i], queueIndices_[i] });
      if (it != createdQueues.end())
      {
        queues_[i] = it->second;
      }
      else
      {
        vkGetDeviceQueue(device, queueFamilyIndices_[i], queueIndices_[i], &queue);
        if (queue == VK_NULL_HANDLE)
        {
          printf("Unable to get queue for family %d and index %d\n", queueFamilyIndices_[i], queueIndices_[i]);
          return false;
        }
        queues_[i] = queue;
        createdQueues[{queueFamilyIndices_[i], queueIndices_[i]}] = queue;
      }
    }
    return true;
  }
}