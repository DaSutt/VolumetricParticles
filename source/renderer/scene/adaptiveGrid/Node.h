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

namespace Renderer
{
  struct NodeInfo
  {
    uint32_t textureOffset;				//x,y,z 10 bit leafNodeBit, mipmapBit
		uint32_t textureOffsetMipMap;
		int childOffset;
  };

	struct ImageInfo
	{
		glm::ivec3 image;
		int imageIndex;
		glm::ivec3 mipMap;
		int mipMapImageIndex;
	};

  class NodeData
  {
  public:
    NodeData();
    int AddNode(glm::ivec3 nodePos, int gridIndex, int parentIndex, int textureIndex);
    void SetBit(int nodeIndex, int bit);
    void Clear();

		void UpdateAtlasSideLength(int sideLength) { atlasSideLength_ = sideLength; }
		void UpdateImageOffsets(const int parentImageOffset);
		void UpdateMipMap(int imageIndex, int indexNode);
		void SetChildOffset(int offset, int nodeIndex);

		std::vector<ImageInfo> imageInfos_;

		std::vector<int> nodeGridIndex;
		std::vector<int> parentIndices_;
		std::vector<glm::vec3> gridPos_;
		std::vector<uint32_t> activeBits_;
    std::vector<int> bitCounts_;
		std::vector<int> childCount_;

    int GetNodeCount()const { return nodeCount_; }
    int GetNodeSize()const { return nodebitCount_; }
		auto& GetNodeInfos() const { return nodeInfos_; }
		void* GetNodeInfoData() { return nodeInfos_.data(); }

		int GetNodeInfoSize() const { return static_cast<int>(nodeInfos_.size() * sizeof(NodeInfo)); }
		
		static uint32_t PackTextureOffset(const glm::ivec3& textureOffset);
		static bool MipMapSet(const uint32_t imageOffset);
  private:
		void SetMipMapBit(uint32_t& textureOffset);
    
		std::vector<NodeInfo> nodeInfos_;
		
		const int nodebitCount_;
    int nodeCount_;
		static int atlasSideLength_;
  };
}