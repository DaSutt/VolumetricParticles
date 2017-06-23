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

#include "Instance.h"
#include <vector>
#include <assert.h>
#include <iostream>
#include <sstream>

#include "..\resources\QueueManager.h"

namespace Renderer
{
  bool Instance::Create()
  {
    if (
      !CreateInstance() ||
#ifdef _DEBUG
      !CreateDebugCallback() ||
#endif
      !SelectPhysicalDevice() ||
      !CreateDevice()
      )
    {
      printf("Instance creation failed\n");
      return false;
    }
    return true;
  }

  void Instance::Release()
  {
    vkDestroyDevice(device_, nullptr);
#ifdef _DEBUG
    DestroyDebugCallback();
#endif
    vkDestroyInstance(instance_, nullptr);
  }

  //Helper functions for instance layers and extensions
  namespace
  {
    std::vector<const char*> GetInstanceLayers()
    {
      std::vector<const char*> layers;

      //check that standard validation is supported
#ifdef _DEBUG
      uint32_t layerCount = 0;
      auto result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
      assert(result == VK_SUCCESS);

      std::vector<VkLayerProperties> layerProperties(layerCount);
      result = vkEnumerateInstanceLayerProperties(&layerCount, layerProperties.data());
      assert(result == VK_SUCCESS);

      for (auto& layer : layerProperties)
      {
        if (strcmp(layer.layerName, "VK_LAYER_LUNARG_standard_validation") == 0)
        {
          layers.push_back("VK_LAYER_LUNARG_standard_validation");
        }
      }
#endif
      return layers;
    }

    std::vector<const char*> GetInstanceExtensions()
    {
      std::vector<const char*> extensions =
      {
        "VK_KHR_surface", "VK_KHR_win32_surface"
      };

#ifdef _DEBUG
      uint32_t extensionCount = 0;
      auto result = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
      assert(result == VK_SUCCESS);

      std::vector<VkExtensionProperties> extensionProperties(extensionCount);
      result = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensionProperties.data());
      assert(result == VK_SUCCESS);

      for (auto& extension : extensionProperties)
      {
        if (strcmp(extension.extensionName, "VK_EXT_debug_report") == 0)
        {
          extensions.push_back("VK_EXT_debug_report");
        }
      }

#endif // _DEBUG

      return extensions;
    }
  }

  bool Instance::CreateInstance()
  {
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Particle-Project";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 0);
    appInfo.pEngineName = "Particle-Project Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    auto layers = GetInstanceLayers();
    instanceInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
    instanceInfo.ppEnabledLayerNames = layers.data();
    auto extensions = GetInstanceExtensions();
    instanceInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    instanceInfo.ppEnabledExtensionNames = extensions.data();

    auto result = vkCreateInstance(&instanceInfo, nullptr, &instance_);
    if (result != VK_SUCCESS)
    {
      printf("Create Instance failed, %d\n", result);
    }

    return instance_ != VK_NULL_HANDLE;
  }

  //Helper functions for physical device
  namespace
  {
    bool CheckPhyscialDeviceRequirements(VkPhysicalDevice physicalDevice)
    {
      //TODO: check requirements
      return true;
    }
  }

  bool Instance::SelectPhysicalDevice()
  {
    uint32_t physicalDeviceCount = 0;
    auto result = vkEnumeratePhysicalDevices(instance_, &physicalDeviceCount, nullptr);
    if (result != VK_SUCCESS)
    {
      printf("Failed getting number of physical devices, %d\n", result);
      return false;
    }

    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    result = vkEnumeratePhysicalDevices(instance_, &physicalDeviceCount, physicalDevices.data());
    if (result != VK_SUCCESS)
    {
      printf("Failed getting physical devices, %d\n", result);
      return false;
    }

    if (!physicalDevices.empty())
    {
      std::vector<int> suitableDeviceIndex;
      printf("Physical devices:\n");
      for (int i = 0; i < physicalDevices.size(); ++i)
      {
        VkPhysicalDeviceProperties physicalDeviceProperties;
        vkGetPhysicalDeviceProperties(physicalDevices[i], &physicalDeviceProperties);

        if (CheckPhyscialDeviceRequirements(physicalDevices[i]))
        {
          printf("%d %s\n", i, physicalDeviceProperties.deviceName);
          suitableDeviceIndex.push_back(i);
        }
      }

      if (suitableDeviceIndex.size() > 1)
      {
        printf("Select device:\n");
        int deviceIndex = 0;
        std::cin >> deviceIndex;
        if (std::find(suitableDeviceIndex.begin(), suitableDeviceIndex.end(), deviceIndex) != suitableDeviceIndex.end())
        {
          physicalDevice_ = physicalDevices[deviceIndex];
        }
        else
        {
          printf("No suitable device selected using first\n");
          physicalDevice_ = physicalDevices[suitableDeviceIndex[0]];
        }
      }
      else if (suitableDeviceIndex.size() == 1)
      {
        physicalDevice_ = physicalDevices[suitableDeviceIndex[0]];
      }
      else
      {
        printf("No suitable device found. aborting\n");
      }
    }

    return physicalDevice_ != VK_NULL_HANDLE;
  }

  bool Instance::CreateDevice()
  {
    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    auto createInfos = QueueManager::GetQueueInfo(physicalDevice_);
    deviceInfo.pQueueCreateInfos = createInfos.data();
    deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(createInfos.size());

    std::vector<const char*> extensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    deviceInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    deviceInfo.ppEnabledExtensionNames = extensions.data();

    //TODO check if all requirements are met
    VkPhysicalDeviceFeatures physicalDeviceFeatures = {};
    physicalDeviceFeatures.geometryShader = VK_TRUE;
    physicalDeviceFeatures.shaderStorageImageExtendedFormats = VK_TRUE;
    physicalDeviceFeatures.fillModeNonSolid = VK_TRUE;
    deviceInfo.pEnabledFeatures = &physicalDeviceFeatures;

    auto result = vkCreateDevice(physicalDevice_, &deviceInfo, nullptr, &device_);
    assert(result == VK_SUCCESS);
    if (result != VK_SUCCESS)
    {
      printf("Failed creating device, %d\n", result);
      return false;
    }

    return device_ != VK_NULL_HANDLE;
  }

#ifdef _DEBUG

  VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugReportFlagsEXT       flags,
    VkDebugReportObjectTypeEXT  objectType,
    uint64_t                    object,
    size_t                      location,
    int32_t                     messageCode,
    const char*                 pLayerPrefix,
    const char*                 pMessage,
    void*                       pUserData)
  {
    std::stringstream out;
    out << "Vulkan ";
    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
    {
      out << "Error";
    }
    else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
    {
      out << "Performance Warning";
    }
    else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
    {
      out << "Warning";
    }
    else if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
    {
      out << "Information";
    }

    out << ": " << pMessage << "\n";
    std::cerr << out.str();

    return VK_FALSE;
  }

  bool Instance::CreateDebugCallback()
  {
    VkDebugReportCallbackCreateInfoEXT debugCallbackInfo = {};
    debugCallbackInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    //TODO removed because of MeshPass warning
    debugCallbackInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | //VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
      VK_DEBUG_REPORT_WARNING_BIT_EXT;
    debugCallbackInfo.pfnCallback = DebugCallback;

    const auto createDebugCalbackFnc = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance_, "vkCreateDebugReportCallbackEXT"));
    if (createDebugCalbackFnc)
    {
      const auto result = createDebugCalbackFnc(instance_, &debugCallbackInfo, nullptr, &callback_);
      if (result != VK_SUCCESS)
      {
        printf("Failed creating debug callback, %d\n", result);
      }
    }

    return callback_ != VK_NULL_HANDLE;
  }

  void Instance::DestroyDebugCallback()
  {
    const auto destroyDebugCallbackFnc = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance_, "vkDestroyDebugReportCallbackEXT"));
    if (destroyDebugCallbackFnc)
    {
      destroyDebugCallbackFnc(instance_, callback_, nullptr);
    }
  }

#endif
}