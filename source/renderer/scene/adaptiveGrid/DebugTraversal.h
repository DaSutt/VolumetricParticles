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
#include "AdaptiveGridData.h"

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
		RaymarchingData raymarchData_;
		
		//Node data
		struct NodeInfo
		{
			uint32_t imageOffset;
			uint32_t imageOffsetMipMap;
			int childOffset;
		};

		void Traversal(QueueManager* queueManager, BufferManager* bufferManager, ImageManager* imageManager, int x, int y);
		void SetImageIndices(int imageAtlas, int depth, int shadowMap, int noiseTexture);

		struct LevelDataContainer
		{
			std::vector<LevelData> data;
		};
		LevelDataContainer levelData_;
		struct NodeInfoContainer
		{
			std::vector<NodeInfo> data;
		};
		NodeInfoContainer nodeInfos_;
		struct AcitveBitsContainer
		{
			std::vector<uint32_t> data;
		};
		AcitveBitsContainer activeBits_;
		struct BitCountsContainer
		{
			std::vector<int> data;
		};
		BitCountsContainer bitCounts_;
		struct ChildIndexContainer
		{
			std::vector<int> indices;
		};
		ChildIndexContainer childIndices_;

	private:
		
		void GetImageData(QueueManager* queueManager, BufferManager* bufferManager, ImageManager * imageManager);
		void ReleaseImageData(BufferManager* bufferManager);
		glm::ivec2 position_;

		glm::vec4 ImageToViewport(glm::ivec2 imagePos);
		glm::vec3 ViewportToWorld(glm::vec4 viewPortPos);
		
		float Jittering(glm::vec2 imagePos);

		glm::ivec3 CalcGridPos(glm::vec3 gridSpacePos, glm::vec3 gridOffset, int level);
		bool BitActive(glm::ivec2 bitIndexOffset);
		int GetChildNodeIndex(int bitIndexOffset, int bitIndex, int level, int parentChildOffset);

		glm::vec3 CalcTexCoord(glm::vec3 globalPos, int level);
		glm::vec3 CalcTextureOffset(glm::vec3 texCoord, glm::vec3 imageOffset);
		glm::vec3 CalcTexelPosition(glm::vec3 gridPosition, glm::vec3 imageOffset, int level);
		glm::vec4 SampleGridInterpolate(glm::vec3 texelPosition, int imageIndex);
		glm::vec4 SampleGrid(GridStatus gridStatus, glm::vec3 currentGridPos, bool mipMap);

		int CalcMaxLevel(int prevMaxLevel, float stepLength);
		glm::vec3 GetImageOffset(int nodeIndex, bool mipMapping);
		
		float SampleGridShadowRay(glm::vec3 gridSpacePos, float worldStepLength);
		GridStatus UpdateGridStatus(GridStatus gridStatus, glm::vec3 gridSpacePos, float stepLength);

		float SampleTransmittance(glm::vec3 gridPos, glm::ivec3 imageOffset, int level, bool useMipMap);

		float Visibility(GridStatus gridStatus, glm::vec3 gridPos, glm::vec3 worldPos, int stepCount, float jitteringOffset);
		glm::vec4 UpdateScatteringTransmittance(GridStatus gridStatus, glm::vec4 texValue, glm::vec4 accumScatteringTransmittance,
			glm::vec3 gridPos, glm::vec3 worldPos, glm::vec3 direction, float worldStepSize, int stepCount,
			bool maxDepthReached, float jitteringOffset);

		struct GridStatus_Update
		{
			glm::vec4 texValue;
			glm::vec3 texCoordAtlas;
			float nextIntersectionDepth;
		};



		glm::vec4 Raymarching(glm::vec3 gridSpaceOrigin, glm::vec3 worldSpaceOrigin, 
			glm::vec3 direction, float maxDepth, float jitteringOffset);

		//UPDATE
		float CalcExponentialDepth(int currStep);
		glm::vec4 UpdateScatteringTransmittance(glm::vec4 accumScatteringTransmittance,
			glm::vec4 texValue, glm::vec3 rayDirection,
			float shadowTransmittance, float worldStepSize);
		glm::vec4 CalcGridOutside(glm::vec4 accumScatteringTransmittance, glm::vec3 gridOrigin,
			glm::vec3 rayDirection, float depth, float maxDepth);
		
		float CalcNextIntersection(glm::vec3 gridPos, glm::vec3 dirReciprocal, int level);
		glm::vec3 UpdateTexCoord(glm::vec3 gridPos, int level, int parentNodeIndex);
		glm::vec3 TextureStep(glm::vec3 texCoord, glm::vec3 direction, float stepSize, int level);
		GridStatus_Update SampleGrid(GridStatus_Update status, glm::vec3 gridPos, glm::vec3 rayDirection,
			glm::vec3 dirReciprocal, float stepSize, float currDepth);
		float ShadowRayIntersection(glm::vec3 rayOrigin);
		float CalcShadowGridOutside(glm::vec3 gridPos, float gridIntersectionDepth);
		bool InsideShadow(glm::vec3 gridPos);
		float SampleShadow(glm::vec3 gridPos, glm::vec3 dirReciprocal);
		glm::vec4 Raymarching_Update(glm::vec3 gridSpaceOrigin, glm::vec3 rayDirection, 
			float geometryDepth, float jitterOffset);


		float ShadowTransmittanceTest(glm::vec3 rayPosition);
		glm::vec4 ScatteringTransmittanceTest(glm::vec3 rayOrigin, glm::vec3 direction);
		float GeometryShadowTest(glm::vec3 worldPosition);

		void main(glm::ivec2 gl_GlobalInvocationID);
		
		float imageLoad(int index, glm::ivec2 imagePos);
		glm::vec4 texelFetch(int index, glm::ivec3 texelPosition, int lod);
		glm::vec4 textureLod(int index, glm::vec3 texCoord, int lod);


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