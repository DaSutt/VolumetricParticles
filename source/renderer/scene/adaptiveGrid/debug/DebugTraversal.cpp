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

#include "DebugTraversal.h"
#include <glm\glm.hpp>

#include "..\..\..\resources\ImageManager.h"
#include "..\..\..\resources\QueueManager.h"
#include "..\..\..\resources\BufferManager.h"

using namespace glm;
#undef max
#undef min

#define CPU_DEBUG
#include "..\..\..\..\shaders\raymarching\Raymarching.comp"

namespace Renderer
{
	void DebugTraversal::Traversal(QueueManager* queueManager, BufferManager* bufferManager, ImageManager* imageManager, int x, int y)
	{
		printf("Debug traversal at %d %d\n", x, y);

		Raymarching(uvec3(x, y, 0));
		//GetImageData(queueManager, bufferManager, imageManager);
		//ReleaseImageData(bufferManager);
	}

	void DebugTraversal::SetImageIndices(int imageAtlas, int depth, int shadowMap, int noiseTexture)
	{
		imageIndices_[IMAGE_ATLAS] = { imageAtlas, true };
		imageIndices_[IMAGE_DEPTH] = { depth, false };
		imageIndices_[IMAGE_SHADOW] = { shadowMap, true };
		imageIndices_[IMAGE_NOISE] = { noiseTexture, true };
	}

	/*void DebugTraversal::GetImageData(QueueManager* queueManager, BufferManager* bufferManager, ImageManager * imageManager)
	{
		BufferManager::BufferInfo bufferInfo;
		bufferInfo.bufferingCount = 1;
		bufferInfo.pool = BufferManager::MEMORY_TEMP;
		bufferInfo.size = 0;
		for (int i = 0; i < IMAGE_MAX; ++i)
		{
			imageIndices_[i].offset = bufferInfo.size;
			if (imageIndices_[i].refactoring)
			{
				imageIndices_[i].size = imageManager->Ref_GetImageSize(i);
			}
			else
			{
				imageIndices_[i].size = imageManager->GetImageSize(i);
			}
			bufferInfo.size = imageIndices_[i].size + imageIndices_[i].offset;
		}
		bufferInfo.typeBits = BufferManager::BUFFER_TEMP_BIT;
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		tempBufferIndex_ = bufferManager->RequestBuffer(bufferInfo);

		for (int i = 0; i < IMAGE_MAX; ++i)
		{
			const int imageIndex = imageIndices_[i].index;
			const bool refactoring = imageIndices_[i].refactoring;

			VkImageSubresourceLayers subresourceLayers = {};
			if (i == IMAGE_DEPTH || i == IMAGE_SHADOW)
			{
				subresourceLayers.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			}
			else
			{
				subresourceLayers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			}
			subresourceLayers.baseArrayLayer = 0;
			subresourceLayers.layerCount = 1;
			subresourceLayers.mipLevel = 0;

			const auto imageExtent = refactoring ? 
				imageManager->Ref_GetImageInfo(imageIndex).extent : 
				imageManager->GetImageInfo(imageIndex).extent;
			imageIndices_[i].width = imageExtent.width;

			VkBufferImageCopy copyRegion = {};
			copyRegion.bufferOffset = imageIndices_[i].offset;
			copyRegion.bufferRowLength = 0;
			copyRegion.bufferImageHeight = 0;
			copyRegion.imageSubresource = subresourceLayers;
			copyRegion.imageOffset = {};
			copyRegion.imageExtent = imageExtent;
			VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL;
			if (i == IMAGE_DEPTH)
			{
				layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}
			else if (i == IMAGE_NOISE)
			{
				layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			}

			imageManager->CopyImageToBuffer(queueManager,
				bufferManager->GetBuffer(tempBufferIndex_, BufferManager::BUFFER_TEMP_BIT),
				{ copyRegion }, layout, imageIndex, refactoring);
		}

		imageDataPtr_ = bufferManager->Map(tempBufferIndex_, BufferManager::BUFFER_TEMP_BIT);
	}

	void DebugTraversal::ReleaseImageData(BufferManager* bufferManager)
	{
		bufferManager->Unmap(tempBufferIndex_, BufferManager::BUFFER_TEMP_BIT);
		bufferManager->ReleaseTempBuffer(tempBufferIndex_);

		imageDataPtr_ = nullptr;
	}

	float DebugTraversal::imageLoad(int index, glm::ivec2 imagePos)
	{
		const int texelIndex = imagePos.y * imageIndices_[index].width + imagePos.x;
		char* imagePtr = static_cast<char*>(imageDataPtr_) + imageIndices_[index].offset;
		float result = 0.0f;
		if (index == IMAGE_DEPTH)
		{
			const glm::uint16 depthUnorm = reinterpret_cast<glm::uint16*>(imagePtr)[texelIndex];
			result = depthUnorm / 65536.0f;
		}
		return result;
	}

	vec4 DebugTraversal::texelFetch(int index, glm::ivec3 texelPosition, int lod)
	{
		int texelIndex = CalcBitIndex(texelPosition, imageIndices_[index].width);
		char* imagePtr = static_cast<char*>(imageDataPtr_) + imageIndices_[index].offset;
		vec4 result = vec4(0.0f);
		if (index == IMAGE_ATLAS)
		{
			texelIndex *= 2;
			const vec2 xy = unpackHalf2x16(reinterpret_cast<uint*>(imagePtr)[texelIndex]);
			const vec2 zw = unpackHalf2x16(reinterpret_cast<uint*>(imagePtr)[texelIndex + 1]);
			result = vec4(xy, zw);
		}
		else if (index == IMAGE_SHADOW)
		{
			const vec2 xy = unpackHalf2x16(reinterpret_cast<uint*>(imagePtr)[texelIndex / 2]);
			result.x = texelIndex % 2 == 0 ? xy.x : xy.y;
		}
		return result;
	}

	vec3 Calc2DTexPosition(vec3 texCoord, int width)
	{
		return texCoord * static_cast<float>(width);
	}

	vec4 DebugTraversal::textureLod(int index, glm::vec3 texCoord, int lod)
	{
		vec3 texelPosition;
		if (index == IMAGE_ATLAS)
		{
			 texelPosition = CalcTexelPosition(currentGridPos_,
				imageOffset_, currentLevel_);
		}
		else if (index == IMAGE_SHADOW)
		{
			texelPosition = Calc2DTexPosition(texCoord, imageIndices_[index].width);
			texelPosition.z = 0;
		}
		return SampleGridInterpolate(texelPosition, index);
	}*/
}
