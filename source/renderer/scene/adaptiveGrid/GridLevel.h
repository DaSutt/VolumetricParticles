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

#include <glm\glm.hpp>
#include <vector>
#include <set>

#include "Node.h"
#include "..\..\..\scene\components\AABoundingBox.h"

namespace Renderer
{
  class ImageManager;
  class BufferManager;

  class GridLevel
  {
  public:
    struct NodePushData
    {
      int indexStart;
      int indexEnd;
      int imageIndex;
    };

		enum BufferType
		{
			BUFFER_NODE_INFOS,
			BUFFER_ACTIVE_BITS,
			BUFFER_BIT_COUNTS,
			BUFFER_CHILDS,
			BUFFER_MAX
		};

    GridLevel(int gridResolution);

    void SetWorldExtend(const glm::vec3& min, const glm::vec3 extent, float globalResolution);
		void SetParentLevel(GridLevel* parentLevel);

		void Reset();

		//Add node at grid world space pos to current level, parent level is added as well if not already existing
		//If a new node was created the parent level is updated
		//Return the node index of the added node
		int AddNode(const glm::vec3& gridPos_World);
		void SetChildIndex(int childIndexGrid, int childIndexNode, int indexNode);
		void SetLeafLevel() { leafLevel_ = true; }

		//Calculate the grid pos on this level 
		glm::vec3 CalcGridPos_Level(const glm::vec3& gridPos_World);
		int CalcIndex_Grid(const glm::vec3& gridPos_Node);

		//Updates the offsets for the active child nodes
		//Returns the overall offset into the child indices array
		int Update(const int parentChildOffset);
		void UpdateImageIndices(int atlasSideLength);
		//Returns new offset after insertion
		int CopyBufferData(void* dst, BufferType type, int offset);
		
		float GetGridCellSize() const { return gridCellSize_; }
		const auto& GetNodeData() const { return nodeData_; }
		const auto& GetChildIndices() const { return childNodeIndices_; }

		int GetNodeInfoSize() const { return nodeData_.GetNodeInfoSize(); }
		int GetActiveBitSize() const { return static_cast<int>(nodeData_.activeBits_.size() * sizeof(uint32_t)); }
		int GetBitCountsSize() const { return static_cast<int>(nodeData_.bitCounts_.size() * sizeof(int)); }
		int GetChildSize() const { return static_cast<int>(sizeof(int) * childNodeIndices_.size()); }
		int GetNodeCount() const { return nodeData_.GetNodeCount(); }
		int GetImageOffset() const { return imageOffset_; }

		//Returns -1 if no valid node exists
		int FindIndexNode(int parentIndexGrid, int indexGrid) const;

		const int gridResolution_ = 0;
		const int childResolution_ = 0;

		void UpdateBoundingBoxes();
		std::vector<AxisAlignedBoundingBox> nodeBoundingBoxes_;

		int GetMaxImageOffset() const { return imageOffset_; }
		bool HasParent() const { return parentLevel_ != nullptr; }
		bool IsLeafLevel() const { return leafLevel_; }
	private:
		//Calculate the grid pos on this level for a specific node by taking the parent offset into account
		glm::vec3 CalcGridPos_Node(const glm::vec3& gridPos_World, const glm::vec3& parentGridPos_Level);

		GridLevel* parentLevel_ = nullptr;
		//Matrix used for debugging 
		glm::mat4 gridToWorld_;

		NodeData nodeData_;
    std::vector<int> childNodeIndices_;
		//Pair with parent node index and grid index
		std::map<std::pair<int,int>, int> gridToNodeMapping_;
		//map key is parentnode, other map has indexGrid and indexNode mapping
		std::map<int, std::map<int,int>> parentChildIndices_;
		int imageCounter_ = 0;
		int imageOffset_ = 0;
		int parentImageOffset_ = 0;

		float gridCellSize_ = 0.0f;
		float imageTexelSize_ = 0.0f;

		//True if this is the most detailed level of the tree
		bool leafLevel_ = false;
  };

  inline int CalcGridIndex(int x, int y, int z, int resolution)
  {
    return (z * resolution + y) * resolution + x;
  }

  inline glm::ivec3 CalcGridPos(int index, int resolution)
  {
    glm::ivec3 pos;
    const auto res2 = resolution * resolution;
    pos.z = index / res2;
    pos.y = static_cast<int>(index / resolution) % resolution;
    pos.x = index % resolution;
    return pos;
  }
}