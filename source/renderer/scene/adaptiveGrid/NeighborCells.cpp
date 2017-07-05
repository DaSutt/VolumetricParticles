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

#include "NeighborCells.h"

#include "..\..\resources\BufferManager.h"
#include "..\..\resources\ImageManager.h"

#include "..\..\passResources\ShaderBindingManager.h"

#include "..\..\wrapper\Barrier.h"
#include "..\..\wrapper\QueryPool.h"

#include "AdaptiveGridConstants.h"

namespace Renderer
{
	NeighborCells::NeighborCells() :
		nodeResolutionSquared_{GridConstants::nodeResolution * GridConstants::nodeResolution },
		axisOffsets_{
			1, 
			GridConstants::nodeResolution,
			nodeResolutionSquared_ },
		minGridPos_{0},
		maxGridPos_{ GridConstants::nodeResolution - 1}
	{
		atlasProperties_.resolution = GridConstants::imageResolution;
		atlasProperties_.resolutionOffset = GridConstants::imageResolution;
	}

	void NeighborCells::RequestResources(BufferManager* bufferManager, int frameCount, int atlasImageIndex)
	{
		BufferManager::BufferInfo cbInfo;
		cbInfo.typeBits = BufferManager::BUFFER_CONSTANT_BIT | BufferManager::BUFFER_SCENE_BIT;
		cbInfo.pool = BufferManager::MEMORY_CONSTANT;
		cbInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		cbInfo.bufferingCount = frameCount;
		cbInfo.data = &atlasProperties_;
		cbInfo.size = sizeof(AtlasProperties);
		cbIndex_ = bufferManager->Ref_RequestBuffer(cbInfo);

		BufferManager::BufferInfo storageBufferInfo;
		storageBufferInfo.typeBits = BufferManager::BUFFER_GRID_BIT;
		storageBufferInfo.pool = BufferManager::MEMORY_GRID;
		storageBufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		storageBufferInfo.bufferingCount = frameCount;
		//fill with one element, will later be resized properly
		storageBufferInfo.size = sizeof(NeighborData);
		neighborBufferIndex_ = bufferManager->Ref_RequestBuffer(storageBufferInfo);
		totalSize_ = storageBufferInfo.size;
		
		atlasImageIndex_ = atlasImageIndex;
	}

	int NeighborCells::GetShaderBinding(ShaderBindingManager* bindingManager, int frameCount)
	{
		ShaderBindingManager::PushConstantInfo pushConstantInfo{};
		VkPushConstantRange objectIds = {};
		objectIds.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		objectIds.offset = 0;
		objectIds.size = sizeof(PushConstantData);
		pushConstantInfo.pushConstantRanges.push_back(objectIds);
		pushConstantInfo.pass = SUBPASS_VOLUME_ADAPTIVE_NEIGHBOR_UPDATE;
		perLevelPushConstantIndex_ = bindingManager->RequestPushConstants(pushConstantInfo)[0];

		ShaderBindingManager::BindingInfo bindingInfo = {};
		bindingInfo.pass = SUBPASS_VOLUME_ADAPTIVE_NEIGHBOR_UPDATE;
		bindingInfo.resourceIndex = { atlasImageIndex_, cbIndex_, neighborBufferIndex_ };
		bindingInfo.stages = { VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_COMPUTE_BIT, 
			VK_SHADER_STAGE_COMPUTE_BIT };
		bindingInfo.types = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER };
		bindingInfo.Ref_image = true;
		bindingInfo.refactoring_ = { true, true, true };
		bindingInfo.setCount = frameCount;

		bindingManager_ = bindingManager;

		return bindingManager->RequestShaderBinding(bindingInfo);
	}

	void NeighborCells::UpdateAtlasProperties(int sideLength)
	{
		atlasProperties_.sideLength = sideLength;
	}
	
	void NeighborCells::CalculateNeighbors(const std::vector<GridLevel>& gridLevels)
	{
		neighborInfos_.clear();
		mipMapNeighborInfos_.clear();

		mipMapDataOffsets_.clear();

		neighborCopy_.clear();
		mipMapNeighborCopy_.clear();

		neighborSets_.clear();
		mipMapNeighborSets_.clear();

		//skip the top level, because it does not have neighbors
		for (size_t levelIndex = 1; levelIndex < gridLevels.size(); ++levelIndex)
		{
			mipMapDataOffsets_[static_cast<int>(levelIndex)] = static_cast<int>(mipMapNeighborInfos_.size());

			const auto& nodeData = gridLevels[levelIndex].GetNodeData();
			const auto& parentNodeData = gridLevels[levelIndex - 1].GetNodeData();

			const auto& nodeInfos = nodeData.GetNodeInfos();
			const auto& parentNodeInfos = parentNodeData.GetNodeInfos();
			for (size_t indexNode = 0; indexNode < nodeInfos.size(); ++indexNode)
			{
				const int parentNodeIndex = nodeData.parentIndices_[indexNode];
				const auto& gridPos = nodeData.gridPos_[indexNode];

				NeighborInfo neighborInfo{};
				neighborInfo.first = nodeInfos[indexNode].textureOffset;
				NeighborInfo mipMapNeighborInfo{};
				mipMapNeighborInfo.first = nodeInfos[indexNode].textureOffsetMipMap;

				NeighborData neighborData{};
				neighborData.currImageIndex = nodeInfos[indexNode].textureOffset;
				neighborData.activeNeihgborBits = 0;
				NeighborData mipmapNeighborData{};
				mipmapNeighborData.currImageIndex = nodeInfos[indexNode].textureOffsetMipMap;
				mipmapNeighborData.activeNeihgborBits = 0;

				const int destinationImageIndex = nodeInfos[indexNode].textureOffset;

				//Calculate neihgbor grid pos for each direction and check if neighbor exists
				for (int dir = 0; dir < DIR_MAX; ++dir)
				{
					const int coordinate = dir / 2;
					const int directionOffset = (dir % 2 == 0 ? -1 : 1);

					glm::ivec3 neighborGridPos = gridPos;
					int posOffset = neighborGridPos[coordinate] + directionOffset;
					int neighborIndexGrid = -1;
					int currParentNodeIndex = parentNodeIndex;
					int parentNeighborGridIndex = parentNodeData.nodeGridIndex[currParentNodeIndex];

					//only handle edge cases for higher levels, because level 1 edge cases are outside of the grid
					if (levelIndex > 1 && EdgeCase(posOffset))
					{
						posOffset = posOffset < 0 ? 15 : 0;
						neighborGridPos[coordinate] = posOffset;
						neighborIndexGrid = CalcGridIndex(neighborGridPos.x, neighborGridPos.y, neighborGridPos.z, GridConstants::nodeResolution);

						parentNeighborGridIndex = parentNodeData.nodeGridIndex[currParentNodeIndex]
							+ axisOffsets_[coordinate] * directionOffset;
					}
					//Calculate the index for the neighbor based on its grid position
					else
					{
						neighborGridPos[coordinate] = posOffset;
						neighborIndexGrid = CalcGridIndex(neighborGridPos.x, neighborGridPos.y, neighborGridPos.z, GridConstants::nodeResolution);
					}

					//Check the current level if the neighborIndex does exist and add its image index
					const int neighborIndexNode = gridLevels[levelIndex].FindIndexNode(
						parentNeighborGridIndex, neighborIndexGrid);

					if (neighborIndexNode != -1)
					{
						const int neighborImageIndex = nodeInfos[neighborIndexNode].textureOffset;
						neighborData.neighborImageIndices[dir] = neighborImageIndex;
						neighborData.activeNeihgborBits |= 1 << dir;

						const int sourceImageIndex = nodeInfos[neighborIndexNode].textureOffset;
						neighborInfo.second = neighborImageIndex;
						neighborInfo.direction = dir;
						AddNeighborInfo(neighborInfo, false);

						const int mipMapIndex = nodeInfos[indexNode].textureOffsetMipMap;
						const int neighborMipMapIndex = nodeInfos[neighborIndexNode].textureOffsetMipMap;
						if (mipMapIndex != 0 && neighborMipMapIndex != 0)
						{
							const int neighborMipMapIndex = nodeInfos[neighborIndexNode].textureOffsetMipMap;
							const uint32_t neighborMipMapImageIndex = neighborMipMapIndex != -1 ?
							neighborMipMapIndex : neighborImageIndex;
							mipmapNeighborData.activeNeihgborBits |= 1 << dir;

							mipMapNeighborInfo.second = neighborMipMapImageIndex;
							mipMapNeighborInfo.direction = dir;
							AddNeighborInfo(mipMapNeighborInfo, true);
						}
					}
				}
			}
		}
	}

	bool NeighborCells::NeighborPairAdded(uint32_t first, uint32_t second)
	{
		if (neighborSets_.find({ first, second }) != neighborSets_.end())
		{
			return true;
		}
		else
		{
			neighborSets_.insert({ first, second });
			neighborSets_.insert({ second, first });
			return false;
		}
	}

	void NeighborCells::AddNeighborInfo(const NeighborInfo& info, bool mipMap)
	{
		if (!NeighborPairAdded(info.first, info.second))
		{
			if (mipMap)
			{
				mipMapNeighborInfos_.push_back(info);
			}
			else
			{
				neighborInfos_.push_back(info);
			}
		}
	}

	bool NeighborCells::ResizeGpuResources(std::vector<ResourceResize>& resizes)
	{
		bool resize = false;
		const VkDeviceSize newSize = (neighborInfos_.size() + mipMapNeighborInfos_.size())
			* sizeof(NeighborInfo);
		if (totalSize_ < newSize)
		{
			resize = true;
			totalSize_ = newSize;
		}
		resizes.push_back({ totalSize_, neighborBufferIndex_ });
		return resize;
	}

	void NeighborCells::UpdateGpuResources(BufferManager* bufferManager, int frameIndex)
	{
		const int bufferTypeBits = BufferManager::BUFFER_GRID_BIT;
		auto dataPtr = bufferManager->Ref_Map(neighborBufferIndex_, frameIndex, bufferTypeBits);
		const int neighborDataSize = static_cast<int>(neighborInfos_.size() * sizeof(NeighborInfo));
		memcpy(dataPtr, neighborInfos_.data(), neighborDataSize);
		
		auto dataPtrMipMap = reinterpret_cast<char*>(dataPtr) + neighborDataSize;
		memcpy(dataPtrMipMap, mipMapNeighborInfos_.data(), mipMapNeighborInfos_.size() * sizeof(NeighborInfo));
		bufferManager->Ref_Unmap(neighborBufferIndex_, frameIndex, bufferTypeBits);
	}

	void NeighborCells::Dispatch(VkCommandBuffer commandBuffer, ImageManager* imageManager, int level, bool mipMapping, int frameIndex)
	{
		//TODO: replace with correct lowest level
		if (!mipMapping)
		{
			uint32_t dispatchSize = static_cast<uint32_t>(neighborInfos_.size());
			if (dispatchSize > 0)
			{
				auto& queryPool = Wrapper::QueryPool::GetInstance();
				queryPool.TimestampStart(commandBuffer, Wrapper::TIMESTAMP_GRID_NEIGHBOR_UPDATE, frameIndex);

				PushConstantData levelPushConstantData = { 0 };
				vkCmdPushConstants(commandBuffer,
					bindingManager_->GetPipelineLayout(SUBPASS_VOLUME_ADAPTIVE_NEIGHBOR_UPDATE),
					VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantData),
					&levelPushConstantData);

				vkCmdDispatch(commandBuffer, dispatchSize, 1, 1);
				AddImageBarrier(commandBuffer, imageManager);

				queryPool.TimestampEnd(commandBuffer, Wrapper::TIMESTAMP_GRID_NEIGHBOR_UPDATE, frameIndex);
			}
		}
		else
		{
			uint32_t dispatchSize = static_cast<uint32_t>(mipMapNeighborInfos_.size());
			if (dispatchSize > 0)
			{
				PushConstantData levelPushConstantData = { static_cast<int>(neighborInfos_.size()) };
				vkCmdPushConstants(commandBuffer,
					bindingManager_->GetPipelineLayout(SUBPASS_VOLUME_ADAPTIVE_NEIGHBOR_UPDATE),
					VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantData),
					&levelPushConstantData);

				vkCmdDispatch(commandBuffer, dispatchSize, 1, 1);
				AddImageBarrier(commandBuffer, imageManager);
			}
		}
	}

	NeighborCells::Direction NeighborCells::GetOppositeDirection(Direction input)
	{
		switch (input)
		{
		case DIR_LEFT:
			return DIR_RIGHT;
		case DIR_RIGHT:
			return DIR_LEFT;
		case DIR_UP:
			return DIR_DOWN;
		case DIR_DOWN:
			return DIR_UP;
		case DIR_BACK:
			return DIR_FRONT;
		case DIR_FRONT:
			return DIR_BACK;
		}
		return DIR_MAX;
	}

	bool NeighborCells::EdgeCase(int pos)
	{
		return pos < minGridPos_ || pos > maxGridPos_;
	}

	bool NeighborCells::EdgeCase(int gridIndex, int direction)
	{
		int pos = -1;
		switch (direction)
		{
		case 0:
		case 1:
			pos = gridIndex % GridConstants::nodeResolution;
			break;
		case 2:
		case 3:
			pos = (gridIndex / GridConstants::nodeResolution) % GridConstants::nodeResolution;
			break;
		case 4:
		case 5:
			pos = gridIndex / nodeResolutionSquared_;
			break;
		}
		return EdgeCase(pos);
	}

	void NeighborCells::AddImageBarrier(VkCommandBuffer commandBuffer, ImageManager* imageManager)
	{
		ImageManager::BarrierInfo barrierInfo{};
		barrierInfo.imageIndex = atlasImageIndex_;
		barrierInfo.type = ImageManager::BARRIER_WRITE_WRITE;
		auto barriers = imageManager->Barrier({ barrierInfo });

		Wrapper::PipelineBarrierInfo pipelineBarrierInfo{};
		pipelineBarrierInfo.src = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		pipelineBarrierInfo.dst = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		pipelineBarrierInfo.AddImageBarriers(barriers);
		Wrapper::AddPipelineBarrier(commandBuffer, pipelineBarrierInfo);
	}

	void NeighborCells::AddImageCopy(int sourceImageIndex, int dstImageIndex, 
		Direction direction, bool mipMap)
	{
		if (NeighborInserted(sourceImageIndex, dstImageIndex, mipMap))
		{
			return;
		}

		const auto offsets = CalcOffsets(sourceImageIndex, dstImageIndex, direction);

		const VkImageSubresourceLayers subresourceLayers = {
			VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		VkImageCopy srcToDst = {};
		srcToDst.dstSubresource = srcToDst.srcSubresource = subresourceLayers;
		srcToDst.srcOffset = offsets.data[OFFSET_SRC_LOAD];
		srcToDst.dstOffset = offsets.data[OFFSET_DST_STORE];
		srcToDst.extent = GetExtent(direction);

		VkImageCopy dstToSrc = srcToDst;
		dstToSrc.srcOffset = offsets.data[OFFSET_DST_LOAD];
		dstToSrc.dstOffset = offsets.data[OFFSET_SRC_STORE];
		if (mipMap)
		{
			mipMapNeighborCopy_.push_back(srcToDst);
			mipMapNeighborCopy_.push_back(dstToSrc);
		}
		else
		{
			neighborCopy_.push_back(srcToDst);
			neighborCopy_.push_back(dstToSrc);
		}
		AddSet(sourceImageIndex, dstImageIndex, mipMap);
	}

	VkExtent3D NeighborCells::GetExtent(Direction direction)
	{
		VkExtent3D result = { GridConstants::imageResolution, GridConstants::imageResolution, GridConstants::imageResolution };
		switch (direction)
		{
		case DIR_LEFT:
		case DIR_RIGHT:
			result.width = 1;
			break;
		case DIR_UP:
		case DIR_DOWN:
			result.height = 1;
			break;
		case DIR_FRONT:
		case DIR_BACK:
			result.depth = 1;
			break;
		}
		return result;
	}

	void NeighborCells::AddSet(int sourceImageIndex, int dstImageIndex, bool mipMap)
	{
		std::pair<int, int> a = { sourceImageIndex, dstImageIndex };
		std::pair<int, int> b = { dstImageIndex,sourceImageIndex };
		if (mipMap)
		{
			mipMapNeighborSets_.insert(a);
			mipMapNeighborSets_.insert(b);
		}
		else
		{
			neighborSets_.insert(a);
			neighborSets_.insert(b);
		}
	}

	bool NeighborCells::NeighborInserted(int sourceImageIndex, int dstImageIndex, bool mipMap)
	{
		const std::pair<int, int> pair = { sourceImageIndex, dstImageIndex };
		if (mipMap)
		{
			return mipMapNeighborSets_.find(pair) != mipMapNeighborSets_.end();
		}
		else
		{
			return neighborSets_.find(pair) != neighborSets_.end();
		}
	}

	int NeighborCells::GetImageIndexFromOffset(Offsets offset)
	{
		switch (offset)
		{
		case OFFSET_SRC_LOAD:
		case OFFSET_SRC_STORE:
			return 0;
		case OFFSET_DST_LOAD:
		case OFFSET_DST_STORE:
			return 1;
		}
		return -1;
	}

	NeighborCells::NeighborOffsets NeighborCells::CalcOffsets(int srcIndex, int dstIndex, Direction direction)
	{
		std::array<glm::ivec3, 2> srcDstImageOffsets;
		srcDstImageOffsets[0] = CalcGridPos(srcIndex, atlasProperties_.sideLength) *
			GridConstants::imageResolution + glm::ivec3(1);
		srcDstImageOffsets[1] = CalcGridPos(dstIndex, atlasProperties_.sideLength) *
			GridConstants::imageResolution + glm::ivec3(1);

		glm::ivec3 dir{ 0 };
		dir[direction / 2] = 1;
		
		const auto oppositeDirection = GetOppositeDirection(direction);
		NeighborOffsets offsets;
		for (int i = 0; i < OFFSET_MAX; ++i)
		{
			const auto currDir = i <= OFFSET_DST_STORE ? direction : oppositeDirection;
			
			const int imageIndex = GetImageIndexFromOffset(static_cast<Offsets>(i));
			const bool store = i % 2 == 1;
			const glm::ivec3 offset = srcDstImageOffsets[imageIndex] + dir * 
				CalcTextureOffset(currDir, store);
			offsets.data[i] = { offset.x, offset.y, offset.z };
		}
		return offsets;
	}

	int NeighborCells::CalcTextureOffset(Direction direction, bool store)
	{
		if (store)
		{
			return direction % 2 == 0 ? -1 : GridConstants::imageResolution;
		}
		else
		{
			return direction % 2 == 1 ? 1 : GridConstants::imageResolution - 2;
		}
	}
}
