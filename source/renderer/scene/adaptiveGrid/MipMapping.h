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
#include "Node.h"

namespace Renderer
{
	class GridLevel;
	class BufferManager;
	class ShaderBindingManager;
	class ImageManager;

	//Calculate mip map 3D volume textures containing the volumetric data from lower levels
	//The lower levels are averaged and stored on coarser resolution where each node corresponds to one texel
	//This texture is then combined with the original data on the respective level
	//The mipmaps have to be created starting on the lowest level to use these during calculation on higher levels
	class MipMapping
	{
	public:
		MipMapping();
		//Resources:
		//	- 3D storage image used as an image atlas for the averaged mipmaps
		//	- Constant buffer for image atlas properties
		//	- Storage buffer for each child node for averaging
		//	- Storage buffer for each parent node for merging
		void RequestResources(ImageManager* imageManager, BufferManager* bufferManager, int frameCount, int atlasImageIndex);
		//Bindings:
		//Averaging:
		//	- In: Image atlas with original data
		//	- In: Storage buffer with per child node data
		//	- In: Constant buffer with image atlas properties
		//	- Out: Mip map image atlas as storage image
		//	- In: Push constant with start offset for the current level into the child node data
		//Merging:
		//	- In\Out: Image atlas
		//	- In: mipmap image atlas
		//	- In: Constant buffer with image atlas properties
		//	- In: Storage buffer with parent node data
		//	- In: Push constant with start offset for the current level into parent node data
		int GetShaderBinding(ShaderBindingManager* bindingManager, int frameCount, int pass);

		//Add each parent node that has children to the mipmapping
		//Should be called after all grid levels were updated
		void UpdateMipMapNodes(const GridLevel* parentLevel, const GridLevel* childLevel);
		//Should be called after UpdateMipMapNodes was called for all parent levels
		//Fill storage buffers with data
		void UpdateGpuResources(BufferManager* bufferManager, int frameIndex);
		//Returns true if the storage buffers need to be resized and stores the new size and ids in resourceResizes
		bool ResizeGpuResources(ImageManager* imageManager, std::vector<ResourceResize>& resourceResizes);
		//Update the properties of the atlas containing the original data
		void UpdateAtlasProperties(int atlasSideLength, int atlasResolution);

		//TODO
		void Dispatch(ImageManager* imageManager, BufferManager* bufferManager, 
			VkCommandBuffer commandBuffer, int frameIndex, int level, int pass);



		int GetImageAtlasIndex() const { return mipMapImageAtlas_.GetImageIndex(); }
	private:
		enum Passes
		{
			MIPMAP_AVERAGING,		//Go through the child levels, average these and store them in an additional texture
			MIPMAP_MERGING,			//Merge with original data on this level by sampling both textures and adding the values
			MIPMAP_MAX
		};

		struct ChildNodeData
		{
			uint32_t imageOffset;
			uint32_t parentTexel;		//Texel coordinates packed into this 32bit value
		};
		struct ParentNodeData
		{
			uint32_t imageAtlasOffset;
			uint32_t imageAtlas_mipmapOffset;
			uint32_t mipmapOffset;
		};
		struct LevelData_New
		{
			uint32_t averagingStart;	//Start index into the perChildData array during averaging
		};
		struct CBData
		{
			float atlasSideLength_Reciprocal;
			float texelSize;
			float texelSize_Half;
			float texelSizeMipmap;
		};

		//Store the image offset for the child and pack the parent texel
		ChildNodeData GetChildNodeData(const Renderer::NodeData& data, int childNodeIndex, bool leafLevel);
		//After mipmaps for each level have been created update the texel positions based on the size of the mipmap atlas
		void UpdateImageOffsets(ImageManager* imageManager);
		int CalcStorageBufferSize(Passes pass);

		//Add buffer barrier for the averaged value buffer 
		//and for writing to the image atlas
		void AddBarriers(ImageManager* imageManager, VkCommandBuffer commandBuffer, int frameIndex);
		void AddMergingBarrier(ImageManager* imageManager, VkCommandBuffer commandBuffer, int frameIndex, bool lastLevel);

		//Data passed to the shader to average each child and store it
		std::vector<ChildNodeData> perChildData_;
		//Data for each child with the total mipmapIndex
		std::vector<int> mipmapIndices_;
		std::vector<LevelData_New> perLevelDataChild_;
		std::vector<int> perLevelChildCount_;
		int mipmapCount_ = 0;
		//Data for each parent node for merging of the image atlas and the mipmap atlas
		std::vector<ParentNodeData> perParentData_;
		std::vector<LevelData_New> perLevelDataParent_;
		std::vector<int> perLevelParentCount_;

		//Store the averaged values at the centers of nodes at each texel
		ImageAtlas mipMapImageAtlas_;
		CBData cbData_;

		int cbIndex_ = -1;
		int perLevelPushConstantIndex_ = -1;
		int atlasImageIndex_ = -1;
		int perLevelMergingPushConstantIndex_ = -1;
		std::array<ResourceResize, MIPMAP_MAX> storageBuffers_;

		//TODO: Should be removed, currently only used to get pipeline layout for push constants
		ShaderBindingManager* bindingManager_ = nullptr;
	};
}