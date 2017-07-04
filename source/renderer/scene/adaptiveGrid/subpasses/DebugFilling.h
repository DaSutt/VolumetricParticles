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
#include <vulkan\vulkan.h>

namespace Renderer
{
	class GridLevel;
	class BufferManager;
	class ShaderBindingManager;
	class ImageManager;

	//Used to fill the texture atlas with debug data
		//All images are filled inside with color value based on 
			//the image offset inside the atlas [0, atlasSideLength]
		//Individual debug nodes with a specific value, fills the whole node image
	class DebugFilling
	{
	public:		
		void SetGpuResources(int atlasImageIndex, int atlasBufferIndex);
		
		int GetShaderBinding(ShaderBindingManager* bindingManager, int frameCount);
		void SetWorldOffset(const glm::vec3& worldOffset);
		void AddDebugNode(const glm::vec3 worldPosition, 
			float scattering, float absorption, float phaseG, int gridLevel);

		void SetImageCount(GridLevel* mostDetailedLevel);

		void InsertDebugNodes(std::vector<GridLevel>& gridLevels);
		void UpdateDebugNodes(std::vector<GridLevel>& gridLevels);
		void Dispatch(ImageManager* imageManager, VkCommandBuffer commandBuffer);
	private:
		struct PushConstantData
		{
			glm::ivec3 imageOffset;
			int activeNode;					//set to 1 if a debug node should be filled
			float scattering;
			float extinction;
			float phaseG;
		};

		struct DebugNode
		{
			PushConstantData pushData;
			
			glm::vec3 gridPosition;
			int gridLevel;
			int indexNode;
		};

		int dispatchCount_ = 0;

		int atlasImageIndex_ = -1;
		int atlasBufferIndex_ = -1;
		int pushConstantIndex_ = -1;
		
		std::vector<DebugNode> debugNodes_;
		glm::vec3 worldOffset_ = glm::vec3(0.0f);
		
		//TODO: remove this
		ShaderBindingManager* bindingManager_ = nullptr; 
	};

}