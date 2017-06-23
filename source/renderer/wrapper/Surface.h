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

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan\vulkan.h>
#include <vector>
#include <Windows.h>

namespace Renderer
{
  class Instance;
  class QueueManager;

  class Surface
  {
  public:
    bool Create(HWND hwnd, Instance* instance, int width, int height);
    void Release(VkInstance instance, VkDevice device);

    void GetNextImage(VkDevice device);
    void Present(QueueManager* queueManager, const std::vector<VkSemaphore>& signalSemaphores);

    VkFormat GetImageFormat() const { return swapchainImageFormat_; }
    uint32_t GetImageCount() const { return swapchainImageCount_; }
    const VkExtent2D& GetSurfaceSize()const { return surfaceSize_; }

    const VkImageView GetImageView(int index) const { return swapchainImageViews_[index]; }
    VkSemaphore* GetImageAvailableSemaphore() { return &imageAvailableSemaphore_; }

    void BindViewportScissor(VkCommandBuffer commandBuffer);
    bool OnResize(Instance* instance, uint32_t width, uint32_t height);
  private:
    bool CreateSurface(HWND hwnd, VkInstance instance, VkPhysicalDevice physicalDevice, uint32_t queueIndex);
    bool CreateSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, uint32_t queueIndex, int width, int height);
    bool CreateSwapChainViews(VkDevice device);

    bool CreateSemaphores(VkDevice device);

    void ReleaseSwapChain(VkDevice device);

    VkSurfaceKHR    surface_    = VK_NULL_HANDLE;
    VkSwapchainKHR  swapchain_  = VK_NULL_HANDLE;

    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;

    VkSemaphore     imageAvailableSemaphore_ = VK_NULL_HANDLE;

    VkExtent2D surfaceSize_;
    uint32_t swapchainImageCount_ = 3;
    VkFormat swapchainImageFormat_;
    uint32_t currentBackBuffer_ = 0;

    VkViewport viewPort_;
    VkRect2D scissor_;
  };
}