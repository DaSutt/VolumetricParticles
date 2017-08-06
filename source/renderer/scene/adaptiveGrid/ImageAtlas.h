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

#include <vulkan\vulkan.h>

#include "AdaptiveGridData.h"
#include "..\..\resources\ImageManager.h"

namespace Renderer
{
	class BufferManager;
	class ImageManager;
	
	//Manages a image atlas for 3D images
	//All elements have the same size
	class ImageAtlas
	{
	public:
		ImageAtlas(int imageResolution);
		//Resources:
		//	- R16G16B16A16_SFLOAT 3D Storage image 
		//	- Constant buffer containing the size and image resolution of the atlas
		void RequestResources(ImageManager* imageManager, BufferManager* bufferManager, 
			ImageManager::ImageMemoryPool memoryPool, int frameCount);
		//Calculate new side length of the atlas
		void UpdateSize(int maxImageOffset);
		void ResizeImage(ImageManager* imageManager);

		const int GetImageIndex() const { return imageResource.index; }
		const int GetBufferIndex() const { return cbIndex_; }
		const int GetSideLength() const { return sideLength_; }
		const int GetResolution() const { return cbData_.imageResolution; }
	private:
		struct CBData
		{
			int imageResolution;
			int atlasSideLength;
		};

		int sideLength_ = 1;

		ImageManager::ImageMemoryPool memoryPool_;
		GpuResource imageResource;
		CBData cbData_;
		int cbIndex_ = -1;
	};
}