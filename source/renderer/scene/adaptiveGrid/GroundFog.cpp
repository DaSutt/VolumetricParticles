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

#include "GroundFog.h"

#include <math.h>
#include <iostream>
#include <fstream>

#include "GridLevel.h"
#include "AdaptiveGridConstants.h"
#include "..\..\resources\BufferManager.h"
#include "..\..\resources\ImageManager.h"
#include "..\..\passResources\ShaderBindingManager.h"
#include "..\..\passes\GuiPass.h"
#include "..\..\..\fileIO\FileDialog.h"
#include "..\..\..\utility\Status.h"
#include "..\..\wrapper\QueryPool.h"

namespace
{
	//Texture to store the density of the fog in world space
	constexpr bool createDebugTexture = false;

	constexpr bool debugFilling = false;
	const glm::ivec2 debugStart = glm::ivec2(GridConstants::nodeResolution / 2);
	const glm::ivec2 debugEnd = debugStart + glm::ivec2(1,1);
}

namespace Renderer
{
	GroundFog::GroundFog() :
		mediumName_{"groundFogMedium"}
	{}

	void GroundFog::SetSizes(float cellSize, float gridSize)
	{
		cellWorldSize_ = cellSize;
		childCellWorldSize_ = cellSize / static_cast<float>(GridConstants::nodeResolution);
		gridWorldSize_ = gridSize;
		cbData_.texelWorldSize = cellWorldSize_ / GridConstants::nodeResolution;
		cellBufferSize_ = (GridConstants::nodeResolution * GridConstants::nodeResolution + 1) * sizeof(PerCell);
	}

	void GroundFog::RequestResources(ImageManager* imageManager, BufferManager* bufferManager, int frameCount, int atlasImageIndex)
	{
		BufferManager::BufferInfo cbInfo;
		cbInfo.typeBits = BufferManager::BUFFER_CONSTANT_BIT | BufferManager::BUFFER_SCENE_BIT;
		cbInfo.pool = BufferManager::MEMORY_CONSTANT;
		cbInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		cbInfo.bufferingCount = frameCount;
		cbInfo.data = &cbData_;
		cbInfo.size = sizeof(CBData);
		cbIndex_ = bufferManager->Ref_RequestBuffer(cbInfo);

		BufferManager::BufferInfo imageIndexInfo;
		imageIndexInfo.typeBits = BufferManager::BUFFER_GRID_BIT;
		imageIndexInfo.pool = BufferManager::MEMORY_GRID;
		imageIndexInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		imageIndexInfo.bufferingCount = frameCount;
		const int nodeResolutionSquared = GridConstants::nodeResolution * GridConstants::nodeResolution;
		imageIndexInfo.size = (nodeResolutionSquared + 1) * sizeof(PerCell);
		cellData_.resize(nodeResolutionSquared + 1);
		perCellBuffer = bufferManager->Ref_RequestBuffer(imageIndexInfo);

		if (createDebugTexture)
		{
			ImageManager::Ref_ImageInfo imageInfo{};
			imageInfo.arrayCount = 1;
			const uint32_t size = GridConstants::nodeResolution * GridConstants::nodeResolution + 1;
			imageInfo.extent = { size, GridConstants::imageResolution, size };
			imageInfo.format = VK_FORMAT_R32_SFLOAT;
			imageInfo.imageType = ImageManager::IMAGE_NO_POOL;
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageInfo.layout = VK_IMAGE_LAYOUT_GENERAL;
			imageInfo.pool = ImageManager::MEMORY_POOL_INDIVIDUAL;
			imageInfo.sampler = ImageManager::SAMPLER_LINEAR;
			imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			imageInfo.viewPerLayer = 1;
			debugTextureIndex_ = imageManager->Ref_RequestImage(imageInfo);
		}

		atlasImageIndex_ = atlasImageIndex;
	}

	int GroundFog::GetShaderBinding(ShaderBindingManager* bindingManager, int frameCount)
	{
		ShaderBindingManager::BindingInfo bindingInfo = {};
		bindingInfo.pass = SUBPASS_VOLUME_ADAPTIVE_GROUND_FOG;
		bindingInfo.resourceIndex = { atlasImageIndex_, cbIndex_, perCellBuffer };
		bindingInfo.stages = { VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_COMPUTE_BIT };
		bindingInfo.types = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER };
		bindingInfo.Ref_image = true;
		bindingInfo.refactoring_ = { true, true, true };
		bindingInfo.setCount = frameCount;

		if (createDebugTexture)
		{
			bindingInfo.resourceIndex.push_back(debugTextureIndex_);
			bindingInfo.stages.push_back(VK_SHADER_STAGE_COMPUTE_BIT);
			bindingInfo.types.push_back(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
			bindingInfo.refactoring_.push_back(true);
		}

		return bindingManager->RequestShaderBinding(bindingInfo);
	}

	void GroundFog::UpdateCBData(float heightPercentage, float scattering, float absorption, float phaseG, float noiseScale)
	{
		gridSpaceHeight_ = gridWorldSize_ - heightPercentage * gridWorldSize_;
		gridYPos_ = static_cast<int>(floor(gridSpaceHeight_ / cellWorldSize_));
		coarseGridPos_ = gridYPos_ + 2;

		cbData_.scattering = scattering;
		cbData_.absorption = absorption + scattering;
		cbData_.phaseG = phaseG;

		const float cellSpaceHeight = gridSpaceHeight_ - gridYPos_ * cellWorldSize_;
		const float cellPercentage = 1.0f - cellSpaceHeight / cellWorldSize_;
		const float cellSpaceOffset = GridConstants::imageResolution * cellSpaceHeight / cellWorldSize_;
		cbData_.edgeTexelY = static_cast<int>(floor(cellSpaceOffset));
		cbData_.cellFraction = (cbData_.edgeTexelY * childCellWorldSize_ - cellSpaceHeight) / childCellWorldSize_;

		cbData_.noiseScale = noiseScale;
	}

	void GroundFog::UpdateGridCells(GridLevel* gridLevel)
	{
		if (gridSpaceHeight_ == 512.0f)
		{
			//Skip adding cells if no ground fog used
			return;
		}

		nodeIndices_.clear();
		if (debugFilling)
		{
			for (size_t z = debugStart.y; z < debugEnd.y; ++z)
			{
				for (size_t x = debugStart.x; x < debugEnd.x; ++x)
				{
					glm::vec3 gridPos = glm::vec3(x, gridYPos_, z) * gridLevel->GetGridCellSize();
					nodeIndices_.push_back(gridLevel->AddNode(gridPos));
				}
			}
		}
		else
		{
			for (size_t z = 0; z < GridConstants::nodeResolution; ++z)
			{
				for (size_t x = 0; x < GridConstants::nodeResolution; ++x)
				{
					glm::vec3 gridPos = glm::vec3(x, gridYPos_, z) * gridLevel->GetGridCellSize();
					nodeIndices_.push_back(gridLevel->AddNode(gridPos));
				}
			}
		}
	}

	void GroundFog::UpdatePerCellBuffer(BufferManager* bufferManager, GridLevel* gridLevel, int frameIndex, int atlasResolution)
	{
		if (gridSpaceHeight_ == 512.0f)
		{
			dispatchCount_ = 0;
			return;
		}

		auto bufferDataPtr = bufferManager->Ref_Map(perCellBuffer, frameIndex, BufferManager::BUFFER_GRID_BIT);

		//TODO: calculate image offsets
		cellData_.clear();
		const float cellSize = gridLevel->GetGridCellSize();
		const float cellOffset = cellSize / gridLevel->gridResolution_ * 0.5f;

		const auto& nodeData = gridLevel->GetNodeData();
		for (const auto indexNode : nodeIndices_)
		{
			const glm::ivec3 imageOffset = nodeData.imageInfos_[indexNode].image + 1;

			PerCell perCell;
			perCell.imageOffset = NodeData::PackTextureOffset(imageOffset);
			perCell.worldOffset = nodeData.gridPos_[indexNode] * cellSize;
			cellData_.push_back(perCell);
		}

		dispatchCount_ = static_cast<int>(cellData_.size());

		memcpy(bufferDataPtr, cellData_.data(), cellData_.size() * sizeof(PerCell));
		bufferManager->Ref_Unmap(perCellBuffer, frameIndex, BufferManager::BUFFER_GRID_BIT);
	}

	void GroundFog::Dispatch(QueueManager* queueManager, ImageManager* imageManager, 
		BufferManager* bufferManager, VkCommandBuffer commandBuffer, int frameIndex)
	{
		if (createDebugTexture)
		{
			VkClearColorValue clearValue = { 0.0f, 0.0f, 0.0f, 0.0f };
			imageManager->Ref_ClearImage(commandBuffer, debugTextureIndex_, clearValue);
		}

		if (dispatchCount_ > 0)
		{
			auto& queryPool = Wrapper::QueryPool::GetInstance();
			queryPool.TimestampStart(commandBuffer, Wrapper::TIMESTAMP_GRID_GROUND_FOG, frameIndex);

			vkCmdDispatch(commandBuffer, dispatchCount_, 1, 1);

			queryPool.TimestampEnd(commandBuffer, Wrapper::TIMESTAMP_GRID_GROUND_FOG, frameIndex);
		}
		
		if (createDebugTexture && GuiPass::GetMenuState().exportFogTexture)
		{
			ExportGroundFogTexture(queueManager, imageManager, bufferManager);
		}
	}

	void GroundFog::ExportGroundFogTexture(QueueManager* queueManager, ImageManager* imageManager, BufferManager* bufferManager)
	{
		vkQueueWaitIdle(queueManager->GetQueue(QueueManager::QUEUE_COMPUTE));

		BufferManager::BufferInfo bufferInfo;
		bufferInfo.bufferingCount = 1;
		bufferInfo.pool = BufferManager::MEMORY_TEMP;
		bufferInfo.size = imageManager->Ref_GetImageSize(debugTextureIndex_);
		bufferInfo.typeBits = BufferManager::BUFFER_TEMP_BIT;
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		const int tempBufferIndex = bufferManager->RequestBuffer(bufferInfo);

		VkImageSubresourceLayers subresourceLayers = {};
		//TODO check if this value is needed
		subresourceLayers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceLayers.baseArrayLayer = 0;
		subresourceLayers.layerCount = 1;
		subresourceLayers.mipLevel = 0;

		const auto imageExtent = imageManager->Ref_GetImageInfo(debugTextureIndex_).extent;

		VkBufferImageCopy copyRegion = {};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;
		copyRegion.imageSubresource = subresourceLayers;
		copyRegion.imageOffset = {};
		copyRegion.imageExtent = imageExtent;
		imageManager->CopyImageToBuffer(queueManager,
			bufferManager->GetBuffer(tempBufferIndex, BufferManager::BUFFER_TEMP_BIT),
			{ copyRegion }, VK_IMAGE_LAYOUT_GENERAL, debugTextureIndex_, true);

		const auto bufferBits = BufferManager::BUFFER_TEMP_BIT;
		SaveTextureData(bufferManager->Map(tempBufferIndex, bufferBits), bufferInfo.size, imageExtent);

		bufferManager->Unmap(tempBufferIndex, bufferBits);
		bufferManager->ReleaseTempBuffer(tempBufferIndex);
	}

	void GroundFog::SaveTextureData(void* dataPtr, VkDeviceSize imageSize, const VkExtent3D& imageExtent)
	{
		const auto filePath = FileIO::SaveFileDialog(L"pbrt");
		const auto fileName = FileIO::GetFileNameWithExtension(filePath);

		std::ofstream file;
		file.open(filePath.c_str(), std::ios::out);
		if (file)
		{
			const auto floatValues = reinterpret_cast<glm::float32*>(dataPtr);
			const auto elementSize = sizeof(glm::float32);
			const auto elementCount = imageExtent.width * imageExtent.height * imageExtent.depth;

			const auto& s_a = cbData_.absorption;
			const auto& s_s = cbData_.scattering;
			
			const auto& gridStatus = Status::GetGridStatus();
			const float gridSize = gridStatus.size.x;
			const float cellSize = gridSize / static_cast<float>(GridConstants::nodeResolution);
			
			glm::vec3 minTexturePos = gridStatus.min + glm::vec3(0, gridYPos_ * cellSize, 0);
			const glm::vec3 scale = glm::vec3(gridSize, cellSize, gridSize);
			glm::vec3 maxTexturePos = minTexturePos + scale;
			
			//Swap the min and max y position and negate them to account for y up instead of down
			const float yMin = -maxTexturePos.y;
			maxTexturePos.y = -minTexturePos.y;
			minTexturePos.y = yMin;
						
			file <<
				"\nAttributeBegin" <<
				"\nMakeNamedMedium \"" << mediumName_ << "\"" <<
				"\n\t\"string type\" \"heterogeneous\"" <<
				"\n\t\"rgb	sigma_a\" [" << s_a << " " << s_a << " " << s_a << "]" <<
				"\n\t\"rgb sigma_s\" [" << s_s << " " << s_s << " " << s_s << "]" <<
				"\n\t\"float g\" " << cbData_.phaseG << "" <<
				"\n\t\"point p0\" [" << minTexturePos.x << " " << minTexturePos.y << " " << minTexturePos.z << "]" <<
				"\n\t\"point p1\" [" << maxTexturePos.x << " " << maxTexturePos.y << " " << maxTexturePos .z << "]" <<
				"\n\t\"integer nx\" " << imageExtent.width << "" <<
				"\n\t\"integer ny\" " << imageExtent.height << "" <<
				"\n\t\"integer nz\" " << imageExtent.depth << "" <<

				"\n\t\"float density\" [\n";

			for (uint32_t i = 0; i < elementCount; ++i)
			{
				file << floatValues[i] << " ";
				if (i % imageExtent.width == 0)
				{
					file << "\n";
				}
			}

			file << "\n]" <<
				"\nAttributeEnd\n";

			const float yOffset = maxTexturePos.y - cellSize * 0.5f;
			
			Status::UpdateGroundFog(fileName, mediumName_, scale, yOffset);
		}
	}
}