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

#include "ImageManager.h"

#include <iostream>

#include "..\wrapper\PhysicalDevice.h"
#include "..\wrapper\Instance.h"
#include "..\wrapper\Surface.h"
#include "..\wrapper\CommandBuffer.h"
#include "..\wrapper\Synchronization.h"

#include "QueueManager.h"
#include "BufferManager.h"

#include <assert.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include <stb\stb_image.h>
#include <string>

namespace Renderer
{
  ImageManager::InitData::InitData() :
    size{0}, data{nullptr}
  {}

	ImageManager::BarrierInfo::BarrierInfo() :
		type{BARRIER_MAX},
		srcQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
		dstQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED},
		imageIndex{-1}
	{}

	ImageManager::ImageCopyInfo::ImageCopyInfo() :
		srcIndex{ -1 },
		srcLayout{ VK_IMAGE_LAYOUT_BEGIN_RANGE },
		dstIndex{ -1 },
		dstLayout{ VK_IMAGE_LAYOUT_BEGIN_RANGE },
		regionCount{ 0 },
		pRegions{ nullptr }
	{}

	void ImageManager::ImageCopyInfo::AddCopyRegions(std::vector<VkImageCopy>& regions)
	{
		regionCount = static_cast<uint32_t>(regions.size());
		pRegions = regions.data();
	}

  ImageManager::ImageInfo::ImageInfo() :
    extent{0,0,0},
    format{VK_FORMAT_MAX_ENUM},
    usage{VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM},
    sampler{SAMPLER_NONE},
    layout{VK_IMAGE_LAYOUT_GENERAL},
    viewLayout{VK_IMAGE_LAYOUT_GENERAL},
		initialLayout{VK_IMAGE_LAYOUT_MAX_ENUM},
    type{ IMAGE_MAX },
    arrayCount{ 1 },
    initData{}
  {}

  ImageManager::ImageManager(Instance* instance) :
    instance_{instance}
  {
    mutableMemory_ = std::make_unique<Wrapper::MemoryAllocator>(256);
    immutableMemory_ = std::make_unique<Wrapper::MemoryAllocator>(256);
    Ref_memoryPools_[MEMORY_POOL_SCENE] = std::make_unique<Wrapper::MemoryAllocator>(256);
    Ref_memoryPools_[MEMORY_POOL_GRID] = std::make_unique<Wrapper::MemoryAllocator>(256);
    Ref_memoryPools_[MEMORY_POOL_CONSTANT] = std::make_unique<Wrapper::MemoryAllocator>(64);
		Ref_memoryPools_[MEMORY_POOL_INDIVIDUAL] = std::make_unique<Wrapper::MemoryAllocator>();
		Ref_memoryPools_[MEMORY_POOL_GRID_MIPMAP] = std::make_unique<Wrapper::MemoryAllocator>();
  }

  int ImageManager::RequestImage(const ImageInfo& imageInfo)
  {
    const auto index = static_cast<int>(imageInfos_.size());

    images_.push_back(VK_NULL_HANDLE);
    imageViews_.push_back(VK_NULL_HANDLE);
    descriptorInfos_.push_back({});

    if (!CreateImage(imageInfo, index))
    {
      printf("Image creation failed\n");
      return -1;
    }

    imageInfos_.push_back(imageInfo);

    return index;
  }

  bool ImageManager::Create()
  {
    const auto device = instance_->GetDevice();

    if (!CreateSamplers(device)) { return false; }
    if (!CreateCommandBuffers(device)) { return false; }

    if (!Wrapper::CreateFence(device, 0, &transitionFence_)) { return false; }
    if (!Wrapper::CreateFence(device, 0, &transferFence_)) { return false; }

    return true;
  }

  bool ImageManager::InitializeImages(QueueManager* queueManager, bool resize)
  {
    const auto device = instance_->GetDevice();
    const auto physicalDevice = instance_->GetPhysicalDevice();

    Ref_Create();

    vkResetFences(device, 1, &transitionFence_);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    auto result = vkBeginCommandBuffer(transitionCommandBuffer_, &beginInfo);
    if (result != VK_SUCCESS) { return false; }

    std::vector<VkImageMemoryBarrier> imageBarriers;
    for (size_t i = 0; i < imageInfos_.size(); ++i)
    {
      if (!resize || (resize && imageInfos_[i].type == IMAGE_RESIZE))
      {
        imageBarriers.push_back(InitTransition(static_cast<int>(i)));
      }
    }

    vkCmdPipelineBarrier(transitionCommandBuffer_,
      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      0,
      0, nullptr,
      0, nullptr,
      static_cast<uint32_t>(imageBarriers.size()), imageBarriers.data());

    result = vkEndCommandBuffer(transitionCommandBuffer_);
    assert(result == VK_SUCCESS);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &transitionCommandBuffer_;

    const auto& graphicQueue = queueManager->GetQueue(QueueManager::QUEUE_GRAPHICS);
    result = vkQueueSubmit(graphicQueue, 1, &submitInfo, transitionFence_);
    assert(result == VK_SUCCESS);

    vkWaitForFences(device, 1, &transitionFence_, VK_TRUE, UINT64_MAX);

    vkResetCommandPool(device, transitionCommandPool_, 0);

    return true;
  }

  bool ImageManager::OnResize(QueueManager* queueManager, uint32_t width, uint32_t height)
  {
    const auto device = instance_->GetDevice();

    mutableMemory_->Release(device);

    for (size_t index = 0; index < images_.size(); ++index)
    {
      if (imageInfos_[index].type == IMAGE_RESIZE)
      {
        ReleaseImage(device, static_cast<int>(index));
        imageInfos_[index].extent = { width, height, imageInfos_[index].extent.depth };

        if (!CreateImage(imageInfos_[index], static_cast<int>(index)))
        {
          return false;
        }
      }
    }

    return InitializeImages(queueManager, true);
  }

  void ImageManager::Release(VkDevice device)
  {
    vkWaitForFences(device, 1, &transitionFence_, VK_TRUE, UINT64_MAX);
    vkDestroyFence(device, transitionFence_, nullptr);
    vkWaitForFences(device, 1, &transferFence_, VK_TRUE, UINT64_MAX);
    vkDestroyFence(device, transferFence_, nullptr);

    Ref_Release(device);

    mutableMemory_->Release(device);
    immutableMemory_->Release(device);

    for (auto& memory : nonPooledMemory_)
    {
      memory.Release(device);
    }

    for (auto& sampler : samplers_)
    {
      vkDestroySampler(device, sampler, nullptr);
    }

    for (size_t i = 0; i < images_.size(); ++i)
    {
      ReleaseImage(device, static_cast<int>(i));
    }

    vkDestroyCommandPool(device, transitionCommandPool_, nullptr);
    vkDestroyCommandPool(device, transferCommandPool_, nullptr);
  }

  std::vector<VkImageMemoryBarrier> ImageManager::ReadToWriteBarrier(std::vector<int> indices)
  {
    std::vector<VkImageMemoryBarrier> barriers;
    for (auto index : indices)
    {
      VkImageMemoryBarrier barrier = {};
      barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
      if (imageInfos_[index].layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
      {
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      }
      else if (imageInfos_[index].layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
      {
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      }
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.image = images_[index];
      barrier.subresourceRange.baseMipLevel = 0;
      barrier.subresourceRange.levelCount = 1;
      barrier.subresourceRange.baseArrayLayer = 0;
      barrier.subresourceRange.layerCount = 1;
      barriers.push_back(barrier);
    }
    return barriers;
  }

	namespace
	{
		struct AccessMasks
		{
			VkAccessFlags src;
			VkAccessFlags dst;
		};

		AccessMasks GetAccessMasks(ImageManager::BarrierType type)
		{
			switch (type)
			{
			case ImageManager::BARRIER_WRITE_READ:
				return { VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT };
			case ImageManager::BARRIER_WRITE_WRITE:
				return { VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT };
			case ImageManager::BARRIER_READ_WRITE:
				return { VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT };
			default:
				return {};
			}
		}

		struct ImageLayouts
		{
			VkImageLayout oldLayout;
			VkImageLayout newLayout;
		};

		ImageLayouts GetImageLayouts(ImageManager::BarrierType type)
		{
			switch (type)
			{
			case ImageManager::BARRIER_WRITE_READ:
				return { VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
			case ImageManager::BARRIER_WRITE_WRITE:
				return { VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL };
			case ImageManager::BARRIER_READ_WRITE:
				return { VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL };
			default:
				return {};
			}
		}
	}

	std::vector<VkImageMemoryBarrier> ImageManager::Barrier(const std::vector<BarrierInfo>& barrierInfos)
	{
		std::vector<VkImageMemoryBarrier> barriers;
		for (const auto info : barrierInfos)
		{
			const int imageIndex = info.imageIndex;

			VkImageMemoryBarrier barrier = {};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			
			const auto accessMasks = GetAccessMasks(info.type);
			barrier.srcAccessMask = accessMasks.src;
			barrier.dstAccessMask = accessMasks.dst;

			const auto layouts = GetImageLayouts(info.type);
			barrier.oldLayout = layouts.oldLayout;
			barrier.newLayout = layouts.newLayout;
			barrier.srcQueueFamilyIndex = info.srcQueueFamilyIndex;
			barrier.dstQueueFamilyIndex = info.dstQueueFamilyIndex;
			barrier.image = Ref_images_[imageIndex];
			if (Ref_imageInfos_[imageIndex].usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
			{
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			}
			else
			{
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			}
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = Ref_imageInfos_[imageIndex].arrayCount;

			barriers.push_back(barrier);
		}
		return barriers;
	}

	const VkDeviceSize ImageManager::GetImageSize(int index) const
	{
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(instance_->GetDevice(), images_[index], &memRequirements);
		return memRequirements.size;
	}

	const VkDeviceSize ImageManager::Ref_GetImageSize(int index) const
	{
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(instance_->GetDevice(), Ref_images_[index], &memRequirements);
		return memRequirements.size;
	}

	void ImageManager::CopyImageToBuffer(QueueManager* queueManager, VkBuffer buffer, 
		const std::vector<VkBufferImageCopy>& copyRegions, VkImageLayout layout, int imageIndex, bool refactoring)
	{
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(transitionCommandBuffer_, &beginInfo);

		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = layout;
		barrier.srcQueueFamilyIndex = queueManager->GetQueueFamilyIndex(QueueManager::QUEUE_GRAPHICS);
		barrier.dstQueueFamilyIndex = queueManager->GetQueueFamilyIndex(QueueManager::QUEUE_GRAPHICS);
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.image = refactoring ? Ref_images_[imageIndex] : images_[imageIndex];
		
		const auto usage = refactoring ? Ref_imageInfos_[imageIndex].usage :
			imageInfos_[imageIndex].usage;
		if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		else
		{
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;

		if (usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		}
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(transitionCommandBuffer_, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
		
		vkCmdCopyImageToBuffer(transitionCommandBuffer_, refactoring ? Ref_images_[imageIndex] : images_[imageIndex],
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 
			static_cast<uint32_t>(copyRegions.size()),
			copyRegions.data());


		VkImageMemoryBarrier copyToDepthBarrier = barrier;
		copyToDepthBarrier.oldLayout = barrier.newLayout;
		copyToDepthBarrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex;
		copyToDepthBarrier.dstQueueFamilyIndex = barrier.srcQueueFamilyIndex;
		copyToDepthBarrier.newLayout = barrier.oldLayout;
		copyToDepthBarrier.dstAccessMask = barrier.srcAccessMask;
		copyToDepthBarrier.srcAccessMask = barrier.dstAccessMask;

		vkCmdPipelineBarrier(transitionCommandBuffer_, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &copyToDepthBarrier);
		vkEndCommandBuffer(transitionCommandBuffer_);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &transitionCommandBuffer_;

		const auto& graphicsQueue = queueManager->GetQueue(QueueManager::QUEUE_GRAPHICS);
		const auto result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, nullptr);
		assert(result == VK_SUCCESS);

		vkDeviceWaitIdle(instance_->GetDevice());
	}

	void ImageManager::CopyImage(VkCommandBuffer cmdBuffer, const ImageCopyInfo& info)
	{
		assert(info.regionCount > 0);
		vkCmdCopyImage(cmdBuffer,
			Ref_images_[info.srcIndex], info.srcLayout,
			Ref_images_[info.dstIndex], info.dstLayout,
			info.regionCount, info.pRegions);
	}

  bool ImageManager::CreateSamplers(VkDevice device)
  {
    auto sampler = SAMPLER_LINEAR;
    {
      VkSamplerCreateInfo info = {};
      info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
      info.magFilter = VK_FILTER_LINEAR;
      info.minFilter = VK_FILTER_LINEAR;
      info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
      info.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
      info.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
      info.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
      info.minLod = -1000;
      info.maxLod = 1000;
      const auto result = vkCreateSampler(device, &info, nullptr, &samplers_[sampler]);
      if (result != VK_SUCCESS)
      {
        printf("Failed creating image sampler, %d\n", result);
        return false;
      }
    }
    sampler = SAMPLER_GRID;
    {
      VkSamplerCreateInfo info = {};
      info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
      info.magFilter = VK_FILTER_LINEAR;
      info.minFilter = VK_FILTER_LINEAR;
      info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
      info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
      info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
      info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
      info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
      info.minLod = -1000;
      info.maxLod = 1000;
      const auto result = vkCreateSampler(device, &info, nullptr, &samplers_[sampler]);
      if (result != VK_SUCCESS)
      {
        printf("Failed creating image sampler, %d\n", result);
        return false;
      }
    }
		sampler = SAMPLER_SHADOW;
		{
			VkSamplerCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			info.magFilter = VK_FILTER_LINEAR;
			info.minFilter = VK_FILTER_LINEAR;
			info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
			info.minLod = -1000;
			info.maxLod = 1000;
			const auto result = vkCreateSampler(device, &info, nullptr, &samplers_[sampler]);
			if (result != VK_SUCCESS)
			{
				printf("Failed creating image sampler, %d\n", result);
				return false;
			}
		}
    return true;
  }

  bool ImageManager::CreateCommandBuffers(VkDevice device)
  {
    if (!Wrapper::CreateCommandPool(device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      QueueManager::GetQueueFamilyIndex(QueueManager::QUEUE_GRAPHICS), &transitionCommandPool_))
    {
      return false;
    }

    if (!Wrapper::AllocateCommandBuffers(device, transitionCommandPool_, 1, &transitionCommandBuffer_))
    {
      return false;
    }

    if (!Wrapper::CreateCommandPool(device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      QueueManager::GetQueueFamilyIndex(QueueManager::QUEUE_TRANSFER), &transferCommandPool_))
    {
      return false;
    }

    if (!Wrapper::AllocateCommandBuffers(device, transferCommandPool_, 1, &transferCommandBuffer_))
    {
      return false;
    }

    return true;
  }

  void ImageManager::ReleaseImage(VkDevice device, int index)
  {
    vkDestroyImageView(device, imageViews_[index], nullptr);
    vkDestroyImage(device, images_[index], nullptr);
  }

  VkImageMemoryBarrier ImageManager::InitTransition(int index)
  {
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.newLayout = imageInfos_[index].initialLayout != VK_IMAGE_LAYOUT_MAX_ENUM ?
			imageInfos_[index].initialLayout : imageInfos_[index].layout;
    barrier.image = images_[index];
    if (imageInfos_[index].layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    {
      barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }
    else if(imageInfos_[index].layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
      barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }
    else
    {
      barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    }
    return barrier;
  }

  bool ImageManager::InitData(QueueManager* queueManager, VkDevice device, VkPhysicalDevice physicalDevice)
  {
    printf("Init data\n");

    vkResetFences(device, 1, &transferFence_);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    auto result = vkBeginCommandBuffer(transitionCommandBuffer_, &beginInfo);
    if (result != VK_SUCCESS) { return false; }

    std::vector<VkBuffer> uploadBuffers(imageInfos_.size(), VK_NULL_HANDLE);
    std::vector<VkDeviceMemory> uploadMemory(imageInfos_.size(), VK_NULL_HANDLE);

    for (size_t i = 0; i < imageInfos_.size(); ++i)
    {
      if (imageInfos_[i].initData.data != nullptr)
      {
        // Create the Upload Buffer:
        {
          VkBufferCreateInfo buffer_info = {};
          buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
          buffer_info.size = imageInfos_[i].initData.size;
          buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
          buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
          auto result = vkCreateBuffer(device, &buffer_info, nullptr, &uploadBuffers[i]);
          if (result != VK_SUCCESS) { return false; }

          VkMemoryRequirements req;
          vkGetBufferMemoryRequirements(device, uploadBuffers[i], &req);
          VkMemoryAllocateInfo alloc_info = {};
          alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
          alloc_info.allocationSize = req.size;
          alloc_info.memoryTypeIndex = PhysicalDevice::GetMemoryType(physicalDevice, req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
          result = vkAllocateMemory(device, &alloc_info, nullptr, &uploadMemory[i]);
          if (result != VK_SUCCESS) { return false; }
          result = vkBindBufferMemory(device, uploadBuffers[i], uploadMemory[i], 0);
          if (result != VK_SUCCESS) { return false; }
        }
        // Upload to Buffer:
        {
          char* map = NULL;
          auto result = vkMapMemory(device, uploadMemory[i], 0, imageInfos_[i].initData.size, 0, (void**)(&map));
          if (result != VK_SUCCESS) { return false; }
          memcpy(map, imageInfos_[i].initData.data, imageInfos_[i].initData.size);

          VkMappedMemoryRange range[1] = {};
          range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
          range[0].memory = uploadMemory[i];
          range[0].size = imageInfos_[i].initData.size;
          result = vkFlushMappedMemoryRanges(device, 1, range);
          if (result != VK_SUCCESS) { return false; }

          vkUnmapMemory(device, uploadMemory[i]);
        }
        // Copy to Image:
        {
          VkImageMemoryBarrier copy_barrier[1] = {};
          copy_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
          copy_barrier[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
          copy_barrier[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
          copy_barrier[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
          copy_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
          copy_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
          copy_barrier[0].image = images_[i];
          copy_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
          copy_barrier[0].subresourceRange.levelCount = 1;
          copy_barrier[0].subresourceRange.layerCount = 1;
          vkCmdPipelineBarrier(transitionCommandBuffer_, VK_PIPELINE_STAGE_HOST_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, copy_barrier);

          VkBufferImageCopy region = {};
          region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
          region.imageSubresource.layerCount = 1;
          region.imageExtent = imageInfos_[i].extent;
          vkCmdCopyBufferToImage(transitionCommandBuffer_, uploadBuffers[i], images_[i],
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

          VkImageMemoryBarrier use_barrier[1] = {};
          use_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
          use_barrier[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
          use_barrier[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
          use_barrier[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
          use_barrier[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
          use_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
          use_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
          use_barrier[0].image = images_[i];
          use_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
          use_barrier[0].subresourceRange.levelCount = 1;
          use_barrier[0].subresourceRange.layerCount = 1;
          vkCmdPipelineBarrier(transitionCommandBuffer_,
			  VK_PIPELINE_STAGE_TRANSFER_BIT, 
			  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 
			  0, 0, NULL, 0, NULL, 1, use_barrier);
        }
        printf("Upload image data\n");
      }
    }

    result = vkEndCommandBuffer(transitionCommandBuffer_);
    assert(result == VK_SUCCESS);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &transitionCommandBuffer_;

    const auto& graphicQueue = queueManager->GetQueue(QueueManager::QUEUE_GRAPHICS);
    result = vkQueueSubmit(graphicQueue, 1, &submitInfo, transferFence_);
    assert(result == VK_SUCCESS);

    vkWaitForFences(device, 1, &transferFence_, VK_TRUE, UINT64_MAX);

    for (size_t i = 0; i < uploadBuffers.size(); ++i)
    {
      if (uploadBuffers[i] != VK_NULL_HANDLE)
      {
        vkFreeMemory(device, uploadMemory[i], nullptr);
        vkDestroyBuffer(device, uploadBuffers[i], nullptr);
      }
    }

    vkResetCommandPool(device, transferCommandPool_, 0);

    return true;
  }

  namespace
  {
    VkImageType GetImageType(const VkExtent3D& extent)
    {
      if (extent.height == 1 && extent.depth == 1)
      {
        return VK_IMAGE_TYPE_1D;
      }
      else if (extent.depth == 1)
      {
        return VK_IMAGE_TYPE_2D;
      }
      else
      {
        return VK_IMAGE_TYPE_3D;
      }
    }
    VkImageViewType GetViewType(const VkExtent3D& extent, int layerCount)
    {
      if (extent.height == 1 && extent.depth == 1)
      {
        if (layerCount > 1)
        {
          return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
        }
        return VK_IMAGE_VIEW_TYPE_1D;
      }
      else if (extent.depth == 1)
      {
        if (layerCount > 1)
        {
          return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        }
        return VK_IMAGE_VIEW_TYPE_2D;
      }
      else
      {
        return VK_IMAGE_VIEW_TYPE_3D;
      }
    }
  }

  bool ImageManager::CreateImage(const ImageInfo& imageInfo, int index)
  {
    const auto device = instance_->GetDevice();
    const auto physicalDevice = instance_->GetPhysicalDevice();

    auto image = CreateImageObject(device, imageInfo);
    if (image == VK_NULL_HANDLE) { return false; }

    switch (imageInfo.type)
    {
    case IMAGE_RESIZE:
      if (!mutableMemory_->BindImage(physicalDevice, device, image)) { return false; }
      break;
    case IMAGE_IMMUTABLE:
      if (!immutableMemory_->BindImage(physicalDevice, device, image)) { return false; }
      break;
    case IMAGE_NO_POOL:
    {
      nonPooledMemory_.push_back({});
      if (!nonPooledMemory_.back().AllocateAndBindImage(physicalDevice, device, image)) { return false; }
    }
      break;
    default:
      printf("Unable to create image with invalid type\n");
      return false;
    }

    auto imageView = CreateImageView(device, image, imageInfo);
    if (imageView == VK_NULL_HANDLE) { return false; }

    images_[index] = image;
    imageViews_[index] = imageView;
    descriptorInfos_[index] = FillDescriptorInfo(imageInfo, imageView);

    return true;
  }

  VkImage ImageManager::CreateImageObject(VkDevice device, const ImageInfo& info)
  {
    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = GetImageType(info.extent);
    imageCreateInfo.extent = info.extent;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.format = info.format;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.usage = info.usage;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkImage image = VK_NULL_HANDLE;
    auto result = vkCreateImage(device, &imageCreateInfo, nullptr, &image);
    if (result != VK_SUCCESS)
    {
      printf("Failed creating image, %d\n", result);
      return VK_NULL_HANDLE;
    }
    return image;
  }

  VkImageView ImageManager::CreateImageView(VkDevice device, VkImage image, const ImageInfo& info)
  {
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = GetViewType(info.extent, 1);
    viewInfo.format = info.format;
    viewInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    if(info.format == VK_FORMAT_D32_SFLOAT_S8_UINT || info.format == VK_FORMAT_D16_UNORM)
    {
      viewInfo.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
    }
    else
    {
      viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    }
    VkImageView imageView = VK_NULL_HANDLE;
    const auto result = vkCreateImageView(device, &viewInfo, nullptr, &imageView);
    if (result != VK_SUCCESS)
    {
      printf("Failed creating image view, %d\n", result);
      return VK_NULL_HANDLE;
    }
    return imageView;
  }

  VkDescriptorImageInfo ImageManager::FillDescriptorInfo(const ImageInfo& info, VkImageView view)
  {
    VkDescriptorImageInfo descriptorImageInfo = {};
    const auto samplerType = info.sampler;
    if (samplerType != SAMPLER_NONE)
    {
      descriptorImageInfo.sampler = samplers_[samplerType];
    }
    descriptorImageInfo.imageView = view;
    descriptorImageInfo.imageLayout = info.viewLayout;
    return descriptorImageInfo;
  }

  ImageManager::Ref_ImageInfo::Ref_ImageInfo() :
    format{ VK_FORMAT_R8G8B8A8_UNORM },
    usage{ VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT },
    arrayCount{ 1 },
    layout{ VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		initialLayout{VK_IMAGE_LAYOUT_MAX_ENUM},
    sampler{Sampler::SAMPLER_LINEAR},
    viewPerLayer{false}
  {

  }

  int ImageManager::Ref_RequestImage(const Ref_ImageInfo& imageInfo, std::string fileName)
  {
    const auto index = static_cast<int>(Ref_imageInfos_.size());
    const auto device = instance_->GetDevice();

    Ref_ImageInfo info = imageInfo;
    if (!fileName.empty() && info.imageType & IMAGE_FILE)
    {
      if (strcmp(fileName.c_str(), "NONE") == 0)
      {
        printf("Material without texture\n");
        return -1;
      }

      size_t fileIndexPos;
      if (imageInfo.arrayCount > 1)
      {
        fileIndexPos = fileName.find("_X_");
        fileName = fileName.replace(fileIndexPos, 3, std::to_string(0));
      }

      Ref_CreateInfo createInfo;
      createInfo.imageIndex = index;
      int replaceCount = 1;
      for (int i = 0; i < imageInfo.arrayCount; ++i)
      {
        int width, height, channelCount = 0;
        int neededChannels = Ref_GetChannelCount(imageInfo.format);
        auto data = stbi_load(fileName.c_str(), &width, &height, &channelCount, neededChannels);
        if (data == nullptr)
        {
          printf("Unable to load texture\n");
          return -1;
        }
        info.extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
        createInfo.data.push_back(data);
        createInfo.allocatedSize.push_back(static_cast<VkDeviceSize>(width * height * neededChannels));

        if (imageInfo.arrayCount > 1)
        {
          if (i == 10)
          {
            ++replaceCount;
          }
          fileName = fileName.replace(fileIndexPos, replaceCount, std::to_string(i + 1));
        }
      }
      Ref_imageCreateData_.push_back(createInfo);
    }

    VkImage image = Ref_CreateImage(info, index);
    if (image == VK_NULL_HANDLE)
    {
      printf("Image creation failed\n");
      return -1;
    }

    Ref_images_.push_back(image);
    Ref_imageInfos_.push_back(info);

    auto imageView = Ref_CreateImageView(device, image, info);
    if (imageView == VK_NULL_HANDLE)
    {
      printf("Image view creation failed\n");
      return -1;
    }
    Ref_imageViews_.push_back(imageView);

    if (info.viewPerLayer)
    {
      for (int i = 0; i < info.arrayCount; ++i)
      {
        Ref_arrayImageViews_[index].push_back(
          Ref_CreateImageView(device, image, info, static_cast<uint32_t>(i), 1));
      }
    }

    VkDescriptorImageInfo descriptorImageInfo = {};
    descriptorImageInfo.sampler = samplers_[info.sampler];
    descriptorImageInfo.imageView = imageView;
    descriptorImageInfo.imageLayout = info.initialLayout != VK_IMAGE_LAYOUT_MAX_ENUM ?
			info.initialLayout : info.layout;
    Ref_descriptorInfos_.push_back(descriptorImageInfo);

    return index;
  }

  bool ImageManager::Ref_Create()
  {
    return true;
  }

  void ImageManager::Ref_ReleaseScene()
  {
    if (sceneLoaded_)
    {
      const auto device = instance_->GetDevice();
      Ref_memoryPools_[MEMORY_POOL_SCENE]->Release(device);

      for (size_t i = 0; i < Ref_images_.size(); ++i)
      {
        vkDestroyImageView(device, Ref_imageViews_[i], nullptr);
        vkDestroyImage(device, Ref_images_[i], nullptr);
      }
      Ref_images_.clear();
      Ref_imageViews_.clear();
      Ref_imageInfos_.clear();
      Ref_descriptorInfos_.clear();

      sceneLoaded_ = false;
    }
  }

  void ImageManager::Ref_OnLoadScene(BufferManager* bufferManager, QueueManager* queueManager)
  {
    Ref_UploadImages(bufferManager, queueManager);
  }

  void ImageManager::Ref_Release(VkDevice device)
  {
    for (auto& info : Ref_imageCreateData_)
    {
      for (auto& data : info.data)
      {
        stbi_image_free(data);
      }
    }

    for (auto& pool : Ref_memoryPools_)
    {
      pool->Release(device);
    }

    for (auto& view : Ref_imageViews_)
    {
      vkDestroyImageView(device, view, nullptr);
    }
    for (const auto& views : Ref_arrayImageViews_)
    {
      for (const auto& view : views.second)
      {
        vkDestroyImageView(device, view, nullptr);
      }
    }

    for (auto& image : Ref_images_)
    {
      vkDestroyImage(device, image, nullptr);
    }
  }

  const VkDescriptorImageInfo& ImageManager::Ref_GetDescriptorInfo(int index) const
  {
    return Ref_descriptorInfos_[index];
  }

  VkImage ImageManager::Ref_CreateImage(const Ref_ImageInfo& info, int index)
  {
    const auto device = instance_->GetDevice();
    const auto physicalDevice = instance_->GetPhysicalDevice();

    auto image = Ref_CreateImageObject(device, info);
    if (image == VK_NULL_HANDLE)
    {
      return VK_NULL_HANDLE;
    }

    if (!Ref_memoryPools_[info.pool]->BindImage(physicalDevice, device, image))
    {
      printf("Failed binding image\n");
      return VK_NULL_HANDLE;
    }

    return image;
  }

  VkImage ImageManager::Ref_CreateImageObject(VkDevice device, const Ref_ImageInfo& info)
  {
    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = GetImageType(info.extent);
    imageCreateInfo.extent = info.extent;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = info.arrayCount;
    imageCreateInfo.format = info.format;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.usage = info.usage;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkImage image = VK_NULL_HANDLE;
    auto result = vkCreateImage(device, &imageCreateInfo, nullptr, &image);
    if (result != VK_SUCCESS)
    {
      printf("Failed creating image, %d\n", result);
      return VK_NULL_HANDLE;
    }
    return image;
  }

  VkImageView ImageManager::Ref_CreateImageView(VkDevice device, VkImage image, const Ref_ImageInfo& info)
  {
    return Ref_CreateImageView(device, image, info,
      0, static_cast<uint32_t>(info.arrayCount));
  }

  VkImageView ImageManager::Ref_CreateImageView(VkDevice device, VkImage image, const Ref_ImageInfo& info,
    uint32_t baseArrayLayer, uint32_t layerCount)
  {
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = GetViewType(info.extent, info.arrayCount);
    viewInfo.format = info.format;
    viewInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    if (info.format == VK_FORMAT_D32_SFLOAT_S8_UINT || info.format == VK_FORMAT_D16_UNORM)
    {
      viewInfo.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, baseArrayLayer, layerCount };
    }
    else
    {
      viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, baseArrayLayer, layerCount };
    }
    VkImageView imageView = VK_NULL_HANDLE;
    const auto result = vkCreateImageView(device, &viewInfo, nullptr, &imageView);
    if (result != VK_SUCCESS)
    {
      printf("Failed creating image view, %d\n", result);
      return VK_NULL_HANDLE;
    }
    return imageView;
  }

  void ImageManager::Ref_CopyDstTransition(QueueManager* queueManager)
  {
    const auto device = instance_->GetDevice();

    vkWaitForFences(device, 1, &transitionFence_, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &transitionFence_);

    VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(transitionCommandBuffer_, &beginInfo);

    std::vector<VkImageMemoryBarrier> barriers;

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    for (size_t i = 0; i < Ref_imageInfos_.size(); ++i)
    {
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.subresourceRange.layerCount = Ref_imageInfos_[i].arrayCount;
      barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

      if (Ref_imageInfos_[i].usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
      {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        barrier.dstAccessMask = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
      }

      if (Ref_imageInfos_[i].imageType == IMAGE_FILE)
      {
        barrier.image = Ref_images_[i];
        barriers.push_back(barrier);
      }
    }

    vkCmdPipelineBarrier(transitionCommandBuffer_, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
      0, 0, nullptr, 0, nullptr,
      static_cast<uint32_t>(barriers.size()), barriers.data());

    auto result = vkEndCommandBuffer(transitionCommandBuffer_);
    assert(result == VK_SUCCESS);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &transitionCommandBuffer_;

    const auto& graphicQueue = queueManager->GetQueue(QueueManager::QUEUE_GRAPHICS);
    result = vkQueueSubmit(graphicQueue, 1, &submitInfo, transitionFence_);
    assert(result == VK_SUCCESS);

    vkWaitForFences(device, 1, &transitionFence_, VK_TRUE, UINT64_MAX);
  }

  int ImageManager::Ref_GetChannelCount(VkFormat format)
  {
    switch (format)
    {
    case VK_FORMAT_R8G8B8A8_UNORM:
      return 4;
    case VK_FORMAT_R8_UNORM:
      return 1;
    default:
      printf("Channel count for format not implemented\n");
      return -1;
    }
  }

  void ImageManager::Ref_UploadImages(BufferManager* bufferManager, QueueManager* queueManager)
  {
    VkDeviceSize totalSize = 0;
    for (auto& info : Ref_imageCreateData_)
    {
      for (const auto size : info.allocatedSize)
      {
        totalSize += size;
      }
    }

    BufferManager::BufferInfo info = {};
    info.pool = BufferManager::MEMORY_TEMP;
    info.typeBits = BufferManager::BUFFER_TEMP_BIT;
    info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    info.size = totalSize * sizeof(unsigned char);
    if (info.size != 0)
    {
      const auto tempBuffer = bufferManager->RequestBuffer(info);

      auto data = bufferManager->Map(tempBuffer, BufferManager::BUFFER_TEMP_BIT);

      for (auto& info : Ref_imageCreateData_)
      {
        for (size_t i = 0; i < info.data.size(); ++i)
        {
          memcpy(data, info.data[i], info.allocatedSize[i] * sizeof(unsigned char));
          data = static_cast<unsigned char*>(data) + info.allocatedSize[i];
        }
      }

      bufferManager->Unmap(tempBuffer, BufferManager::BUFFER_TEMP_BIT);

      Ref_CopyDstTransition(queueManager);

      VkDeviceSize bufferOffset = 0;
      for (size_t i = 0; i < Ref_imageCreateData_.size(); ++i)
      {
        const auto index = Ref_imageCreateData_[i].imageIndex;
        if (Ref_imageInfos_[index].imageType == IMAGE_FILE)
        {
          VkBufferImageCopy imageCopy = {};
          imageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
          imageCopy.imageSubresource.mipLevel = 0;
          imageCopy.imageSubresource.baseArrayLayer = 0;
          imageCopy.imageSubresource.layerCount = Ref_imageInfos_[index].arrayCount;
          imageCopy.imageExtent = Ref_imageInfos_[index].extent;
          imageCopy.bufferOffset = bufferOffset;

          bufferManager->RequestImageCopy(tempBuffer, Ref_images_[index],
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageCopy);

          for (const auto size : Ref_imageCreateData_[i].allocatedSize)
          {
            bufferOffset += size;
          }
        }
      }

      bufferManager->PerformCopies(queueManager);
    }

    int debugCount = 0;
    for (auto& info : Ref_imageCreateData_)
    {
      for (auto& data : info.data)
      {
        stbi_image_free(data);
      }
    }

    Ref_imageCreateData_.clear();
  }

  void ImageManager::Ref_ResizeImages(int index, VkDeviceSize newSize, ImageMemoryPool pool)
  {
    const auto device = instance_->GetDevice();
    //TODO: remove with correct fences
    vkDeviceWaitIdle(device);
    Ref_memoryPools_[pool]->Release(device);

    uint32_t extent = static_cast<uint32_t>(newSize);
    Ref_imageInfos_[index].extent = { extent, extent, extent };
    vkDestroyImage(device, Ref_images_[index], nullptr);
    vkDestroyImageView(device, Ref_imageViews_[index], nullptr);

    Ref_images_[index] = Ref_CreateImage(Ref_imageInfos_[index], index);
    Ref_imageViews_[index] = Ref_CreateImageView(instance_->GetDevice(), Ref_images_[index], Ref_imageInfos_[index]);

    Ref_descriptorInfos_[index].imageView = Ref_imageViews_[index];
    Ref_descriptorInfos_[index].imageLayout = Ref_imageInfos_[index].layout;
  }

  void ImageManager::Ref_ClearImage(VkCommandBuffer commandBuffer, int index, VkClearColorValue clearColor)
  {
    VkImageSubresourceRange range;
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = Ref_imageInfos_[index].arrayCount;
    vkCmdClearColorImage(commandBuffer, Ref_images_[index], VK_IMAGE_LAYOUT_GENERAL,
      &clearColor, 1, &range);
  }
}