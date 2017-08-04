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
#include <glm\glm.hpp>
#include <vulkan\vulkan.h>

#include "AdaptiveGridData.h"

namespace Renderer
{
	class GridLevel;
	class BufferManager;
	class ImageManager;
	class ShaderBindingManager;
	class QueueManager;

	//Ground fog effect at a ceratin height with noise
	//The top layer of the fog is the only one which is detailed
	//below the top layer the coarser level is used
	class GroundFog
	{
	public:
		GroundFog();
		//Calculate the dimensions of the ground data based on the current grid size
		void SetSizes(float nodeSize, float gridSize);
		
		//Resources:
		//	- Constant buffer with volumetric values and fog variables
		//	- Constant buffer with world offsets + image offsets per node
		//	- Optional: 3D texture covering the whole ground fog in world space
		void RequestResources(ImageManager* imageManager, BufferManager* bufferManager, int frameCount, int atlasImageIndex);
		//Bindings:
		//	- In: Volumetric, ground fog data CB
		//	- In: Per node world, image offsets
		//	- Out: Image atlas as storage image
		//	- Out: Optional debug texture storage image
		int GetShaderBinding(ShaderBindingManager* bindingManager, int frameCount);
		
		//Calculate which nodes are covered by the ground fog
		void UpdateCBData(float heightPercentage, float scattering, float absorption, float phaseG, float noiseScale);
		//Inserts needed grid cells into the medium scale grid level
		void UpdateGridCells(GridLevel* gridLevel);
		//Needs to be called after the image indices for the grid are computed
		//Stores the world offsets and image offset for each node
		void UpdatePerNodeBuffer(BufferManager* bufferManager, GridLevel* gridLevel, int frameIndex, int atlasResolution);
		bool ResizeGPUResources(std::vector<ResourceResize>& resourceResizes);

		//Fills the ground fog with volumetric data based on 3D noise density
		//If a debug texture is to be created it is filled and exported
		void Dispatch(QueueManager* queueManager, ImageManager* imageManager, 
			BufferManager* bufferManager, VkCommandBuffer commandBuffer, int frameIndex);
	private:
		struct CBData
		{
			float scattering;
			float absorption;
			float phaseG;

			float texelWorldSize;
			float noiseScale;
			float cellFraction;		//amount that the edge texel is covered
			int edgeTexelY;				//texel at which the height fog stops
		};
		
		struct PerNodeData
		{
			glm::vec3 worldOffset;
			uint32_t imageOffset;		//x,y,z values 10 bit 
		};
		
		//Copy the ground fog density texture from GPU to CPU
		void ExportGroundFogTexture(QueueManager* queueManager, ImageManager* imageManager, BufferManager* bufferManager);
		//Save in PBRT file format
		void SaveTextureData(void* dataPtr, VkDeviceSize imageSize, const VkExtent3D& imageExtent);
		
		//TODO check value
		float childCellWorldSize_;
		
		//TODO check value
		float gridWorldSize_ = 0.0f;
		float nodeWorldSize_ = 0.0f;
		bool active_ = false;
		//Node position inside the grid
		int gridYPos_ = 15;
		int coarseTexelPosition_ = 15;
		int dispatchCount_ = 0;
				
		CBData cbData_;
		std::vector<PerNodeData> nodeData_;
		size_t nodeDataSize_ = 0;
		//Indices of the nodes inside the grid level
		std::vector<int> nodeIndices_;
		
		
		int cbIndex_ = -1;
		int perNodeBuffer_ = -1;
		int atlasImageIndex_ = -1;
		//Used for storing the density of the ground fog in world space
		int debugTextureIndex_ = -1;
	};
}