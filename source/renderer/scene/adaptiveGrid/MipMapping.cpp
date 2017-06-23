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

#include "MipMapping.h"

#include "GridLevel.h"
#include "AdaptiveGridConstants.h"
#include "..\..\resources\BufferManager.h"
#include "..\..\resources\ImageManager.h"
#include "..\..\passResources\ShaderBindingManager.h"
#include "..\..\wrapper\Barrier.h"
#include "..\..\wrapper\QueryPool.h"
#include "..\..\..\utility\Math.h"

namespace Renderer
{
	void MipMapping::RequestResources(ImageManager* imageManager, BufferManager* bufferManager, int frameCount, int atlasImageIndex)
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
		imageIndexInfo.size = sizeof(int);
		for (auto& bufferInfo : buffers_)
		{
			bufferInfo.index = bufferManager->Ref_RequestBuffer(imageIndexInfo);
			bufferInfo.size = imageIndexInfo.size;
		}

		mipMapImageAtlas_.RequestResources(imageManager, bufferManager, ImageManager::MEMORY_POOL_GRID_MIPMAP,  frameCount);

		atlasImageIndex_ = atlasImageIndex;
	}

	int MipMapping::GetShaderBinding(ShaderBindingManager* bindingManager, int frameCount, int pass)
	{
		ShaderBindingManager::BindingInfo bindingInfo = {};
		bindingInfo.setCount = frameCount;
		
		bindingManager_ = bindingManager;

		switch (pass)
		{
		case MIPMAP_AVERAGING:
		{
			ShaderBindingManager::PushConstantInfo pushConstantInfo{};
			VkPushConstantRange objectIds = {};
			objectIds.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			objectIds.offset = 0;
			objectIds.size = sizeof(int);
			pushConstantInfo.pushConstantRanges.push_back(objectIds);
			pushConstantInfo.pass = SUBPASS_VOLUME_ADAPTIVE_MIPMAPPING;
			perLevelPushConstantIndex_ = bindingManager->RequestPushConstants(pushConstantInfo)[0];

			bindingInfo.pass = SUBPASS_VOLUME_ADAPTIVE_MIPMAPPING;
			bindingInfo.resourceIndex = { atlasImageIndex_, mipMapImageAtlas_.GetImageIndex(), 
				cbIndex_, buffers_[GPU_STORAGE_MIPMAP_INFO].index };
			bindingInfo.stages = { VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_COMPUTE_BIT,
				VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_COMPUTE_BIT };
			bindingInfo.types = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER };
			bindingInfo.Ref_image = true;
			bindingInfo.refactoring_ = { true, true, true, true };
		} break;
		case MIPMAP_MERGING:
		{
			ShaderBindingManager::PushConstantInfo pushConstantInfo{};
			VkPushConstantRange objectIds = {};
			objectIds.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			objectIds.offset = 0;
			objectIds.size = sizeof(int);
			pushConstantInfo.pushConstantRanges.push_back(objectIds);
			pushConstantInfo.pass = SUBPASS_VOLUME_ADAPTIVE_MIPMAPPING_MERGING;
			perLevelMergingPushConstant_ = bindingManager->RequestPushConstants(pushConstantInfo)[0];

			bindingInfo.pass = SUBPASS_VOLUME_ADAPTIVE_MIPMAPPING_MERGING;
			bindingInfo.resourceIndex = { atlasImageIndex_, mipMapImageAtlas_.GetImageIndex(),
				cbIndex_, buffers_[GPU_STORAGE_IMAGE_OFFSETS].index };
			bindingInfo.stages = { VK_SHADER_STAGE_COMPUTE_BIT,VK_SHADER_STAGE_COMPUTE_BIT,
				VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_COMPUTE_BIT };
			bindingInfo.types = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER };
			bindingInfo.Ref_image = true;
			bindingInfo.refactoring_ = { true, true, true, true };
		} break;
		}

		return bindingManager->RequestShaderBinding(bindingInfo);
	}

	void MipMapping::UpdateAtlasProperties(int sideLength)
	{
		atlasSideLength_ = sideLength;

		const int texelCount = GridConstants::imageResolutionOffset * atlasSideLength_;

		cbData_.imageResolutionOffsetRec = 1.0f / (texelCount);
		cbData_.atlasTexelToTexCoord = 1.0f / (texelCount);
		const float texelSize = 1.0f / static_cast<float>(texelCount);
		cbData_.texCoordOffset = 0.5f * texelSize;
	}

	void MipMapping::UpdateMipMapNodes(const GridLevel* parentLevel, const GridLevel* childLevel)
	{
		//Clear the vectors on a new frame
		if (!parentLevel->HasParent())
		{
			mipMapData_.clear();
			mipMapOffsets_.clear();

			imageOffsets_.clear();
			levelData_.clear();
			mipMapCount_ = 0;
		}

		LevelData levelData;
		levelData.averagingOffset = static_cast<int>(mipMapData_.size());
		levelData.mergingOffset = static_cast<int>(imageOffsets_.size());
		
		const auto& nodeDataParent = parentLevel->GetNodeData();
		const auto& childNodeIndices = parentLevel->GetChildIndices();

		const auto& nodeDataChild = childLevel->GetNodeData();
		const bool childLeafLevel = childLevel->IsLeafLevel();

		const int parentNodeCount = nodeDataParent.GetNodeCount();

		const auto& nodeInfoChilds = nodeDataChild.GetNodeInfos();
		const auto& nodeInfosParent = nodeDataParent.GetNodeInfos();
		int childIndexOffset = 0;

		const auto imageOffset = NodeData::PackTextureOffset({ 1,1,1 });
		//go through all parent nodes and collect all child nodes that contribute to mip mapping
		for (int parentNodeIndex = 0; parentNodeIndex < parentNodeCount; ++parentNodeIndex)
		{
			const int nodeCount = nodeDataParent.childCount_[parentNodeIndex];
			uint32_t childImageOffset = 0;

			//For each child store the start in the image atlas without edge values
			for (int i = childIndexOffset; i < nodeCount + childIndexOffset; ++i)
			{
				const int childNodeIndex = childNodeIndices[i];
				//On the leaf level the original textures are used for averaging
				if (childLeafLevel)
				{
					childImageOffset = nodeInfoChilds[childNodeIndex].textureOffset;
				}
				//On coarser levels use the mip map values to accord for the more detailed levels as well
				else
				{
					childImageOffset = nodeInfoChilds[childNodeIndex].textureOffsetMipMap;
				}
				childImageOffset += imageOffset;
				const auto parentTexelOffset = NodeData::PackTextureOffset( nodeDataChild.gridPos_[childNodeIndex] ) 
					+ imageOffset;

				mipMapData_.push_back({ childImageOffset, parentTexelOffset });
				mipMapOffsets_.push_back(mipMapCount_);
			}

			//Skip parent nodes without any children
			if (nodeCount > 0)
			{
				childIndexOffset += nodeCount;
				
				NodeImageOffsets offsets;
				offsets.imageOffset = nodeInfosParent[parentNodeIndex].textureOffset;
				offsets.mipMapImageOffset = nodeInfosParent[parentNodeIndex].textureOffsetMipMap;
				offsets.mipMapSourceOffset = static_cast<uint32_t>(imageOffsets_.size());
				imageOffsets_.push_back(offsets);
				
				mipMapCount_++;
			}
		}

		levelData.childImageCount = static_cast<uint32_t>(mipMapData_.size() - levelData.averagingOffset);
		levelData.mipMapCount = static_cast<uint32_t>(imageOffsets_.size() - levelData.mergingOffset);
		levelData_.push_back(levelData);
	}

	void MipMapping::UpdateGpuResources(BufferManager* bufferManager, int frameIndex)
	{
		if (mipMapData_.empty())
		{
			return;
		}

		const int bufferTypeBits = BufferManager::BUFFER_GRID_BIT;
		for (int i = 0; i < GPU_MAX; ++i)
		{
			const int bufferIndex = buffers_[i].index;
			auto bufferPtr = bufferManager->Ref_Map(bufferIndex, frameIndex, bufferTypeBits);
		
			void* source = nullptr;
			VkDeviceSize copySize = 0;
		
			switch (i)
			{
			case GPU_STORAGE_MIPMAP_INFO:
				source = mipMapData_.data();
				copySize = mipMapData_.size() * sizeof(MipMapInfo);
				break;
			case GPU_STORAGE_IMAGE_OFFSETS:
				source = imageOffsets_.data();
				copySize = imageOffsets_.size() * sizeof(NodeImageOffsets);
				break;
			}
		
			memcpy(bufferPtr, source, copySize);
			bufferManager->Ref_Unmap(bufferIndex, frameIndex, bufferTypeBits);
		}				
	}

	int MipMapping::CalcSize(int bufferType)
	{
		switch (bufferType)
		{
		case GPU_STORAGE_MIPMAP_INFO:
			return static_cast<int>(sizeof(MipMapInfo) * std::max(mipMapData_.size(), 1ull));
		case GPU_STORAGE_IMAGE_OFFSETS:
			return static_cast<int>(sizeof(NodeImageOffsets) * std::max(imageOffsets_.size(), 1ull));
		default:
			printf("Invalide mip mapping buffer type %d\n", bufferType);
			return 0;
		}
	}

	bool MipMapping::ResizeGpuResources(ImageManager* imageManager, std::vector<ResourceResize>& resourceResizes)
	{
		UpdateImageOffsets(imageManager);

		bool resize = false;
		for (int i = 0; i < GPU_MAX; ++i)
		{
			const auto newSize = CalcSize(i);
			if (buffers_[i].size < newSize)
			{
				resize = true;
				buffers_[i].size = newSize;
			}
			resourceResizes.push_back({ buffers_[i].size, buffers_[i].index });
		}
		return resize;
	}

	void MipMapping::Dispatch(ImageManager* imageManager, BufferManager* bufferManager, VkCommandBuffer commandBuffer, int frameIndex, int level, int pass)
	{
		//Skip empty levels
		if (levelData_[level].childImageCount <= 0)
		{
			return;
		}

		Wrapper::TimeStamps timeStampMipMapping;
		Wrapper::TimeStamps timeStampMipMappingMerging;
		switch (level)
		{
		case 0:
			timeStampMipMapping = Wrapper::TIMESTAMP_GRID_MIPMAPPING_0;
			timeStampMipMappingMerging = Wrapper::TIMESTAMP_GRID_MIPMAPPING_MERGIN_0;
			break;
		case 1:
			timeStampMipMapping = Wrapper::TIMESTAMP_GRID_MIPMAPPING_1;
			timeStampMipMappingMerging = Wrapper::TIMESTAMP_GRID_MIPMAPPING_MERGIN_1;
			break;
		}

		switch (pass)
		{
		case MIPMAP_AVERAGING:
		{
			auto& queryPool = Wrapper::QueryPool::GetInstance();
			queryPool.TimestampStart(commandBuffer, timeStampMipMapping, frameIndex);
			
			vkCmdPushConstants(commandBuffer, bindingManager_->GetPipelineLayout(SUBPASS_VOLUME_ADAPTIVE_MIPMAPPING),
				VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int), &levelData_[level].averagingOffset);
			vkCmdDispatch(commandBuffer, levelData_[level].childImageCount, 1, 1);

			queryPool.TimestampEnd(commandBuffer, timeStampMipMapping, frameIndex);

			AddBarriers(imageManager, commandBuffer, frameIndex);
		}
		break;
		case MIPMAP_MERGING:
		{
			auto& queryPool = Wrapper::QueryPool::GetInstance();
			queryPool.TimestampStart(commandBuffer, timeStampMipMappingMerging, frameIndex);

			vkCmdPushConstants(commandBuffer, bindingManager_->GetPipelineLayout(SUBPASS_VOLUME_ADAPTIVE_MIPMAPPING_MERGING),
				VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int), &levelData_[level].mergingOffset);
			vkCmdDispatch(commandBuffer, levelData_[level].mipMapCount, 1, 1);

			queryPool.TimestampEnd(commandBuffer, timeStampMipMappingMerging, frameIndex);

			const bool firstLevel = level == levelData_.size() - 1;
			AddMergingBarrier(imageManager, commandBuffer, frameIndex, !firstLevel);
		}
		break;
		}
	}

	void MipMapping::AddBarriers(ImageManager* imageManager, VkCommandBuffer commandBuffer, int frameIndex)
	{
		Wrapper::PipelineBarrierInfo pipelineBarrierInfo{};
		pipelineBarrierInfo.src = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		pipelineBarrierInfo.dst = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
				
		
		ImageManager::BarrierInfo imageAtlasBarrier{};
		imageAtlasBarrier.type = ImageManager::BARRIER_WRITE_WRITE;
		imageAtlasBarrier.imageIndex = atlasImageIndex_;
		ImageManager::BarrierInfo mipMapAtlasBarrier{};
		mipMapAtlasBarrier.type = ImageManager::BARRIER_WRITE_READ;
		mipMapAtlasBarrier.imageIndex = mipMapImageAtlas_.GetImageIndex();
		
		auto imageBarriers = imageManager->Barrier({ imageAtlasBarrier, mipMapAtlasBarrier });
		pipelineBarrierInfo.AddImageBarriers(imageBarriers);
				
		Wrapper::AddPipelineBarrier(commandBuffer, pipelineBarrierInfo);
	}

	void MipMapping::AddMergingBarrier(ImageManager* imageManager, VkCommandBuffer commandBuffer, int frameIndex, bool lastLevel)
	{
		Wrapper::PipelineBarrierInfo pipelineBarrierInfo{};
		pipelineBarrierInfo.src = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		pipelineBarrierInfo.dst = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

		ImageManager::BarrierInfo imageAtlasBarrier{};
		imageAtlasBarrier.imageIndex = atlasImageIndex_;

		//Writing mip map values must be finished
		if (lastLevel)
		{
			//After last level atlasImage is read only
			imageAtlasBarrier.type = ImageManager::BARRIER_WRITE_READ;
		}
		else
		{
			imageAtlasBarrier.type = ImageManager::BARRIER_WRITE_WRITE;
		}
		ImageManager::BarrierInfo mipMapAtlasBarrier{};
		mipMapAtlasBarrier.imageIndex = mipMapImageAtlas_.GetImageIndex();
		mipMapAtlasBarrier.type = ImageManager::BARRIER_READ_WRITE;

		auto barriers = imageManager->Barrier({ imageAtlasBarrier, mipMapAtlasBarrier});

		pipelineBarrierInfo.AddImageBarriers(barriers);
		Wrapper::AddPipelineBarrier(commandBuffer, pipelineBarrierInfo);
	}

	void MipMapping::UpdateImageOffsets(ImageManager* imageManager)
	{
		mipMapImageAtlas_.UpdateSize(static_cast<int>(imageOffsets_.size()));
		
		const auto atlasSideLength = mipMapImageAtlas_.GetSideLength();
		for (size_t i = 0; i < mipMapData_.size(); ++i)
		{
			const auto atlasOffset = Math::Index1Dto3D(mipMapOffsets_[i], atlasSideLength) *
				GridConstants::imageResolutionOffset;
			mipMapData_[i].parentTexel += NodeData::PackTextureOffset(atlasOffset);
		}

		for (auto& offset : imageOffsets_)
		{
			const auto atlasOffset = Math::Index1Dto3D(offset.mipMapSourceOffset, atlasSideLength)
				* GridConstants::imageResolutionOffset + 1;
			offset.mipMapSourceOffset = NodeData::PackTextureOffset(atlasOffset);
		}

		mipMapImageAtlas_.Resize(imageManager);
	}
}