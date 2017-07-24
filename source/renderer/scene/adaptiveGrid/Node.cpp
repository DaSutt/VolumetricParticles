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

#include "Node.h"

#include "AdaptiveGridConstants.h"
#include "..\..\..\utility\Math.h"

namespace Renderer
{
  constexpr int bitCount = sizeof(uint32_t) * 8;
	//x,y,z, leaf bit, activeBit
	constexpr int packingOffsets[4] = { 22, 12, 2, 1 };

	int NodeData::atlasSideLength_ = 2;

  NodeData::NodeData() :
    nodebitCount_{ static_cast<int>(GridConstants::nodeResolution *
			GridConstants::nodeResolution * GridConstants::nodeResolution 
			/ bitCount) },
    nodeCount_{0}
  {
  }

  void NodeData::Clear()
  {
		nodeCount_ = 0;
    
		nodeInfos_.clear();
		imageInfos_.clear();
    
		nodeGridIndex.clear();
		parentIndices_.clear();
		gridPos_.clear();
    activeBits_.clear();
    bitCounts_.clear();
		childCount_.clear();
  }

	void NodeData::UpdateImageOffsets(const int parentImageOffset)
	{
		for (size_t i = 0; i < imageInfos_.size(); ++i)
		{
			imageInfos_[i].imageIndex += parentImageOffset;
			const auto textureOffset = Math::Index1Dto3D(imageInfos_[i].imageIndex, atlasSideLength_);
			imageInfos_[i].image = textureOffset;
			nodeInfos_[i].textureOffset = PackTextureOffset(textureOffset);

			const int mipMapImageIndex = imageInfos_[i].mipMapImageIndex;
			if (mipMapImageIndex != -1)
			{
				SetMipMapBit(nodeInfos_[i].textureOffset);
				const auto textureOffset = Math::Index1Dto3D(mipMapImageIndex, atlasSideLength_);
				nodeInfos_[i].textureOffsetMipMap = PackTextureOffset(textureOffset);
				imageInfos_[i].mipMap = textureOffset;
			}
		}
	}

  int NodeData::AddNode(glm::ivec3 nodePos, int gridIndex, int parentNodeIndex, int textureIndex)
  {
		int index = nodeCount_;
		nodeGridIndex.push_back(gridIndex);
		parentIndices_.push_back(parentNodeIndex);
		//set the mip mapping texture to -1, will be overwritten later if active children found
    
		ImageInfo info;
		info.imageIndex = textureIndex;
		info.mipMapImageIndex = -1;
		imageInfos_.push_back(info);
		nodeInfos_.push_back({});

		gridPos_.push_back(nodePos);
		childCount_.push_back({ 0 });
    activeBits_.resize(activeBits_.size() + nodebitCount_);
    bitCounts_.resize(bitCounts_.size() + nodebitCount_);

    ++nodeCount_;
		return index;
  }

  void NodeData::SetBit(int nodeIndex, int bit)
  {
    const int index = bit / bitCount + nodeIndex * nodebitCount_;
    const auto offset = bit % bitCount;
    const bool alreadySet = (activeBits_[index] >> offset) & 1;
    if (!alreadySet)
    {
      activeBits_[index] |= 1 << offset;
      //increment bit counts
      for (size_t i = index + 1; i < nodebitCount_ + nodeIndex * nodebitCount_; ++i)
      {
        ++bitCounts_[i];
      }
			++childCount_[nodeIndex];
    }
  }

	void NodeData::UpdateMipMap(int imageIndex, int indexNode)
	{
		imageInfos_[indexNode].mipMapImageIndex = imageIndex;
		SetMipMapBit(nodeInfos_[indexNode].textureOffset);
	}

	void NodeData::SetChildOffset(int offset, int nodeIndex)
	{
		nodeInfos_[nodeIndex].childOffset = offset;
	}

	uint32_t NodeData::PackTextureOffset(const glm::ivec3& textureOffset)
	{
		const glm::uvec3 offset = static_cast<glm::uvec3>(textureOffset);
		assert(offset.x < 1024 && offset.y < 1024 && offset.z < 1024);
		
		uint32_t packedOffset =
			offset.x << packingOffsets[0] |
			offset.y << packingOffsets[1] |
			offset.z << packingOffsets[2];
		return packedOffset;
	}

	bool NodeData::MipMapSet(const uint32_t imageOffset)
	{
		return imageOffset & (1 << 0);
	}

	void NodeData::SetMipMapBit(uint32_t& textureOffset)
	{
		textureOffset |= 1;
	}
}