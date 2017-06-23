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

#include <vector>
#include <array>
#include <glm\glm.hpp>
#include <vulkan\vulkan.h>

#include "GridLevel.h"
#include "AdaptiveGridData.h"

namespace Renderer
{
	class ImageManager;
	class ShaderBindingManager;

	//Used for updating the edge texels in the image atlas depending on the neighbor cells of 
	//the same level
	//Mip map neighbors have to be calculated after all mip maps were created
	class NeighborCells
	{
	public:
		NeighborCells();
		void RequestResources(BufferManager* bufferManager, int frameCount, int atlasImageIndex);
		int GetShaderBinding(ShaderBindingManager* bindingManager, int frameCount);

		//Updates axisOffsets and min,max grid positions
		void UpdateAtlasProperties(int sideLength);
		//Takes the updated grid levels and extracts neighbor data from them
		void CalculateNeighbors(const std::vector<GridLevel>& gridLevels);

		//Adds the gpu resource to the resizes and returns true if resize is neccessary
		bool ResizeGpuResources(std::vector<ResourceResize>& resizes);
		void UpdateGpuResources(BufferManager* bufferManager, int frameIndex);

		//Edge texels are updated in the atlas, barrier is added to finish writing
		void Dispatch(VkCommandBuffer commandBuffer, ImageManager* imageManager, int level, 
			bool mipMapping, int frameIndex);
	private:
		struct NeighborData
		{
			uint32_t currImageIndex;
			uint32_t activeNeihgborBits;
			std::array<uint32_t, 6> neighborImageIndices;
		};

		struct AtlasProperties
		{
			int resolution;
			int resolutionOffset;
			int sideLength;
		};

		struct PushConstantData
		{
			int neighborOffset;
		};

		//TODO check if in correct order
		enum Direction
		{
			DIR_LEFT,
			DIR_RIGHT,
			DIR_UP,
			DIR_DOWN,
			DIR_BACK,
			DIR_FRONT,
			DIR_MAX
		};
		Direction GetOppositeDirection(Direction input);

		enum Offsets
		{
			OFFSET_SRC_LOAD,
			OFFSET_DST_STORE,
			OFFSET_DST_LOAD,
			OFFSET_SRC_STORE,
			OFFSET_MAX
		};

		struct NeighborOffsets
		{
			std::array<VkOffset3D, OFFSET_MAX> data;
		};
		int GetImageIndexFromOffset(Offsets offset);

		bool EdgeCase(int gridCoordinate);
		bool EdgeCase(int gridIndex, int direction);
		void AddImageBarrier(VkCommandBuffer commandBuffer, ImageManager* imageManager);
		void AddImageCopy(int sourceImageIndex, int dstImageIndex,
			Direction direction, bool mipMap);
		void AddSet(int sourceImageIndex, int dstImageIndex, bool mipMap);
		bool NeighborInserted(int sourceImageIndex, int dstImageIndex, bool mipMap);
		NeighborOffsets CalcOffsets(int srcIndex, int dstIndex, Direction direction);
		int CalcTextureOffset(Direction direction, bool store);
		VkExtent3D GetExtent(Direction direction);

		std::vector<VkImageCopy> neighborCopy_;
		
		std::vector<VkImageCopy> mipMapNeighborCopy_;
		std::set<std::pair<int, int>> mipMapNeighborSets_;
		
		//std::vector<NeighborData> neighborData_;
		//std::vector<NeighborData> mipMapNeighborData_;

		struct NeighborInfo
		{
			uint32_t first;
			uint32_t second;
			int direction;
			int padding;
		};
		bool NeighborPairAdded(uint32_t first, uint32_t second);
		void AddNeighborInfo(const NeighborInfo& info, bool mipMap);

		std::set<std::pair<uint32_t, uint32_t>> neighborSets_;
		std::vector<NeighborInfo> neighborInfos_;
		std::vector<NeighborInfo> mipMapNeighborInfos_;
		//Offsets for each level of mip map data
		std::map<int, int> mipMapDataOffsets_;
		//Offsets to be added to the 1D array coordinates to advance one step along the x,y,z axis

		AtlasProperties atlasProperties_;
		int cbIndex_ = -1;

		int neighborBufferIndex_ = -1;
		int perLevelPushConstantIndex_ = -1;
		VkDeviceSize totalSize_ = 0;

		int atlasImageIndex_ = -1;

		ShaderBindingManager* bindingManager_ = nullptr;

		const int nodeResolutionSquared_;
		const glm::ivec3 axisOffsets_;
		const int minGridPos_ = 0;
		const int maxGridPos_ = 0;
	};
}