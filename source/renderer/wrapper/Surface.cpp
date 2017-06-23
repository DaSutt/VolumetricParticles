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

#include "Surface.h"

#include <assert.h>
#include <algorithm>

#include "..\resources\QueueManager.h"
#include "..\wrapper\Instance.h"

#include "..\..\utility\Status.h"

namespace Renderer
{
  bool Surface::Create(HWND hwnd, Instance* instance, int width, int height)
  {
    if (instance == nullptr)
    {
      printf("Instance needs to be created before Surface\n");
      return false;
    }

    const auto queueIndex = QueueManager::GetQueueFamilyIndex(QueueManager::QUEUE_GRAPHICS);

    if (
      !CreateSurface(hwnd, instance->GetInstance(), instance->GetPhysicalDevice(), queueIndex) ||
      !CreateSemaphores(instance->GetDevice())
      )
    {
      printf("Surface creation failed\n");
      return false;
    }
    if (!OnResize(instance, width, height)) { return false; }

    return true;
  }

  bool Surface::OnResize(Instance* instance, uint32_t width, uint32_t height)
  {
    viewPort_.x = 0;
    viewPort_.y = 0;
    viewPort_.width = static_cast<float>(width);
    viewPort_.height = static_cast<float>(height);
    viewPort_.minDepth = 0.0f;
    viewPort_.maxDepth = 1.0f;

    scissor_.offset.x = 0;
    scissor_.offset.y = 0;
    scissor_.extent.width = width;
    scissor_.extent.height = height;

    ReleaseSwapChain(instance->GetDevice());

    const auto queueIndex = QueueManager::GetQueueFamilyIndex(QueueManager::QUEUE_GRAPHICS);

    if(!CreateSwapchain(instance->GetPhysicalDevice(), instance->GetDevice(), queueIndex,
      width, height))
    {
      printf("Swap chain creation failed\n");
      return false;
    }

		Status::UpdateSurfaceSize(width, height);

    return true;
  }

  void Surface::Release(VkInstance instance, VkDevice device)
  {
    vkDestroySemaphore(device, imageAvailableSemaphore_, nullptr);

    ReleaseSwapChain(device);
    vkDestroySurfaceKHR(instance, surface_, nullptr);
  }

  void Surface::GetNextImage(VkDevice device)
  {
    vkAcquireNextImageKHR(device, swapchain_, UINT64_MAX, imageAvailableSemaphore_,
    VK_NULL_HANDLE, &currentBackBuffer_);
  }

  void Surface::Present(QueueManager* queueManager, const std::vector<VkSemaphore>& signalSemaphores)
  {
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
    presentInfo.pWaitSemaphores = signalSemaphores.data();
    VkSwapchainKHR swapchains[] = { swapchain_ };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &currentBackBuffer_;

    const auto& queue = queueManager->GetQueue(QueueManager::QUEUE_GRAPHICS);

    vkQueuePresentKHR(queue, &presentInfo);
  }

  void Surface::BindViewportScissor(VkCommandBuffer commandBuffer)
  {
    vkCmdSetViewport(commandBuffer, 0, 1, &viewPort_);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor_);
  }

  bool Surface::CreateSurface(HWND hwnd, VkInstance instance, VkPhysicalDevice physicalDevice, uint32_t queueIndex)
  {
    const auto presentSupported = vkGetPhysicalDeviceWin32PresentationSupportKHR(
      physicalDevice, queueIndex);

    if (presentSupported)
    {
      VkWin32SurfaceCreateInfoKHR surfaceInfo = {};
      surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
      surfaceInfo.hinstance = GetModuleHandle(NULL);
      surfaceInfo.hwnd = hwnd;

      const auto result = vkCreateWin32SurfaceKHR(instance, &surfaceInfo, nullptr, &surface_);
      if (result != VK_SUCCESS)
      {
        printf("Failed creating win32 surface, %d\n", result);
      }
    }
    else
    {
      printf("Present not supported for queue family\n");
    }
    return surface_ != VK_NULL_HANDLE;
  }

  namespace
  {
    struct SurfaceCapabilities
    {
      VkExtent2D extent;
      uint32_t imageCount = 0;
      VkSurfaceTransformFlagBitsKHR transform;
    };

    SurfaceCapabilities GetSwapchainSurfaceCapabilities(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, const SurfaceCapabilities& targetCapabilities)
    {
      VkSurfaceCapabilitiesKHR surfaceCapabilities;
      auto result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);
      assert(result == VK_SUCCESS);

      SurfaceCapabilities capabilities;
      capabilities.extent.width = (std::max)(surfaceCapabilities.minImageExtent.width, (std::min)(surfaceCapabilities.maxImageExtent.width, targetCapabilities.extent.width));
      capabilities.extent.height = (std::max)(surfaceCapabilities.minImageExtent.height, (std::min)(surfaceCapabilities.maxImageExtent.height, targetCapabilities.extent.height));

      if (capabilities.extent.width != surfaceCapabilities.currentExtent.width)
      {
        capabilities.extent.width = surfaceCapabilities.currentExtent.width;
      }
      if (capabilities.extent.height != surfaceCapabilities.currentExtent.height)
      {
        capabilities.extent.height = surfaceCapabilities.currentExtent.height;
      }

      capabilities.imageCount = (std::max)(surfaceCapabilities.minImageCount, (std::min)(surfaceCapabilities.maxImageCount, targetCapabilities.imageCount));
      capabilities.transform = (surfaceCapabilities.currentTransform == VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : surfaceCapabilities.currentTransform;

      return capabilities;
    }

    void PrintDeviceSurfaceFormats(const std::vector<VkSurfaceFormatKHR>& formats)
    {
      printf("Supported formats:\n");
      for (const auto& f : formats)
      {
        printf("%d\n", f.format);
      }
    }

    VkSurfaceFormatKHR GetSwapChainImageFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, std::vector<VkFormat> targetFormats)
    {
      uint32_t formatCount = 0;
      auto result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
      assert(result == VK_SUCCESS);
      if (result == VK_SUCCESS)
      {
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());
        assert(result == VK_SUCCESS);

        //search for desired format
        for (const auto& target : targetFormats)
        {
          for (const auto& f : formats)
          {
            if (f.format == target && f.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
            {
              return f;
            }
          }
        }

        printf("Target formats not found\n");
        PrintDeviceSurfaceFormats(formats);
      }

      return VkSurfaceFormatKHR();
    }

    void PrintDevicePresentModes(const std::vector<VkPresentModeKHR>& presentModes)
    {
      printf("Supported present modes:\n");
      for (const auto& p : presentModes)
      {
        printf("%d\n", p);
      }
    }

    VkPresentModeKHR GetSwapchainPresentMode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, const std::vector<VkPresentModeKHR>& targetPresentModes)
    {
      uint32_t presentModeCount = 0;
      auto result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
      assert(result == VK_SUCCESS);
      if (result == VK_SUCCESS)
      {
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());
        assert(result == VK_SUCCESS);

        for (const auto& target : targetPresentModes)
        {
          for (const auto& pm : presentModes)
          {
            if (pm == target)
            {
              return pm;
            }
          }
        }
        printf("Target presentmode not found return first\n");
        PrintDevicePresentModes(presentModes);
        if (!presentModes.empty())
        {
          return presentModes.front();
        }
      }
      return VkPresentModeKHR();
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

  bool Surface::CreateSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, uint32_t queueIndex, int width, int height)
  {
    VkBool32 presentSupported = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueIndex, surface_, &presentSupported);
    if (!presentSupported)
    {
      printf("Present not supported\n");
      return false;
    }

    SurfaceCapabilities targetCapabilities;
    surfaceSize_.width = width;
    surfaceSize_.height = height;
    targetCapabilities.extent = surfaceSize_;
    targetCapabilities.imageCount = swapchainImageCount_;

    const auto capabilities = GetSwapchainSurfaceCapabilities(physicalDevice, surface_, targetCapabilities);
    surfaceSize_ = capabilities.extent;
    swapchainImageCount_ = capabilities.imageCount;
    const auto format = GetSwapChainImageFormat(physicalDevice, surface_, { VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM });
    swapchainImageFormat_ = format.format;
    const auto presentMode = GetSwapchainPresentMode(physicalDevice, surface_, { VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_RELAXED_KHR });

    VkSwapchainCreateInfoKHR swapChainInfo = {};
    swapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainInfo.surface = surface_;
    swapChainInfo.minImageCount = capabilities.imageCount;
    swapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapChainInfo.preTransform = capabilities.transform;
    swapChainInfo.imageFormat = format.format;
    swapChainInfo.imageColorSpace = format.colorSpace;
    swapChainInfo.imageExtent = capabilities.extent;
    swapChainInfo.imageArrayLayers = 1; //no array layers
    swapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapChainInfo.presentMode = presentMode;
    swapChainInfo.clipped = VK_TRUE;

    auto result = vkCreateSwapchainKHR(device, &swapChainInfo, nullptr, &swapchain_);
    assert(result == VK_SUCCESS);
    if (result != VK_SUCCESS)
    {
      printf("Failed creating swap chain, %d\n", result);
      return false;
    }

    result = vkGetSwapchainImagesKHR(device, swapchain_, &swapchainImageCount_, nullptr);
    assert(result == VK_SUCCESS);
    if (result != VK_SUCCESS)
    {
      printf("Failed getting swap chain image Count, %d\n", result);
      return false;
    }

    swapchainImages_.resize(swapchainImageCount_);
    result = vkGetSwapchainImagesKHR(device, swapchain_, &swapchainImageCount_, swapchainImages_.data());
    assert(result == VK_SUCCESS);
    if (result != VK_SUCCESS)
    {
      printf("Failed getting swap chain images, %d\n", result);
      return false;
    }

    return CreateSwapChainViews(device);
  }

  bool Surface::CreateSwapChainViews(VkDevice device)
  {
    swapchainImageViews_.resize(swapchainImageCount_);
    for (size_t i = 0; i < swapchainImages_.size(); ++i)
    {
      VkImageViewCreateInfo imageViewInfo = {};
      imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      imageViewInfo.image = swapchainImages_[i];
      imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      imageViewInfo.format = swapchainImageFormat_;
      imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      imageViewInfo.subresourceRange.baseMipLevel = 0;
      imageViewInfo.subresourceRange.levelCount = 1;
      imageViewInfo.subresourceRange.baseArrayLayer = 0;
      imageViewInfo.subresourceRange.layerCount = 1;

      const auto result = vkCreateImageView(device, &imageViewInfo, nullptr, &swapchainImageViews_[i]);
      assert(result == VK_SUCCESS);
      if (result != VK_SUCCESS)
      {
        printf("Failed creating swap chain image view, %d\n", result);
        return false;
      }
    }
    return true;
  }

  bool Surface::CreateSemaphores(VkDevice device)
  {
    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    const auto result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore_);
    if (result != VK_SUCCESS)
    {
      printf("Failed creating image available semaphore, %d\n", result);
    }
    return imageAvailableSemaphore_ != VK_NULL_HANDLE;
  }

  void Surface::ReleaseSwapChain(VkDevice device)
  {
    if (swapchain_ != VK_NULL_HANDLE)
    {
      for (auto& iv : swapchainImageViews_)
      {
        vkDestroyImageView(device, iv, nullptr);
      }

      vkDestroySwapchainKHR(device, swapchain_, nullptr);
    }
  }
}