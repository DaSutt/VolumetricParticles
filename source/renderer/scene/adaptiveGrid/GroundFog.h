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
		void SetSizes(float cellSize, float gridSize);
		void RequestResources(ImageManager* imageManager, BufferManager* bufferManager, int frameCount, int atlasImageIndex);
		int GetShaderBinding(ShaderBindingManager* bindingManager, int frameCount);

		void UpdateCBData(float heightPercentage, float scattering, float absorption, float phaseG, float noiseScale);
		//Inserts needed grid cells into grid level 1
		void UpdateGridCells(GridLevel* gridLevel);
		//Needs to be called after the image indices for the grid are computed
		void UpdatePerCellBuffer(BufferManager* bufferManager, GridLevel* gridLevel, int frameIndex, int atlasResolution);
		void Dispatch(QueueManager* queueManager, ImageManager* imageManager, 
			BufferManager* bufferManager, VkCommandBuffer commandBuffer, int frameIndex);
		
		int GetPerCellBufferIndex() const { return perCellBuffer; }
		int GetPerCellBufferSize() const { return cellBufferSize_; }
		int GetCoarseGridTexel() const { return coarseGridPos_; }
	private:
		struct CBData
		{
			float scattering;
			float absorption;
			float phaseG;
			float cellFraction;		//amount that the edge texel is covered
			int edgeTexelY;				//texel at which the height fog stops
			float texelWorldSize;
			float noiseScale;
		};

		struct PerCell
		{
			glm::vec3 worldOffset;
			uint32_t imageOffset;		//x,y,z values 10 bit set last bit if coarser detail should be filled
		};

		void ExportGroundFogTexture(QueueManager* queueManager, ImageManager* imageManager, BufferManager* bufferManager);
		void SaveTextureData(void* dataPtr, VkDeviceSize imageSize, const VkExtent3D& imageExtent);

		int cellBufferSize_;
		float cellWorldSize_;
		float childCellWorldSize_;
		float gridWorldSize_;

		float gridSpaceHeight_ = 0.0f;
		int gridYPos_ = 15;
		int coarseGridPos_ = 15;
		int dispatchCount_ = 0;

		CBData cbData_;
		std::vector<PerCell> cellData_;

		std::vector<int> nodeIndices_;

		int cbIndex_ = -1;
		int perCellBuffer = -1;
		int atlasImageIndex_ = -1;

		//Used for storing the density of the ground fog in world space
		int debugTextureIndex_ = -1;

		const std::string mediumName_;
	};
}