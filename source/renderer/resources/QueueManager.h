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

namespace Renderer
{
  class Instance;

  class QueueManager
  {
  public:
    enum Queues
    {
      QUEUE_GRAPHICS,
      QUEUE_COMPUTE,
      QUEUE_TRANSFER,
      QUEUE_MAX
    };

    static std::vector<VkDeviceQueueCreateInfo> GetQueueInfo(VkPhysicalDevice physicalDevice);
    static uint32_t GetQueueFamilyIndex(Queues type);

    bool Create(Instance* instance);
    VkQueue GetQueue(Queues type) { return queues_[type]; }
  private:
    static const std::vector<VkQueueFlagBits> queueFlags;
    static std::vector<uint32_t> queueFamilyIndices_;
    static std::vector<uint32_t> queueIndices_;

    std::vector<VkQueue> queues_;
  };
}