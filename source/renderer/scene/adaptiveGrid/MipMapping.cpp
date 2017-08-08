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
	MipMapping::MipMapping() :
		mipMapImageAtlas_{GridConstants::nodeResolution + 1}		//One pixel padding for sampling of the texture
	{}

	void MipMapping::RequestResources(ImageManager* imageManager, BufferManager* bufferManager, int frameCount, int atlasImageIndex)
	{
		mipMapImageAtlas_.RequestResources(imageManager, bufferManager, ImageManager::MEMORY_POOL_GRID_MIPMAP, frameCount);
		
		{
			BufferManager::BufferInfo cbInfo;
			cbInfo.typeBits = BufferManager::BUFFER_CONSTANT_BIT | BufferManager::BUFFER_SCENE_BIT;
			cbInfo.pool = BufferManager::MEMORY_CONSTANT;
			cbInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			cbInfo.bufferingCount = frameCount;
			cbInfo.data = &cbData_;
			cbInfo.size = sizeof(CBData);
			cbIndex_ = bufferManager->Ref_RequestBuffer(cbInfo);
		}

		{
			BufferManager::BufferInfo storageBufferInfo;
			storageBufferInfo.typeBits = BufferManager::BUFFER_GRID_BIT;
			storageBufferInfo.pool = BufferManager::MEMORY_GRID;
			storageBufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			storageBufferInfo.bufferingCount = frameCount;
			storageBufferInfo.size = sizeof(int);
			for (auto& bufferInfo : storageBuffers_)
			{
				bufferInfo.index = bufferManager->Ref_RequestBuffer(storageBufferInfo);
				bufferInfo.size = storageBufferInfo.size;
			}
		}

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
			bindingInfo.resourceIndex = { atlasImageIndex_, cbIndex_, 
				storageBuffers_[pass].index, mipMapImageAtlas_.GetImageIndex() };
			bindingInfo.stages = { VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_COMPUTE_BIT,
				VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_COMPUTE_BIT };
			bindingInfo.types = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE };
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
			perLevelMergingPushConstantIndex_ = bindingManager->RequestPushConstants(pushConstantInfo)[0];

			bindingInfo.pass = SUBPASS_VOLUME_ADAPTIVE_MIPMAPPING_MERGING;
			bindingInfo.resourceIndex = { atlasImageIndex_, mipMapImageAtlas_.GetImageIndex(),
				cbIndex_, storageBuffers_[pass].index };
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

	void MipMapping::UpdateMipMapNodes(const GridLevel* parentLevel, const GridLevel* childLevel)
	{
		//Clear the vectors on a new frame if the top most level is called
		if (!parentLevel->HasParent())
		{
			perChildData_.clear();
			mipmapIndices_.clear();
			perLevelDataChild_.clear();
			perLevelChildCount_.clear();
			mipmapCount_ = 0;

			perParentData_.clear();
			perLevelDataParent_.clear(); 
			perLevelParentCount_.clear();
		}

		LevelData_New levelData;
		levelData.averagingStart = static_cast<uint32_t>(perChildData_.size());

		const auto& nodeDataParent = parentLevel->GetNodeData();
		const auto& nodeInfoParent = nodeDataParent.GetNodeInfos();
		const int parentNodeCount = nodeDataParent.GetNodeCount();
		perLevelDataParent_.push_back({ static_cast<uint32_t>(mipmapCount_ )});
		const auto& childNodeIndices = parentLevel->GetChildIndices();

		const auto& nodeDataChild = childLevel->GetNodeData();
		const bool childIsLeafLevel = childLevel->IsLeafLevel();

		//Offset into the child nodes, updated after each parent node
		int childNodeOffset = 0;
		//parent node offset is used to remove the per level offset from the child indices
		int parentNodeOffset = 0;
		if (!childNodeIndices.empty())
		{
			parentNodeOffset = -parentLevel->GetChildOffset();
		}
		//go through all parent nodes and collect all child nodes that contribute to mipmapping
		for (int parentNodeIndex = 0; parentNodeIndex < parentNodeCount; ++parentNodeIndex)
		{
			const int nodeCount = nodeDataParent.childCount_[parentNodeIndex];
			//for each child store the start in the image atlas 
			for (int i = childNodeOffset; i < nodeCount + childNodeOffset; ++i)
			{
				const int childNodeIndex = childNodeIndices[i] + parentNodeOffset;
				perChildData_.push_back(GetChildNodeData(nodeDataChild, childNodeIndex, childIsLeafLevel));
				mipmapIndices_.push_back(mipmapCount_);
			}
			//Advance the number of needed mipmaps for parent nodes with active children
			if (nodeCount > 0)
			{
				const auto& parentNodeInfo = nodeInfoParent[parentNodeIndex];
				ParentNodeData parentData;
				parentData.imageAtlasOffset = parentNodeInfo.textureOffset;
				parentData.imageAtlas_mipmapOffset = parentNodeInfo.textureOffsetMipMap;
				parentData.mipmapOffset = mipmapCount_;
				perParentData_.push_back(parentData);
				
				mipmapCount_++;
			}

			childNodeOffset += nodeCount;
		}

		perLevelDataChild_.push_back(levelData);
		perLevelChildCount_.push_back(childNodeOffset);
		perLevelParentCount_.push_back(mipmapCount_ - perLevelDataParent_.back().averagingStart);
	}
	
	void MipMapping::UpdateGpuResources(BufferManager* bufferManager, int frameIndex)
	{
		if (perChildData_.empty())
		{
			return;
		}

		const int bufferTypeBits = BufferManager::BUFFER_GRID_BIT;
		for (int i = 0; i < MIPMAP_MAX; ++i)
		{
			const int bufferIndex = storageBuffers_[i].index;
			auto bufferPtr = bufferManager->Ref_Map(bufferIndex, frameIndex, bufferTypeBits);

			void* source = nullptr;
			VkDeviceSize copySize = CalcStorageBufferSize(static_cast<Passes>(i));

			switch (i)
			{
			case MIPMAP_AVERAGING:
				source = perChildData_.data();
				break;
			case MIPMAP_MERGING:
				source = perParentData_.data();
				break;
			}

			memcpy(bufferPtr, source, copySize);
			bufferManager->Ref_Unmap(bufferIndex, frameIndex, bufferTypeBits);
		}
	}

	bool MipMapping::ResizeGpuResources(ImageManager* imageManager, std::vector<ResourceResize>& resourceResizes)
	{
		UpdateImageOffsets(imageManager);

		bool resize = false;
		for (int i = 0; i < MIPMAP_MAX; ++i)
		{
			const auto newSize = CalcStorageBufferSize(static_cast<Passes>(i));
			if (storageBuffers_[i].size < newSize)
			{
				resize = true;
				storageBuffers_[i].size = newSize;
			}
			resourceResizes.push_back(storageBuffers_[i]);
		}
		return resize;
	}

	void MipMapping::Dispatch(ImageManager* imageManager, BufferManager* bufferManager, VkCommandBuffer commandBuffer, int frameIndex, int level, int pass)
	{
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
			const uint32_t dispatchCount = static_cast<uint32_t>(perLevelChildCount_[level]);

			if (dispatchCount > 0)
			{
				auto& queryPool = Wrapper::QueryPool::GetInstance();
				queryPool.TimestampStart(commandBuffer, timeStampMipMapping, frameIndex);

				vkCmdPushConstants(commandBuffer, bindingManager_->GetPipelineLayout(SUBPASS_VOLUME_ADAPTIVE_MIPMAPPING),
					VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int), &perLevelDataChild_[level].averagingStart);
				vkCmdDispatch(commandBuffer, dispatchCount, 1, 1);

				queryPool.TimestampEnd(commandBuffer, timeStampMipMapping, frameIndex);
			}

			AddBarriers(imageManager, commandBuffer, frameIndex);
		}
		break;
		case MIPMAP_MERGING:
		{
			const uint32_t dispatchCount = static_cast<uint32_t>(perLevelParentCount_[level]);

			auto& queryPool = Wrapper::QueryPool::GetInstance();
			queryPool.TimestampStart(commandBuffer, timeStampMipMappingMerging, frameIndex);
			
			vkCmdPushConstants(commandBuffer, bindingManager_->GetPipelineLayout(SUBPASS_VOLUME_ADAPTIVE_MIPMAPPING_MERGING),
				VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int), &perLevelDataParent_[level].averagingStart);
			vkCmdDispatch(commandBuffer, dispatchCount, 1, 1);
			
			queryPool.TimestampEnd(commandBuffer, timeStampMipMappingMerging, frameIndex);

			const bool firstLevel = level == perLevelDataChild_.size() - 1;
			AddMergingBarrier(imageManager, commandBuffer, frameIndex, !firstLevel);
		}
		break;
		}
	}

	MipMapping::ChildNodeData MipMapping::GetChildNodeData(const Renderer::NodeData& data, int childNodeIndex, bool leafLevel)
	{
		const auto& nodeInfo = data.GetNodeInfos()[childNodeIndex];

		ChildNodeData childNodeData = {};
		childNodeData.imageOffset = leafLevel ? nodeInfo.textureOffset : nodeInfo.textureOffsetMipMap;
		//The parent texel is based on the node position of the child node
		childNodeData.parentTexel = NodeData::PackTextureOffset(data.gridPos_[childNodeIndex]);
		return childNodeData;
	}

	void MipMapping::UpdateImageOffsets(ImageManager* imageManager)
	{
		mipMapImageAtlas_.UpdateSize(mipmapCount_);

		const int atlasSideLength = mipMapImageAtlas_.GetSideLength();
		const int atlasResolution = mipMapImageAtlas_.GetResolution();
		//Add the offsets into the image atlas to the texel coordinates
		for (size_t i = 0; i < mipmapIndices_.size(); ++i)
		{
			const int mipmapIndex = mipmapIndices_[i];
			const auto atlasOffset = Math::Index1Dto3D(mipmapIndex, atlasSideLength)
				* atlasResolution;
			perChildData_[i].parentTexel += NodeData::PackTextureOffset(atlasOffset);
			perParentData_[mipmapIndex].mipmapOffset = perChildData_[i].parentTexel;
		}
		
		mipMapImageAtlas_.ResizeImage(imageManager);
	}
	
	void MipMapping::UpdateAtlasProperties(int atlasSideLength, int atlasResolution)
	{
		cbData_.atlasSideLength_Reciprocal = 1.0f / atlasSideLength;
		cbData_.texelSize = 1.0f / (atlasResolution * atlasSideLength);
		cbData_.texelSize_Half = cbData_.texelSize * 0.5f;
		cbData_.texelSizeMipmap = 1.0f / (mipMapImageAtlas_.GetResolution() * mipMapImageAtlas_.GetSideLength());
	}

	int MipMapping::CalcStorageBufferSize(MipMapping::Passes pass)
	{
		switch (pass)
		{
		case MIPMAP_AVERAGING:
			return static_cast<int>(sizeof(ChildNodeData) * std::max(perChildData_.size(), 1ull));
		case MIPMAP_MERGING:
			return static_cast<int>(sizeof(ParentNodeData) * std::max(perParentData_.size(), 1ull));
		default:
			printf("Invalide mip mapping buffer type %d\n", pass);
			return 0;
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
}