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
#include <vulkan\vulkan.h>

namespace Renderer
{
	class ShaderBindingManager;
	class ImageManager;
	class GroundFog;
	class BufferManager;

	//Fills the coarsest level of the volumetric grid with uniform values
	class GlobalVolume
	{
	public:
		//Request the constant buffer with the volumetric values
		void RequestResources(BufferManager* bufferManager, int frameCount, int atlasImageIndex);
		//Bindings:
		//	- In: Volumetric data constant buffer
		//	- Out: Image Atlas as storage image
		int GetShaderBinding(ShaderBindingManager* bindingManager, int frameCount);

		//Update the volumetric values for the whole scene
		void UpdateCB(GroundFog* groundFog);
		//TODO: remove clearing of image atlas
		//TODO: only dispatch if values have changed
		void Dispatch(ImageManager* imageManager, VkCommandBuffer commandBuffer, int frameIndex);
	private:
		struct CBData
		{
			glm::vec4 globalValue;		//Uniform data for the whole scene
			glm::vec3 groundFogValue; //Uniform data below the interface of the ground fog
			int groundFogTexelStart;	
		};
		CBData cbData_;

		bool updated_ = true;
		int imageAtlasIndex_ = -1;
		int cbIndex_ = -1;
	};
}