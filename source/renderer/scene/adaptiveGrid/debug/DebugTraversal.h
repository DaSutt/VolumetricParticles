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
#include "..\AdaptiveGridData.h"

namespace Renderer
{
	class ImageManager;
	class QueueManager;
	class BufferManager;

	const int MAX_LEVELS = 3;
	
	struct Ray {
		glm::vec3 origin;
		float maxLength;
		glm::vec3 direction;
	};

	struct GridStatus
	{
		glm::ivec3 gridPos;
		int currentLevel;
		glm::ivec3 imageOffset;
		int maxLevel;
		glm::vec4 accummTexValue;
		float maxStepLength;
		int levelChangeCount;
		glm::vec4 lowestLevelTexValue;
	};

	class DebugTraversal
	{
	public:
		void Traversal(QueueManager* queueManager, BufferManager* bufferManager, ImageManager* imageManager, int x, int y);
		void SetImageIndices(int imageAtlas, int depth, int shadowMap, int noiseTexture);

	private:
		enum Images
		{
			IMAGE_ATLAS,
			IMAGE_DEPTH,
			IMAGE_SHADOW,
			IMAGE_NOISE,
			IMAGE_MAX
		};

		struct ImageInfo
		{
			int index;
			bool refactoring;
			VkDeviceSize size;
			VkDeviceSize offset;
			void* dataPtr;
			int width;
		};

		std::array<ImageInfo, IMAGE_MAX> imageIndices_;
		int tempBufferIndex_ = -1;

		int depthImage_ = IMAGE_DEPTH;
		int textureAtlas_ = IMAGE_ATLAS;
		int shadowMap_ = IMAGE_SHADOW;
		void* imageDataPtr_ = nullptr;

		glm::vec3 currentGridPos_;
		glm::ivec3 imageOffset_;
		int currentLevel_;
	};
}