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

#include "AdaptiveGridData.h"
#include "ImageAtlas.h"

namespace Renderer
{
	class GridLevel;
	class BufferManager;
	class ShaderBindingManager;
	class ImageManager;

	//For each level that has active children we need to store a mip map level
	class MipMapping
	{
	public:
		struct DispatchInfo
		{
			int x;
			int y; 
			int z;
			int resolution;
		};

		void RequestResources(ImageManager* imageManager, BufferManager* bufferManager, int frameCount, int atlasImageIndex);
		int GetShaderBinding(ShaderBindingManager* bindingManager, int frameCount, int pass);
		
		void UpdateAtlasProperties(int sideLength);

		//Add each parent node that has children to the mipmapping
		//Should be called after all grid levels were updated
		void UpdateMipMapNodes(const GridLevel* parentLevel, const GridLevel* childLevel);
		//Should be called after UpdateMipMapNodes was called for all parent levels
		void UpdateGpuResources(BufferManager* bufferManager, int frameIndex);
		bool ResizeGpuResources(ImageManager* imageManager, std::vector<ResourceResize>& resourceResizes);

		void Dispatch(ImageManager* imageManager, BufferManager* bufferManager, VkCommandBuffer commandBuffer, int frameIndex, int level, int pass);
	private:
		enum Passes
		{
			MIPMAP_AVERAGING,
			MIPMAP_MERGING,
			MIPMAP_MAX
		};

		struct CBData
		{
			float imageResolutionOffsetRec;
			float atlasTexelToTexCoord;
			float texCoordOffset;
		};

		enum GpuResources
		{
			GPU_STORAGE_MIPMAP_INFO,
			GPU_STORAGE_IMAGE_OFFSETS,
			GPU_MAX
		};

		int CalcSize(int bufferType);
		//Add buffer barrier for the averaged value buffer 
		//and for writing to the image atlas
		void AddBarriers(ImageManager* imageManager, VkCommandBuffer commandBuffer, int frameIndex);
		void AddMergingBarrier(ImageManager* imageManager, VkCommandBuffer commandBuffer, int frameIndex, bool lastLevel);
		void UpdateImageOffsets(ImageManager* imageManager);

		struct MipMapInfo
		{
			uint32_t childImageOffset;
			uint32_t parentTexel;
		};
		//Returns the image offset for the child that has to be averaged
		//On non leaf nodes the mipmaps are averaged instead of the original values
		//Also returns the texel in the parent node to which the child values corresponds
		MipMapInfo GetMipMapInfo(int childNodeIndex, const GridLevel* childLevel);

		int cbIndex_ = -1;
		int perLevelPushConstantIndex_ = -1;
		int perFillingPushConstantIndex_ = -1;
		int atlasImageIndex_ = -1;
		std::array<ResourceResize, GPU_MAX> buffers_;
		
		int atlasSideLength_ = 1;
		CBData cbData_;

		std::vector<MipMapInfo> mipMapData_;
		std::vector<int> mipMapOffsets_;
		
		struct LevelData
		{
			uint32_t childImageCount;
			int averagingOffset;
			uint32_t mipMapCount;
			int mergingOffset;
		};

		//Store the offsets for the different levels. 
		//Coarser levels need to be calculated later to use the mip map data of their children
		std::vector<LevelData> levelData_;
		int mipMapCount_ = 0;

		struct NodeImageOffsets
		{
			uint32_t imageOffset;
			uint32_t mipMapImageOffset;
			uint32_t mipMapSourceOffset;
		};
		std::vector<NodeImageOffsets> imageOffsets_;
		int perLevelMergingPushConstant_ = -1;

		ImageAtlas mipMapImageAtlas_;

		//TODO: Should be removed, currently only used to get pipeline layout for push constants
		ShaderBindingManager* bindingManager_ = nullptr;
	};
}