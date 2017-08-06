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

#include "ImageAtlas.h"

#include "AdaptiveGridConstants.h"
#include "..\..\resources\BufferManager.h"
#include "..\..\resources\ImageManager.h"

namespace Renderer
{
	ImageAtlas::ImageAtlas(int imageResolution) :
		cbData_{imageResolution}
	{}

	void ImageAtlas::RequestResources(ImageManager* imageManager, BufferManager* bufferManager, 
		ImageManager::ImageMemoryPool memoryPool, int frameCount)
	{
		memoryPool_ = memoryPool;

		ImageManager::Ref_ImageInfo imageInfo;
		const uint32_t imageResolution = static_cast<uint32_t>(cbData_.imageResolution);
		imageInfo.extent = { imageResolution,imageResolution,imageResolution };
		imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		imageInfo.imageType = ImageManager::IMAGE_GRID;
		imageInfo.layout = VK_IMAGE_LAYOUT_GENERAL;
		imageInfo.pool = memoryPool_;
		imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		imageInfo.sampler = ImageManager::SAMPLER_GRID;
		imageResource = { imageManager->Ref_RequestImage(imageInfo), 0, 1 };

		BufferManager::BufferInfo cbInfo;
		cbInfo.typeBits = BufferManager::BUFFER_CONSTANT_BIT | BufferManager::BUFFER_SCENE_BIT;
		cbInfo.pool = BufferManager::MEMORY_CONSTANT;
		cbInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		cbInfo.bufferingCount = frameCount;
		cbInfo.data = &cbData_;
		cbInfo.size = sizeof(CBData);
		cbIndex_ = bufferManager->Ref_RequestBuffer(cbInfo);
	}

	void ImageAtlas::UpdateSize(int maxImageOffset)
	{
		sideLength_ = static_cast<int>(ceilf(powf(static_cast<float>(maxImageOffset), 1.0f / 3.0f)));
		sideLength_ = std::max(1, sideLength_);

		cbData_.atlasSideLength = sideLength_;
	}

	void ImageAtlas::ResizeImage(ImageManager* imageManager)
	{
		VkDeviceSize newSize = sideLength_ * cbData_.imageResolution;
		auto& maxSize = imageResource.maxSize;
		
		if (maxSize < newSize)
		{
			maxSize = newSize;
			imageManager->Ref_ResizeImages(imageResource.index, maxSize, memoryPool_);
		}
	}

}