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

#include "GridLevel.h"

#include "..\..\resources\ImageManager.h"
#include "..\..\resources\BufferManager.h"
#include "..\..\passes\GuiPass.h"

#include "AdaptiveGridConstants.h"

#include <glm\gtc\matrix_transform.hpp>

namespace Renderer
{
	GridLevel::GridLevel(int gridResolution) :
		gridResolution_{ gridResolution },
		nodeData_{}
	{
	}

	void GridLevel::SetWorldExtend(const glm::vec3& min, const glm::vec3 extent, float globalResolution)
	{
		const auto scale = extent / globalResolution;
		gridToWorld_ = glm::scale(glm::translate(glm::mat4(1.0f), min), scale);
		gridCellSize_ = scale.x;
		imageTexelSize_ = gridCellSize_ / GridConstants::imageResolution;
	}

	void GridLevel::SetParentLevel(GridLevel* parentLevel)
	{
		parentLevel_ = parentLevel;
	}

	void GridLevel::Reset()
	{
		nodeBoundingBoxes_.clear();
		nodeData_.Clear();
		gridToNodeMapping_.clear();
		childNodeIndices_.clear();
		parentChildIndices_.clear();
		imageOffset_ = 0;
		imageCounter_ = 0;
	}

	int GridLevel::AddNode(const glm::vec3& gridPos_World)
	{
		auto gridPosParent = glm::vec3();
		auto indexGridParent = 0; 
		if (parentLevel_)
		{
			gridPosParent = parentLevel_->CalcGridPos_Level(gridPos_World);
			indexGridParent = parentLevel_->CalcIndex_Grid(gridPosParent);
		}
		
		const auto gridPosNode = CalcGridPos_Node(gridPos_World, gridPosParent);
		const auto indexGrid = CalcIndex_Grid(gridPosNode);

		const auto parentNodeKey = std::pair<int, int>(indexGridParent, indexGrid);

		const auto& gridToNode = gridToNodeMapping_.find(parentNodeKey);
		//Node does not exist, add it
		if (gridToNode == gridToNodeMapping_.end())
		{
			int indexNodeParent = 0;
			if (parentLevel_)
			{
				indexNodeParent = parentLevel_->AddNode(gridPos_World);
			}
			
			const int indexNode = nodeData_.AddNode(gridPosNode, indexGrid, indexNodeParent, imageCounter_++);
			gridToNodeMapping_[parentNodeKey] = indexNode;
			if (parentLevel_)
			{
				parentLevel_->SetChildIndex(indexGrid, indexNode, indexNodeParent);
			}
			return indexNode;
		}
		else
		{
			return gridToNode->second;
		}
	}

	void GridLevel::SetChildIndex(int childIndexGrid, int childIndexNode, int indexNode)
	{
		nodeData_.SetBit(indexNode, childIndexGrid);
		parentChildIndices_[indexNode][childIndexGrid] = childIndexNode;
	}

	glm::vec3 GridLevel::CalcGridPos_Level(const glm::vec3& gridPos_World)
	{
		return floor(gridPos_World / gridCellSize_);
	}

	glm::vec3 GridLevel::CalcGridPos_Node(const glm::vec3& gridPos_World, const glm::vec3& parentGridPos_Level)
	{
		glm::vec3 parentOffset = glm::vec3();
		if (parentLevel_)
		{
			parentOffset = parentGridPos_Level *
				static_cast<float>(parentLevel_->gridResolution_);
		}
		return CalcGridPos_Level(gridPos_World) - parentOffset;
	}

	int GridLevel::CalcIndex_Grid(const glm::vec3& gridPos_Node)
	{
		glm::ivec3 pos = gridPos_Node;
		return (pos.z * gridResolution_ + pos.y) * gridResolution_ + pos.x;
	}

	int GridLevel::Update(const int parentChildOffset)
	{
		parentImageOffset_ = 0;
		if (parentLevel_)
		{
			parentImageOffset_ = parentLevel_->GetImageOffset();
		}
		
		imageOffset_ = nodeData_.GetNodeCount() + parentImageOffset_;
		
		int childOffset = parentChildOffset;
		for (const auto& parentChildIndices : parentChildIndices_)
		{
			const int parentIndexNode = parentChildIndices.first;
			const auto& childIndices = parentChildIndices.second;
			
			nodeData_.SetChildOffset(childOffset, parentIndexNode);
			for (const auto& childIndex : childIndices)
			{
				childNodeIndices_.push_back(childIndex.second + parentChildOffset + 1);
			}
			childOffset += static_cast<int>(childIndices.size());

			//Update mipmap data
			if (!childIndices.empty())
			{
				nodeData_.UpdateMipMap(imageOffset_, parentIndexNode);
				imageOffset_++;
			}
		}
		childOffset_ = parentChildOffset + 1;

		return childOffset;
	}

	void GridLevel::UpdateImageIndices(int atlasSideLength)
	{
		nodeData_.UpdateAtlasSideLength(atlasSideLength);
		nodeData_.UpdateImageOffsets(parentImageOffset_);
	}

	int GridLevel::CopyBufferData(void* dst, BufferType type, int offset)
	{
		void* dataOffset = static_cast<char*>(dst) + offset;
		
		void* src = nullptr;
		int size = 0;

		switch (type)
		{
		case BUFFER_NODE_INFOS:
			src = nodeData_.GetNodeInfoData();
			size = GetNodeInfoSize();
			break;
		case BUFFER_ACTIVE_BITS:
			src = nodeData_.activeBits_.data();
			size = GetActiveBitSize();
			break;
		case BUFFER_BIT_COUNTS:
			src = nodeData_.bitCounts_.data();
			size = GetBitCountsSize();
			break;
		case BUFFER_CHILDS:
			src = childNodeIndices_.data();
			size = GetChildSize();
			break;
		default:
			printf("Wrong type %d for copying grid level data\n", type);
			break;
		}

		memcpy(dataOffset, src, size);
		return size + offset;
	}

	int GridLevel::FindIndexNode(int parentIndexNode, int indexGrid) const
	{
		int indexNode = -1;
		const auto& gridToNode = gridToNodeMapping_.find({ parentIndexNode, indexGrid });
		if (gridToNode != gridToNodeMapping_.end())
		{
			indexNode = gridToNode->second;
		}
		return indexNode;
	}

	void GridLevel::UpdateBoundingBoxes()
	{
		const auto& gridPositions = nodeData_.gridPos_;
		const auto& parentIndices = nodeData_.parentIndices_;
		for (size_t i = 0; i < gridPositions.size(); ++i)
		{
			AxisAlignedBoundingBox nodeBB;
			nodeBB.min = gridPositions[i];
			nodeBB.max = nodeBB.min + glm::vec3(1.0f);

			nodeBoundingBoxes_.push_back(ApplyTranslationScale(nodeBB, gridToWorld_));
			
		}
		if (parentLevel_)
		{
			const auto& parentPositions = parentLevel_->GetNodeData().gridPos_;
			float parentGridCellSize = parentLevel_->GetGridCellSize();
			for (size_t i = 0; i < gridPositions.size(); ++i)
			{
				const auto& parentOffset = parentPositions[parentIndices[i]] * parentGridCellSize;
				nodeBoundingBoxes_[i].min += parentOffset;
				nodeBoundingBoxes_[i].max += parentOffset;
			}
		}
	}
}