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
#include <memory>
#include <map>

#include "..\wrapper\MemoryAllocator.h"

namespace Renderer
{
  class Instance;
  class Surface;
  class QueueManager;
  class BufferManager;

  class ImageManager
  {
  public:
    enum Sampler
    {
      SAMPLER_LINEAR,
      SAMPLER_GRID,
			SAMPLER_SHADOW,
      SAMPLER_MAX,
      SAMPLER_NONE
    };

    enum Type
    {
      IMAGE_RESIZE,
      IMAGE_IMMUTABLE,
      IMAGE_NO_POOL,
      IMAGE_MAX
    };

		enum BarrierType
		{
			BARRIER_WRITE_READ,
			BARRIER_WRITE_WRITE,
			BARRIER_READ_WRITE,
			BARRIER_MAX
		};

		struct BarrierInfo
		{
			BarrierType type;
			uint32_t srcQueueFamilyIndex;
			uint32_t dstQueueFamilyIndex;
			int imageIndex;
			BarrierInfo();
		};

    struct InitData
    {
      VkDeviceSize size;
      void* data;
      InitData();
    };

    struct ImageInfo
    {
      VkExtent3D extent;
      VkFormat format;
      VkImageUsageFlags usage;
      Sampler sampler;
      VkImageLayout layout;
      VkImageLayout viewLayout;
			VkImageLayout initialLayout;
      InitData initData;
      Type type;
      int arrayCount;
      ImageInfo();
    };

    ImageManager(Instance* instance);

    int RequestImage(const ImageInfo& imageInfo);


    bool Create();
    bool InitializeImages(QueueManager* queueManager, bool resize);
    bool InitData(QueueManager* queueManager, VkDevice device, VkPhysicalDevice physicalDevice);
    bool OnResize(QueueManager* queueManager, uint32_t width, uint32_t height);
    void Release(VkDevice device);

    std::vector<VkImageMemoryBarrier> ReadToWriteBarrier(std::vector<int> indices);
		std::vector<VkImageMemoryBarrier> Barrier(const std::vector<BarrierInfo>& barrierInfos);

    const VkImage& GetImage(int index) { return images_[index]; }
    const VkImageView& GetImageView(int index) { return imageViews_[index]; }
    const VkDescriptorImageInfo& GetDescriptorInfo(int index) const { return descriptorInfos_[index]; }
		const auto& GetImageInfo(int index) const { return imageInfos_[index]; }
		const auto& Ref_GetImageInfo(int index) const { return Ref_imageInfos_[index]; }
		const VkDeviceSize GetImageSize(int index) const;
		const VkDeviceSize Ref_GetImageSize(int index) const;

    const VkFormat GetOffscreenImageFormat() const { return VK_FORMAT_R16G16B16A16_SFLOAT; }
    const VkFormat GetDepthImageFormat() const { return VK_FORMAT_D16_UNORM; }

		void CopyImageToBuffer(QueueManager* queueManager, VkBuffer buffer, const std::vector<VkBufferImageCopy>& copyRegions, 
			VkImageLayout layout, int imageIndex, bool refactoring);
		
		struct ImageCopyInfo
		{
			int srcIndex;
			VkImageLayout srcLayout;
			int dstIndex;
			VkImageLayout dstLayout;
			uint32_t regionCount;
			VkImageCopy* pRegions;
			ImageCopyInfo();
			void AddCopyRegions(std::vector<VkImageCopy>& regions);
		};

		void CopyImage(VkCommandBuffer cmdBuffer, const ImageCopyInfo& copyInfo);

    //Refactoring
    enum ImageTypeBits
    {
      IMAGE_FILE = 1 << 0,
      IMAGE_GRID = 1 << 1,
    };

    enum ImageMemoryPool
    {
      MEMORY_POOL_SCENE,
      MEMORY_POOL_GRID,
			MEMORY_POOL_GRID_MIPMAP,
      MEMORY_POOL_CONSTANT,
			MEMORY_POOL_INDIVIDUAL,
      MEMORY_POOL_MAX
    };

    struct Ref_ImageInfo
    {
      uint32_t imageType;
      ImageMemoryPool pool;
      VkExtent3D extent;
      VkFormat format;
      VkImageUsageFlags usage;
      VkImageLayout layout;
			VkImageLayout initialLayout;
      int arrayCount;
      bool viewPerLayer;
      Sampler sampler;
      Ref_ImageInfo();
    };

    int Ref_RequestImage(const Ref_ImageInfo& imageInfo, std::string fileName = "");
    bool Ref_Create();
    void Ref_ReleaseScene();
    void Ref_OnLoadScene(BufferManager* bufferManager, QueueManager* queueManager);
    void Ref_Release(VkDevice device);
    void Ref_UploadImages(BufferManager* bufferManager, QueueManager* queueManager);
    void Ref_ResizeImages(int index, VkDeviceSize newSize, ImageMemoryPool pool);
    void Ref_ClearImage(VkCommandBuffer commandBuffer, int index, VkClearColorValue clearColor);

    const VkDescriptorImageInfo& Ref_GetDescriptorInfo(int index) const;
    const VkImageView& Ref_GetImageView(int index) const { return Ref_imageViews_[index]; }
    const VkImageView& Ref_GetArrayImageView(int imageIndex, int arrayOffset) const
      { return Ref_arrayImageViews_.at(imageIndex)[arrayOffset]; }
  private:
    struct Ref_CreateInfo
    {
      int imageIndex;
      std::vector<unsigned char*> data;
      std::vector<VkDeviceSize> allocatedSize;
    };

    VkImage Ref_CreateImage(const Ref_ImageInfo& info, int index);
    VkImage Ref_CreateImageObject(VkDevice device, const Ref_ImageInfo& info);
    VkImageView Ref_CreateImageView(VkDevice device, VkImage image, const Ref_ImageInfo& info);
    VkImageView Ref_CreateImageView(VkDevice device, VkImage image, const Ref_ImageInfo& info,
      uint32_t baseArrayLayer, uint32_t layerCount);
    void Ref_CopyDstTransition(QueueManager* queueManager);
    int Ref_GetChannelCount(VkFormat format);

    std::array<std::unique_ptr<Wrapper::MemoryAllocator>, MEMORY_POOL_MAX> Ref_memoryPools_;
    std::vector<Ref_ImageInfo> Ref_imageInfos_;
    std::vector<VkImage> Ref_images_;
    std::vector<VkImageView> Ref_imageViews_;
    std::map<int, std::vector<VkImageView>> Ref_arrayImageViews_;
    std::vector<Ref_CreateInfo> Ref_imageCreateData_;
    std::vector<VkDescriptorImageInfo> Ref_descriptorInfos_;

    std::map<std::string, int> sceneTextureMapping_;
    bool sceneLoaded_ = false;

    //end refactoring

    bool CreateSamplers(VkDevice device);
    bool CreateCommandBuffers(VkDevice device);
    void ReleaseImage(VkDevice device, int index);

    VkImageMemoryBarrier InitTransition(int index);

    bool CreateImage(const ImageInfo& imageInfo, int index);
    VkImage CreateImageObject(VkDevice device, const ImageInfo& info);
    VkImageView CreateImageView(VkDevice device, VkImage image, const ImageInfo& info);
    VkDescriptorImageInfo FillDescriptorInfo(const ImageInfo& info, VkImageView view);

    std::vector<ImageInfo> imageInfos_;
    std::vector<VkImage> images_;
    std::vector<VkImageView> imageViews_;
    std::vector<VkDescriptorImageInfo> descriptorInfos_;

    Instance* instance_;
    std::unique_ptr<Wrapper::MemoryAllocator> mutableMemory_;
    std::unique_ptr<Wrapper::MemoryAllocator> immutableMemory_;
    std::vector<Wrapper::MemoryAllocator> nonPooledMemory_;

    std::array<VkSampler, SAMPLER_MAX> samplers_;

    VkCommandPool transitionCommandPool_;
    VkCommandPool transferCommandPool_;
    VkCommandBuffer transitionCommandBuffer_;
    VkCommandBuffer transferCommandBuffer_;

    VkFence transitionFence_;
    VkFence transferFence_;

  };
}