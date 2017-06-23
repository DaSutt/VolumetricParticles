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

#include "DebugTraversal.h"
#include <glm\glm.hpp>

#include "..\..\resources\ImageManager.h"
#include "..\..\resources\QueueManager.h"
#include "..\..\resources\BufferManager.h"

using namespace glm;

#undef max
#undef min

namespace Renderer
{
	namespace
	{
		const float NO_CHANGE = 1.0e10f;
		const float EPSILON = 0.0002f;
		const float PI_4 = 12.5663706144f;
		const bool MIPMAP_TEXTURE_DEBUGGING = false;
		const bool HARDWARE_SAMPLING = true;
		const bool ENABLE_SHADOW_RAYS = true;
		const int SHADOW_RAY_COUNT = 50;

		const uint IMAGE_BIT_OFFSETS[3] = { 22, 12, 2 };
		const uint IMAGE_BIT_MASKS[2] = { 0x3fffff, 0xfff };
		const uint IMAGE_LEAF_BIT = 1;
		const uint IMAGE_MIPMAP_BIT = 0;

		const int NODE_RESOLUTION = 16;
		const int IMAGE_RESOLUTION = NODE_RESOLUTION + 1;
		const int IMAGE_RESOLUTION_OFFSET = IMAGE_RESOLUTION + 2;

		const bool TEST_TRANSMITTANCE = false;
		const bool TEST_SCATTERING = false;

		const vec3 TEST_BOUNDING_BOX_MIN = vec3(0, -32, 0);
		const vec3 TEST_BOUNDING_BOX_MAX = vec3(160, 0, 32);
		const int TEST_UNIFORM_STEP_COUNT = 300;
		const float TEST_SCATTERING_COEFFICIENT = 0.03f;
		const float TEST_EXTINCTION_COEFFICIENT = TEST_SCATTERING_COEFFICIENT + 0.0f;

		const bool LEVEL_VISUALIZATION = false;
		const vec3 LEVEL_COLORS[MAX_LEVELS] =
		{
			vec3(0.0f, 0.0f, 1.0f),
			vec3(0.0f, 1.0f, 0.0f),
			vec3(1.0f, 0.0f ,0.0f)
		};
		const float LEVEL_TRANSMITTANCE = 0.5f;

		const bool FREQUENCY_VISUALIZATION = false;
		const float FREQUENCY_TRANSMITTANCE = 0.5f;

		const bool TEXTURE_OUTPUT = false;
	}

	vec4 DebugTraversal::ImageToViewport(ivec2 imagePos) {
		vec2 imageTexCoord = vec2(imagePos) / vec2(raymarchData_.screenSize);
		vec4 viewPortPos = vec4((imageTexCoord.x * 2.0 - 1.0), ((1.0 - imageTexCoord.y) * 2 - 1) * -1, 0.0, 1.0);
		return viewPortPos;
	}

	vec3 DebugTraversal::ViewportToWorld(vec4 viewPortPos) {
		vec4 worldPosition = raymarchData_.viewPortToWorld * viewPortPos;
		worldPosition /= worldPosition.w;
		return vec3(worldPosition);
	}

	Ray CreateRay(vec3 start, vec3 end) {
		Ray ray;
		ray.direction = end - start;
		ray.maxLength = length(ray.direction);
		ray.direction /= ray.maxLength;
		ray.origin = start;
		return ray;
	}

	float DebugTraversal::Jittering(vec2 imagePos)
	{
		vec3 randomness = raymarchData_.randomness;
		vec3 texCoord = vec3(vec2(imagePos) / vec2(raymarchData_.screenSize)
			+ vec2(randomness), randomness.z);
		return raymarchData_.jitteringScale;
		//return texture(noiseTextureArray_, texCoord).r * raymarchData_.jitteringScale;
	}

	vec3 CalcDirectionReciprocal(vec3 direction) {
		vec3 dirReciprocal;
		dirReciprocal.x = direction.x == 0 ? NO_CHANGE : 1.0f / direction.x;
		dirReciprocal.y = direction.y == 0 ? NO_CHANGE : 1.0f / direction.y;
		dirReciprocal.z = direction.z == 0 ? NO_CHANGE : 1.0f / direction.z;
		return dirReciprocal;
	}

	//return value x = entry, y - exit, z - 0.0 if no intersection, 1.0 if intersection
	vec3 RayBoundingBoxIntersection(vec3 origin, vec3 recDir, vec3 boxMin, vec3 boxMax) {
		vec3 tmin = (boxMin - origin) * recDir;
		vec3 tmax = (boxMax - origin) * recDir;
		vec3 tNear = min(tmin, tmax);
		vec3 tFar = max(tmin, tmax);

		float entry = max(max(tNear.x, 0.0f), max(tNear.y, tNear.z));
		float exit = min(min(tFar.x, tFar.y), tFar.z);
		return vec3(entry, exit, (exit > 0.0 && entry < exit) ? 1.0 : 0.0);
	}

	float CalcExponentialDepth(int currStep, int maxSteps, float expScale)
	{
		return exp(currStep / float(maxSteps) * expScale);
	}

	float RayPlaneIntersection(vec3 rayOrigin, vec3 rayDirection)
	{
		//Fixed values for testing intersection with top of grid
		vec3 planeNormal = vec3(0.0, -1.0, 0.0);
		vec3 planePosition = vec3(256, 0, 0);
		float D = -(dot(planeNormal, planePosition));

		return -(dot(planeNormal, rayOrigin) + D) / (dot(planeNormal, rayDirection));
	}

	float CalcDepth(vec2 near_FarNearDiv, int stepCount, int maxSteps)
	{
		return near_FarNearDiv.x * pow(near_FarNearDiv.y, (stepCount / float(maxSteps))) - 1.0f;
	}

	ivec3 DebugTraversal::CalcGridPos(vec3 gridSpacePos, vec3 gridOffset, int level)
	{
		return ivec3((gridSpacePos - gridOffset) / levelData_.data[level].childCellSize);
	}

	int CalcBitIndex(ivec3 gridPos, int resolution) {
		return (gridPos.z * resolution + gridPos.y) * resolution + gridPos.x;
	}

	ivec2 CalcBitIndexOffset(int bitIndex, int nodeIndex, int level)
	{
		ivec2 bitIndexOffset;
		bitIndexOffset.x = bitIndex / 32 + nodeIndex * 128;
		bitIndexOffset.y = bitIndex % 32;
		return bitIndexOffset;
	}

	bool DebugTraversal::BitActive(ivec2 bitIndexOffset)
	{
		return bool((activeBits_.data[bitIndexOffset.x] >> bitIndexOffset.y) & 1);
	}

	int BitCounting(int bitIndex, uint bits) {
		uint pos = bitIndex % 32;
		uint value = bits & ((1 << pos) - 1);	//mask all higher bits
		uint count = 0;

		value = value - ((value >> 1) & 0x55555555);
		value = (value & 0x33333333) + ((value >> 2) & 0x33333333);
		count = ((value + (value >> 4) & 0xF0F0F0F) * 0x1010101) >> 24;
		return int(count);
	}

	int DebugTraversal::GetChildNodeIndex(int bitIndexOffset, int bitIndex, int level, int parentChildOffset)
	{
		int bitCount = bitCounts_.data[bitIndexOffset] +
			BitCounting(bitIndex, activeBits_.data[bitIndexOffset]);
		int childIndex = bitCount + levelData_.data[level].childOffset + parentChildOffset;
		return childIndices_.indices[childIndex];
	}

	//Calculates texture coordinate int [0,1] range for current node
	vec3 DebugTraversal::CalcTexCoord(vec3 globalPos, int level) {
		float cellSize = levelData_.data[level].gridCellSize;
		vec3 fraction = (globalPos) / cellSize;
		return fraction - vec3(ivec3(fraction));
	}

	//Calculates texture coordinate in atlas space, input texCoord [0,1]
	vec3 DebugTraversal::CalcTextureOffset(vec3 texCoord, vec3 imageOffset) {
		return (texCoord * raymarchData_.textureFraction + raymarchData_.textureOffset)
			+ imageOffset* raymarchData_.atlasTexelToNodeTexCoord;
	}

	const ivec3 offsets[8] =
	{
		ivec3(0,0,0),
		ivec3(1,0,0),
		ivec3(0,1,0),
		ivec3(1,1,0),

		ivec3(0,0,1),
		ivec3(1,0,1),
		ivec3(0,1,1),
		ivec3(1,1,1)
	};

	vec4 Interpolate(vec4 min, vec4 max, float difference)
	{
		return min * (1.0f - difference) + max * difference;
	}

	vec3 DebugTraversal::CalcTexelPosition(vec3 gridPosition, vec3 imageOffset, int level)
	{
		float cellSize = levelData_.data[level].gridCellSize;
		//Remove offset of node inside grid
		vec3 offset = vec3(ivec3(gridPosition / cellSize)) * cellSize;
		float texelSize = cellSize / NODE_RESOLUTION;
		vec3 texelPosition = (gridPosition - offset) / texelSize;
		//add image offset
		return texelPosition + vec3(ivec3(imageOffset) * IMAGE_RESOLUTION_OFFSET + 1);
	}

	//Performs trilinear sampling without hardware, input: normalized atlas texture coordinate
	vec4 DebugTraversal::SampleGridInterpolate(vec3 texelPosition, int imageIndex)
	{
		const vec3 min = floor(texelPosition);
		const vec3 max = min + vec3(1);

		vec3 difference = (texelPosition - min) / (max - min);
		//difference.y = (1.0f - difference.y);

		//return texelFetch(textureAtlas_, ivec3(min), 0);

		vec4 texelValues[8];
		for (int i = 0; i < 8; ++i)
		{
			const ivec3 texCoord = offsets[i] + ivec3(min);
			texelValues[i] = texelFetch(imageIndex, texCoord, 0);
		}

		vec4 xInterpolations[4];
		for (int i = 0; i < 4; ++i)
		{
			const int texelIndex = i * 2;
			xInterpolations[i] = Interpolate(texelValues[texelIndex], texelValues[texelIndex + 1],
				difference.x);
		}

		vec4 yInterpolations[2];
		for (int i = 0; i < 2; ++i)
		{
			const int texelIndex = i * 2;
			yInterpolations[i] = Interpolate(xInterpolations[texelIndex], xInterpolations[texelIndex + 1],
				difference.y);
		}

		return Interpolate(yInterpolations[0], yInterpolations[1], difference.z);
	}

	vec4 DebugTraversal::SampleGrid(GridStatus gridStatus, vec3 currentGridPos, bool mipMap)
	{
		if (HARDWARE_SAMPLING)
		{
			vec3 texCoord = CalcTexCoord(currentGridPos, gridStatus.currentLevel);
			vec3 texCoordAtlas = CalcTextureOffset(texCoord, gridStatus.imageOffset);
			if (mipMap)
			{
				//TODO replace the texture size with offset here
				//const float offset = 0.5 / float(raymarchData_.atlasSideLength * 19);
				//texCoordAtlas.x -= offset;
				//texCoordAtlas.z -= offset;
				//texCoordAtlas.y -= offset;
			}
			if (MIPMAP_TEXTURE_DEBUGGING)
			{
				if (gridStatus.currentLevel == 1)
				{
					return vec4(texCoordAtlas, 0.0);
				}
			}
			return textureLod(textureAtlas_, texCoordAtlas, 0);
		}
		else
		{
			vec3 texelPosition = CalcTexelPosition(currentGridPos,
				gridStatus.imageOffset, gridStatus.currentLevel);
			return SampleGridInterpolate(texelPosition, IMAGE_ATLAS);
		}
	}

	float DebugTraversal::SampleTransmittance(vec3 gridPos, ivec3 imageOffset, int level, bool mipMap)
	{
		if (HARDWARE_SAMPLING)
		{
			vec3 texCoord = CalcTexCoord(gridPos, level);
			vec3 texCoordAtlas = CalcTextureOffset(texCoord, imageOffset);
			return textureLod(textureAtlas_, texCoordAtlas, 0).y;
		}
		else
		{
			vec3 texelPosition = CalcTexelPosition(gridPos,
				imageOffset, level);
			return SampleGridInterpolate(texelPosition, IMAGE_ATLAS).y;
		}
	}

	int DebugTraversal::CalcMaxLevel(int prevMaxLevel, float stepLength)
	{
		//float minStepLength = levelData_.data[prevMaxLevel].gridCellSize * raymarchData_.lodScale_Reciprocal;
		//prevMaxLevel = stepLength > minStepLength ? max(0, prevMaxLevel - 1) : prevMaxLevel;
		//return prevMaxLevel;
		return 1;
	}

	bool UseMipMap(int currentLevel, int maxLevel)
	{
		return currentLevel == maxLevel;
	}

	vec3 DebugTraversal::GetImageOffset(int nodeIndex, bool mipMapping)
	{
		uint imageOffset = nodeInfos_.data[nodeIndex].imageOffset;
		//Set image offset to mipmap if node has one
		imageOffset = (mipMapping && (imageOffset & (1 << IMAGE_MIPMAP_BIT)) == 1) ?
			nodeInfos_.data[nodeIndex].imageOffsetMipMap : imageOffset;

		//Unpack image offset
		return vec3(
			imageOffset >> IMAGE_BIT_OFFSETS[0],
			(imageOffset & IMAGE_BIT_MASKS[0]) >> IMAGE_BIT_OFFSETS[1],
			(imageOffset & IMAGE_BIT_MASKS[1]) >> IMAGE_BIT_OFFSETS[2]);
	}

	float DebugTraversal::SampleGridShadowRay(vec3 gridSpacePos, float worldStepLength)
	{
		float transmittance = 0.0f;

		/*
		int level = 0;
		int parentNodeIndex = 0;
		//Sample coarse level
		ivec4 imageOffset = CalcImageOffset(level, 1,parentNodeIndex);
		transmittance += SampleTransmittance(gridSpacePos, imageOffset.xyz, level, imageOffset.w == 1);

		vec3 gridOffset = vec3(0,0,0);
		ivec3 gridPos = CalcGridPos(gridSpacePos, gridOffset, level);
		//TODO: add correct resolution
		int bitIndex = CalcBitIndex(gridPos, NODE_RESOLUTION);
		ivec2 bitIndexOffset = CalcBitIndexOffset(bitIndex, parentNodeIndex, level);

		NodeInfo parentNodeInfo;
		parentNodeInfo.imageIndex = 0;
		parentNodeInfo.childOffset = 0;

		if (BitActive(bitIndexOffset))
		{
		int nodeIndex = GetChildNodeIndex(bitIndexOffset.x, bitIndex, level, parentNodeInfo.childOffset)
		+ levelData_.data[level + 1].nodeOffset;
		gridOffset = levelData_.data[level].childCellSize * vec3(gridPos);

		level++;
		parentNodeInfo = nodeInfos_.data[nodeIndex];

		imageOffset = CalcImageOffset(level, 1, nodeIndex);
		transmittance += SampleTransmittance(gridSpacePos, imageOffset.xyz, level, imageOffset.w == 1);
		}*/

		return transmittance;
	}

	GridStatus DebugTraversal::UpdateGridStatus(GridStatus gridStatus, vec3 gridSpacePos, float stepLength)
	{
		int parentNodeIndex = 0;
		NodeInfo parentNodeInfo;
		parentNodeInfo.imageOffset = 0;
		parentNodeInfo.childOffset = 0;
		vec3 gridOffset = vec3(0, 0, 0);
		gridStatus.currentLevel = 0;
		int currentMaxLevel = CalcMaxLevel(gridStatus.maxLevel, stepLength);

		//Save the amount of sampled phase functions for non zero scattering values
		int phaseGAccumCount = 0;
		float phaseG = 0.0f;

		bool useMipMap = UseMipMap(gridStatus.currentLevel, currentMaxLevel);
		vec3 imageOffset = GetImageOffset(parentNodeIndex, useMipMap);

		gridStatus.imageOffset = imageOffset;
		gridStatus.accummTexValue = SampleGrid(gridStatus, gridSpacePos, useMipMap);
		gridStatus.lowestLevelTexValue = gridStatus.accummTexValue;
		phaseGAccumCount = gridStatus.accummTexValue.x == 0 ? phaseGAccumCount : phaseGAccumCount + 1;
		phaseG = gridStatus.accummTexValue.x == 0 ? phaseG : phaseG + gridStatus.accummTexValue.z;

		bool levelIntersection = gridStatus.levelChangeCount < 0 && (gridStatus.maxLevel != currentMaxLevel);
		gridStatus.levelChangeCount = levelIntersection ? 10 : gridStatus.levelChangeCount;


		for (int i = 0; i < gridStatus.maxLevel; ++i)
		{
			ivec3 gridPos = CalcGridPos(gridSpacePos, gridOffset, i);
			//TODO: add correct resolution
			int bitIndex = CalcBitIndex(gridPos, NODE_RESOLUTION);
			ivec2 bitIndexOffset = CalcBitIndexOffset(bitIndex, parentNodeIndex, i);

			if (BitActive(bitIndexOffset))
			{
				int nodeIndex = GetChildNodeIndex(bitIndexOffset.x, bitIndex, i, parentNodeInfo.childOffset)
					+ levelData_.data[i + 1].nodeOffset;
				gridOffset = levelData_.data[i].childCellSize * vec3(gridPos);
				gridStatus.currentLevel++;
				parentNodeInfo = nodeInfos_.data[nodeIndex];

				useMipMap = UseMipMap(gridStatus.currentLevel, gridStatus.maxLevel);
				if (MIPMAP_TEXTURE_DEBUGGING)
				{
					imageOffset = GetImageOffset(parentNodeIndex, useMipMap);
				}
				else
				{
					imageOffset = GetImageOffset(nodeIndex, useMipMap);
				}
				gridStatus.imageOffset = imageOffset;
				vec4 texValue = SampleGrid(gridStatus, gridSpacePos, useMipMap);

				if (MIPMAP_TEXTURE_DEBUGGING)
				{
					if (gridStatus.currentLevel == 1)
					{
						useMipMap = true;
					}

					if (useMipMap)
					{
						if (gridStatus.currentLevel == 1)
						{
							gridStatus.accummTexValue = texValue;
							gridStatus.lowestLevelTexValue = texValue;
							gridStatus.maxLevel = 2;
						}
						else if (gridStatus.currentLevel == 2)
						{
							gridStatus.currentLevel = 2;
							//gridStatus.accummTexValue = vec4(1);
							return gridStatus;
						}
					}
					else
					{
						gridStatus.currentLevel = 1;
						gridStatus.accummTexValue = texValue;
						return gridStatus;
					}
				}
				else
				{
					//Blend between the two mip map levels 
					if (i == gridStatus.maxLevel - 1 && gridStatus.levelChangeCount >= 0)
					{
						gridStatus.currentLevel--;
						useMipMap = UseMipMap(gridStatus.currentLevel, currentMaxLevel);
						gridStatus.imageOffset = GetImageOffset(parentNodeIndex, useMipMap);
						vec4 mipMapValue = SampleGrid(gridStatus, gridSpacePos, useMipMap);

						//mip map weight becomes higher further away from the original intersection
						float mipMapWeight = 1.0f / (gridStatus.levelChangeCount + 1) + 0.2f;
						texValue = vec4(vec2(texValue) * (1.0f - mipMapWeight) + vec2(mipMapValue) * mipMapWeight, 0.0f, 0.0f);
						texValue.z = mipMapValue.x == 0 ? texValue.z : (texValue.z + mipMapValue.z) * 0.5f;

						gridStatus.currentLevel++;
					}
				}

				gridStatus.lowestLevelTexValue = texValue;
				parentNodeIndex = nodeIndex;

				if (!MIPMAP_TEXTURE_DEBUGGING)
				{
					gridStatus.accummTexValue += texValue;
				}
				phaseGAccumCount = texValue.x == 0 ? phaseGAccumCount : phaseGAccumCount + 1;
				phaseG = texValue.x == 0 ? phaseG : phaseG + texValue.z;
			}
			else
			{
				break;
			}
		}
		//average the phase function
		gridStatus.accummTexValue.z = phaseGAccumCount > 0 ? phaseG / phaseGAccumCount : phaseG;
		gridStatus.levelChangeCount--;
		gridStatus.maxLevel = gridStatus.levelChangeCount < 0 ? currentMaxLevel : gridStatus.maxLevel;

		if (gridStatus.accummTexValue.y != 0.0)
		{
			//gridStatus.accummTexValue.x = TEST_SCATTERING_COEFFICIENT;
			//gridStatus.accummTexValue.y = TEST_EXTINCTION_COEFFICIENT;
		}

		return gridStatus;
	}

	float DebugTraversal::Visibility(GridStatus gridStatus, vec3 gridPos, vec3 worldPos, int stepCount, float jitteringOffset)
	{
		float visibility = 1.0f;

		int cascadeIndex = 0;
		vec4 lightCoord = raymarchData_.shadowCascades[cascadeIndex] * vec4(worldPos, 1.0);
		//No in scattering inside directional light
		//if (min(lightCoord.x, lightCoord.y) > 0.0 && max(lightCoord.x, lightCoord.y) < 1.0 &&
		//	texture(shadowMap_, vec3(lightCoord.xy, cascadeIndex)).r + 0.001f < lightCoord.z)
		//{
		//	visibility = 0.0f;
		//}
		//perform shadow ray calculation
		//else
		{
			float rayDistance = RayPlaneIntersection(gridPos, raymarchData_.lightDirection);
			vec3 rayStart = gridPos + rayDistance * raymarchData_.lightDirection;
			vec3 rayDirection = -raymarchData_.lightDirection;

			int maxSteps = SHADOW_RAY_COUNT;
			float worldStepSize = rayDistance / float(maxSteps);
			float previousDepth = 0.0f;

			float jitteringDepth = worldStepSize * jitteringOffset;

			for (int currSteps = 0; currSteps < maxSteps; ++currSteps)
			{
				float depth = min(rayDistance, worldStepSize * currSteps + jitteringDepth);
				vec3 currentGridPos = rayStart + rayDirection * depth;
				float stepSize = max(0.01f, depth - previousDepth);
				float transmittance = SampleGridShadowRay(currentGridPos, stepSize);
				visibility *= exp(-transmittance * stepSize);
				previousDepth = depth;
			}
		}
		return visibility;
	}

	float PhaseFunction(float LDotV, float g) {
		float d = 1 + g * LDotV;
		return (1.0f - g * g) / (PI_4 * d * d);
	}

	vec4 DebugTraversal::UpdateScatteringTransmittance(GridStatus gridStatus, vec4 texValue, vec4 accumScatteringTransmittance,
		vec3 gridPos, vec3 worldPos, vec3 direction, float worldStepSize, int stepCount,
		bool maxDepthReached, float jitteringOffset)
	{
		float visibility = 1.0f;
		//Dont calculate visibility when inscattering is zero
		if (texValue.x != 0.0 || maxDepthReached)
		{
			if (ENABLE_SHADOW_RAYS)
			{
				visibility = Visibility(gridStatus, gridPos, worldPos, stepCount, jitteringOffset);
			}
		}
		float transmittance = accumScatteringTransmittance.a;
		float scattering = texValue.x;
		float extinction = max(0.0000000001f, texValue.y);
		float phaseG = texValue.z;

		float LDotV = dot(raymarchData_.lightDirection, direction);
		vec3 L = raymarchData_.irradiance * PhaseFunction(LDotV, phaseG);
		vec3 S = L * visibility * scattering;
		vec3 SInt = (S - S * exp(-extinction * worldStepSize)) / extinction;
		accumScatteringTransmittance += vec4(transmittance * SInt, 0.0);

		accumScatteringTransmittance.a *= exp(-extinction * worldStepSize);
		if (maxDepthReached)
		{
			const float expectedTransmittance = exp(-0.01f * 32.0f);
			if (visibility != 1.0f)
			{
				//accumScatteringTransmittance.a *= expectedTransmittance;
				//return accumScatteringTransmittance;
			}
			if (expectedTransmittance != visibility)
			{
				//accumScatteringTransmittance = vec4(1,0,0,0);
				//return accumScatteringTransmittance;
			}
			accumScatteringTransmittance.a *= visibility;
		}

		return accumScatteringTransmittance;
	}


	vec4 DebugTraversal::Raymarching(vec3 gridSpaceOrigin, vec3 worldSpaceOrigin, vec3 direction, float maxDepth, float jitteringOffset)
	{
		//vec4 accumScatteringTransmittance = vec4(0, 0, 0, 1);

		////Calculate intersection with global volume and skip raymarching if no intersection
		//const vec3 globalIntersection =
		//	RayBoundingBoxIntersection(gridSpaceOrigin, CalcDirectionReciprocal(direction),
		//		vec3(0), vec3(levelData_.data[0].gridCellSize));
		//if (globalIntersection.z == 0.0)
		//{
		//	return accumScatteringTransmittance;
		//}

		//float currentDepth = globalIntersection.x;
		//const vec3 gridOrigin = gridSpaceOrigin + direction * globalIntersection.x;

		//GridStatus gridStatus;
		//gridStatus.currentLevel = 0;
		//gridStatus.levelChangeCount = 0;
		//gridStatus.maxLevel = raymarchData_.maxLevel;

		//maxDepth = min(maxDepth, raymarchData_.farPlane);

		//int stepCount = 0;

		//const float exponentialScale = log(raymarchData_.farPlane - raymarchData_.nearPlane);
		//float prevDepth = 0.0f;

		//int startLevel = -1;
		//int changeCount = 0;
		//int maxFrequency = 0;

		//while (stepCount < raymarchData_.maxSteps && currentDepth < raymarchData_.farPlane)
		//{
		//	float nextDepth = CalcExponentialDepth(stepCount, raymarchData_.maxSteps, exponentialScale);
		//	nextDepth = min(nextDepth, maxDepth);
		//	float stepLength = nextDepth - prevDepth;

		//	prevDepth = nextDepth;
		//	stepCount++;

		//	//TODO add jittering here
		//	float jitteredDepth = nextDepth;//min(nextDepth + jitteringOffset * stepLength, maxDepth);

		//	vec4 texValue = raymarchData_.globalScattering;

		//	//Stop if we reached point outside of volume
		//	vec3 currentGridPos = gridOrigin + direction * jitteredDepth;
		//	if (jitteredDepth < globalIntersection.y)
		//	{
		//		gridStatus = UpdateGridStatus(gridStatus, currentGridPos, stepLength);
		//		if (MIPMAP_TEXTURE_DEBUGGING)
		//		{
		//			if (gridStatus.currentLevel == 2)
		//			{
		//				accumScatteringTransmittance = gridStatus.accummTexValue;
		//				return accumScatteringTransmittance;
		//			}
		//			if (gridStatus.currentLevel == 1 && currentGridPos.y <= 272)
		//			{
		//				accumScatteringTransmittance = gridStatus.accummTexValue;
		//			}
		//		}
		//		texValue = gridStatus.accummTexValue;
		//	}


		//	if (LEVEL_VISUALIZATION)
		//	{
		//		const int level = gridStatus.currentLevel;
		//		if (startLevel != level)
		//		{
		//			startLevel = level;
		//			accumScatteringTransmittance += vec4(LEVEL_COLORS[level], 0.0);
		//			accumScatteringTransmittance.a = LEVEL_TRANSMITTANCE;
		//			changeCount++;
		//		}
		//	}
		//	else if (FREQUENCY_VISUALIZATION)
		//	{
		//		const vec4 lowestTexValue = gridStatus.lowestLevelTexValue;
		//		if (lowestTexValue.y > 0.0 || lowestTexValue.x > 0.0)
		//		{
		//			maxFrequency = max(maxFrequency, gridStatus.currentLevel);
		//		}
		//		accumScatteringTransmittance.a *= exp(-texValue.y * stepLength);
		//	}
		//	else
		//	{
		//		vec3 currentWorldPos = worldSpaceOrigin + direction * jitteredDepth;
		//		accumScatteringTransmittance = UpdateScatteringTransmittance(
		//			gridStatus, texValue, accumScatteringTransmittance,
		//			currentGridPos, currentWorldPos, direction, stepLength, stepCount,
		//			jitteredDepth == maxDepth, jitteringOffset);
		//	}

		//	//float jitterOffset = jitteredDepth;

		//	//jitterOffset = jitterOffset - jitteredDepth;

		//	//accumScatteringTransmittance.a *= exp(-texValue.y * stepLength);

		//	//jitteredDepth = min(jitteredDepth, cubeIntersection.y);    

		//	//TODO: add as variable
		//	if (jitteredDepth >= maxDepth)
		//	{
		//		if (LEVEL_VISUALIZATION)
		//		{
		//			accumScatteringTransmittance /= float(changeCount);
		//			return accumScatteringTransmittance;
		//		}
		//		else if (FREQUENCY_VISUALIZATION)
		//		{
		//			accumScatteringTransmittance.x = (maxFrequency + 1) / float(MAX_LEVELS);
		//			return accumScatteringTransmittance;
		//		}
		//		else
		//		{
		//			return accumScatteringTransmittance;
		//		}
		//	}
		//}

		//if (LEVEL_VISUALIZATION)
		//{
		//	accumScatteringTransmittance /= float(changeCount);
		//	return accumScatteringTransmittance;
		//}
		//else if (FREQUENCY_VISUALIZATION)
		//{
		//	accumScatteringTransmittance.x = (maxFrequency + 1) / float(MAX_LEVELS);
		//	return accumScatteringTransmittance;
		//}
		//else
		//{
		//	return accumScatteringTransmittance;
		//}
return vec4();
	}
	
	float DebugTraversal::CalcExponentialDepth(int currStep)
	{
		return exp(currStep / float(raymarchData_.maxSteps) * raymarchData_.exponentialScale) - 1.0f;
	}

	vec4 DebugTraversal::UpdateScatteringTransmittance(vec4 accumScatteringTransmittance, vec4 texValue, vec3 rayDirection, float shadowTransmittance, float worldStepSize)
	{
		const float transmittance = accumScatteringTransmittance.a;
		const float scattering = texValue.x;
		const float extinction = max(0.0000000001f, texValue.y);
		const float phaseG = texValue.z;

		const float LDotV = dot(raymarchData_.lightDirection, rayDirection);
		const vec3 L = raymarchData_.irradiance * PhaseFunction(LDotV, phaseG);
		const vec3 S = L * shadowTransmittance * scattering;
		const vec3 SInt = (S - S * exp(-extinction * worldStepSize)) / extinction;
		accumScatteringTransmittance += vec4(transmittance * SInt, 0.0f);
		accumScatteringTransmittance.a *= exp(-extinction * worldStepSize);

		return accumScatteringTransmittance;
	}

	//Use edge value of coarsest level for caluclations outside of the level
	vec4 DebugTraversal::CalcGridOutside(vec4 accumScatteringTransmittance, vec3 gridOrigin, vec3 rayDirection, float depth, float maxDepth)
	{
		//Texture position in atlas
		const vec3 texCoord = UpdateTexCoord(gridOrigin + rayDirection * depth, 0, 0)
			* float(IMAGE_RESOLUTION_OFFSET * raymarchData_.atlasSideLength);
		const vec3 absDirection = abs(rayDirection);
		const float max = std::max(std::max(absDirection.x, absDirection.y), absDirection.z);
		ivec3 dirOffset = ivec3(step(absDirection, vec3(max)) * sign(rayDirection));

		const ivec3 edgeTexCoord = ivec3(texCoord) + dirOffset;
		const vec4 texValue = texelFetch(textureAtlas_, edgeTexCoord, 0);
		const float shadowTransmittance = 1.0f;
		const float worldStepSize = maxDepth - depth;

		return UpdateScatteringTransmittance(
			accumScatteringTransmittance, texValue, rayDirection,
			shadowTransmittance, worldStepSize);
	}

	float DebugTraversal::ShadowRayIntersection(vec3 rayOrigin)
	{
		return -(dot(raymarchData_.shadowRayPlaneNormal, rayOrigin)
			+ raymarchData_.shadowRayPlaneDistance);
	}

	float DebugTraversal::CalcNextIntersection(glm::vec3 gridPos, glm::vec3 dirReciprocal, int level)
	{
		const float childCellSize = levelData_.data[level].childCellSize;
		const vec3 nodePos = gridPos / childCellSize;
		const vec3 min = (nodePos - fract(nodePos)) * childCellSize;
		const vec3 intersection = RayBoundingBoxIntersection(gridPos, dirReciprocal,
			min, min + childCellSize);
		return intersection.y;
	}

	vec3 DebugTraversal::UpdateTexCoord(vec3 gridPos, int level, int parentNodeIndex)
	{
		const vec3 imageOffset = GetImageOffset(parentNodeIndex, false);
		const vec3 texCoord = CalcTexCoord(gridPos, level);
		const vec3 texCoordAtlas = CalcTextureOffset(texCoord, imageOffset);

		currentGridPos_ = gridPos;
		imageOffset_ = imageOffset;
		currentLevel_ = level;

		return texCoordAtlas;
	}

	vec3 DebugTraversal::TextureStep(vec3 texCoord, vec3 direction, float stepSize, int level)
	{
		return texCoord + direction * stepSize * levelData_.data[level].texelScale;
	};

	float DebugTraversal::CalcShadowGridOutside(vec3 gridPos, float gridIntersectionDepth)
	{
		vec4 scatteringTransmittance = vec4(0, 0, 0, 1);
		scatteringTransmittance = CalcGridOutside(scatteringTransmittance,
			gridPos, raymarchData_.lightDirection,
			gridIntersectionDepth, ShadowRayIntersection(gridPos));
		return scatteringTransmittance.y;
	}

	DebugTraversal::GridStatus_Update DebugTraversal::SampleGrid(GridStatus_Update status, 
		vec3 gridPos, glm::vec3 rayDirection, glm::vec3 dirReciprocal, 
		float stepSize, float currDepth)
	{
		int level = 0;
		int parentNodeIndex = 0;

		if (currDepth < status.nextIntersectionDepth)
		{
			status.texCoordAtlas = TextureStep(status.texCoordAtlas, rayDirection, stepSize, level);
		}
		else
		{
			status.nextIntersectionDepth = CalcNextIntersection(gridPos, dirReciprocal, level) + currDepth;
			status.texCoordAtlas = UpdateTexCoord(gridPos, level, parentNodeIndex);
		}

		if (HARDWARE_SAMPLING)
		{
			status.texValue = textureLod(textureAtlas_, status.texCoordAtlas, 0);
		}
		else
		{
			const vec3 imageOffset = GetImageOffset(parentNodeIndex, false);
			const vec3 texelPosition = CalcTexelPosition(gridPos,
				imageOffset, level);
			status.texValue = SampleGridInterpolate(texelPosition, IMAGE_ATLAS);
		}
		return status;
	}

	bool DebugTraversal::InsideShadow(vec3 gridPos)
	{
		const vec3 worldPos = gridPos + raymarchData_.gridMinPosition;

		const int cascadeIndex = 0;
		const vec4 lightCoord = raymarchData_.shadowCascades[cascadeIndex] * vec4(worldPos, 1.0);
		if (min(lightCoord.x, lightCoord.y) > 0.0 && max(lightCoord.x, lightCoord.y) < 1.0)
		{
			return (textureLod(shadowMap_, vec3(vec2(lightCoord), cascadeIndex), 0).x) + 0.001f < lightCoord.z;
		}
		return false;
	}

	float DebugTraversal::SampleShadow(vec3 gridPos, vec3 dirReciprocal)
	{
		//if (InsideShadow(gridPos))
		//{
		//	return 0.0f;
		//}
		//else
		{
			const vec3 gridIntersection =
				RayBoundingBoxIntersection(gridPos, dirReciprocal,
					vec3(0), vec3(levelData_.data[0].gridCellSize));
			const float endDepth = gridIntersection.y;

			//TODO needs to be checked
			float shadowTransmittance = 1.0f;// CalcShadowGridOutside(
				//gridPos, endDepth);

			float currDepth = 0.0f;
			int stepCount = 0;
			while (stepCount < SHADOW_RAY_COUNT && currDepth < endDepth)
			{
				const float nextDepth = min(currDepth + levelData_.data[0].shadowRayStepSize, endDepth);
				const float worldStepSize = nextDepth - currDepth;

				const vec3 currGridPos = gridPos + currDepth * raymarchData_.lightDirection;
				const vec3 texCoordAtlas = UpdateTexCoord(currGridPos, 0, 0);

				const float extinction = textureLod(textureAtlas_, texCoordAtlas, 0).y;
				shadowTransmittance *= exp(-extinction * worldStepSize);

				currDepth = nextDepth;
				++stepCount;
			}

			return shadowTransmittance;
		}
	}

	vec4 DebugTraversal::Raymarching_Update(vec3 gridSpaceOrigin, vec3 rayDirection, float geometryDepth, float jitterOffset)
	{
		vec4 accumScatteringTransmittance = vec4(0, 0, 0, 1);

		//Calculate intersection with global volume and skip raymarching if no intersection
		const vec3 dirReciprocal = CalcDirectionReciprocal(rayDirection);
		const vec3 globalIntersection =
			RayBoundingBoxIntersection(gridSpaceOrigin, dirReciprocal,
				vec3(0), vec3(levelData_.data[0].gridCellSize));
		if (globalIntersection.z == 0.0)
		{
			return accumScatteringTransmittance;
		}
		const vec3 gridOrigin = gridSpaceOrigin + rayDirection * globalIntersection.x;

		//used for tracking debug data
		int changeCount = 0;
		int maxFrequency = 0;

		GridStatus_Update gridStatus;

		int currStepCount = 1;
		float currDepth = 0.0f;
		float prevExpDepth = 0.0f;
		while (currStepCount <= raymarchData_.maxSteps && currDepth < globalIntersection.y)
		{
			const float expDepth = CalcExponentialDepth(currStepCount);
			const float stepSize = (expDepth - prevExpDepth) * jitterOffset;

			const float jitterDepth = min(prevExpDepth + stepSize, geometryDepth);
			const float worldStepSize = jitterDepth - prevExpDepth;

			const vec3 gridPos = gridOrigin + rayDirection * jitterDepth;
			gridStatus = SampleGrid(gridStatus, gridPos, rayDirection, dirReciprocal, worldStepSize, jitterDepth);
			float shadowTransmittance = 1.0f;
			if (gridStatus.texValue.x > 0.0f)
			{
				shadowTransmittance = SampleShadow(gridPos, dirReciprocal);
			}

			accumScatteringTransmittance = UpdateScatteringTransmittance(
				accumScatteringTransmittance, gridStatus.texValue, rayDirection,
				shadowTransmittance, worldStepSize);

			if (jitterDepth == geometryDepth)
			{
				return accumScatteringTransmittance;
			}

			currDepth = jitterDepth;
			prevExpDepth = expDepth;
			++currStepCount;
		}

		accumScatteringTransmittance = CalcGridOutside(
			accumScatteringTransmittance, gridOrigin, rayDirection, currDepth, raymarchData_.maxDepth);

		if (LEVEL_VISUALIZATION)
		{
			accumScatteringTransmittance /= float(changeCount);
			return accumScatteringTransmittance;
		}
		else if (FREQUENCY_VISUALIZATION)
		{
			accumScatteringTransmittance.x = (maxFrequency + 1) / float(MAX_LEVELS);
			return accumScatteringTransmittance;
		}
		else
		{
			return accumScatteringTransmittance;
		}
	}

	//Use intersection with test bounding box to calculate transmittance over this depth
	vec4 TransmittanceTest(vec3 rayOrigin, vec3 direction)
	{
		vec3 cubeIntersection =
			RayBoundingBoxIntersection(rayOrigin, CalcDirectionReciprocal(direction),
				TEST_BOUNDING_BOX_MIN, TEST_BOUNDING_BOX_MAX);

		float transmittance = 1.0;

		if (cubeIntersection.z != 0)
		{
			const float depth = cubeIntersection.y - cubeIntersection.x;
			transmittance = exp(-TEST_EXTINCTION_COEFFICIENT * depth);
		}
		return vec4(0, 0, 0, transmittance);
	}

	float DebugTraversal::ShadowTransmittanceTest(vec3 rayPosition)
	{
		return TransmittanceTest(rayPosition, raymarchData_.lightDirection).a;
	}

	//Use uniform sampling in the test bounding box
	vec4 DebugTraversal::ScatteringTransmittanceTest(vec3 rayOrigin, vec3 direction)
	{
		vec4 scatteringTransmittance = vec4(0, 0, 0, 1);
		const vec3 rayIntersection =
			RayBoundingBoxIntersection(rayOrigin, CalcDirectionReciprocal(direction),
				TEST_BOUNDING_BOX_MIN, TEST_BOUNDING_BOX_MAX);

		if (rayIntersection.z == 0)
		{
			return scatteringTransmittance;
		}

		const float stepSize = (rayIntersection.y - rayIntersection.x) / float(TEST_UNIFORM_STEP_COUNT);

		for (int i = 0; i < TEST_UNIFORM_STEP_COUNT; ++i)
		{
			const vec3 currPosition = rayOrigin + direction * stepSize * float(i);
			const float shadow = ShadowTransmittanceTest(currPosition);

			const float LDotV = dot(raymarchData_.lightDirection, direction);
			vec3 L = raymarchData_.irradiance * PhaseFunction(LDotV, 0.0f);
			const vec3 S = L * shadow * TEST_SCATTERING_COEFFICIENT;
			const float sampleExtinction = max(0.0000000001f, TEST_EXTINCTION_COEFFICIENT);
			const vec3 Sint = (S - S * exp(-sampleExtinction * stepSize)) / sampleExtinction;
			scatteringTransmittance += vec4(scatteringTransmittance.a * Sint, 0.0);

			scatteringTransmittance.a *= exp(-TEST_EXTINCTION_COEFFICIENT * stepSize);
		}

		return scatteringTransmittance;
	}

	float DebugTraversal::GeometryShadowTest(vec3 worldPosition)
	{
		return ShadowTransmittanceTest(worldPosition);
	}

	void DebugTraversal::main(glm::ivec2 gl_GlobalInvocationID)
	{
		const ivec2 imagePos = ivec2(gl_GlobalInvocationID);

		//skip if image pos is outside of screen
		if (imagePos.x > raymarchData_.screenSize.x || imagePos.y > raymarchData_.screenSize.y)
		{
			return;
		}

		//TODO check stencil if shadow ray is needed at this position
		const float depth = imageLoad(depthImage_, imagePos);
		const vec4 startViewPort = ImageToViewport(imagePos);
		const vec4 endViewPort = startViewPort + vec4(0.0f, 0.0f, depth, 0.0f);

		const vec3 worldNear = ViewportToWorld(startViewPort);
		const vec3 worldEnd = ViewportToWorld(endViewPort);

		const Ray worldRay = CreateRay(worldNear, worldEnd);
		//move ray into grid space [0, gridSize]
		const vec3 gridOrigin = worldRay.origin - raymarchData_.gridMinPosition;
		const float jitteringOffset = 1.0f;//Jittering(imagePos);

		//const vec4 raymarchingResult = Raymarching(gridOrigin, worldRay.origin, worldRay.direction, worldRay.maxLength, jitteringOffset);
		const vec4 raymarchingResult = Raymarching_Update(gridOrigin,
			worldRay.direction, worldRay.maxLength, jitteringOffset);
		vec4 result = raymarchingResult;

		if (TEST_TRANSMITTANCE)
		{
			result = TransmittanceTest(worldRay.origin, worldRay.direction);
		}
		else if (TEST_SCATTERING)
		{
			result = ScatteringTransmittanceTest(worldRay.origin, worldRay.direction);
		}
		else if (TEXTURE_OUTPUT)
		{
			vec3 imageIndex = vec3(vec2(imagePos) / 18.0f / 36.0f, 26.0f / 36.0f);
			result = textureLod(textureAtlas_, imageIndex, 0);
		}
		if (TEST_TRANSMITTANCE || TEST_SCATTERING)
		{
			const float geometryShadow = GeometryShadowTest(worldEnd);
			result.a *= geometryShadow;
		}

		printf("Debug traversal ended\n");
	}


	void DebugTraversal::Traversal(QueueManager* queueManager, BufferManager* bufferManager, ImageManager* imageManager, int x, int y)
	{
		position_ = glm::ivec2(x, y);
		printf("Debug traversal at %d %d\n", x, y);
		GetImageData(queueManager, bufferManager, imageManager);

		main(position_);

		ReleaseImageData(bufferManager);
	}

	void DebugTraversal::SetImageIndices(int imageAtlas, int depth, int shadowMap, int noiseTexture)
	{
		imageIndices_[IMAGE_ATLAS] = { imageAtlas, true };
		imageIndices_[IMAGE_DEPTH] = { depth, false };
		imageIndices_[IMAGE_SHADOW] = { shadowMap, true };
		imageIndices_[IMAGE_NOISE] = { noiseTexture, true };
	}

	void DebugTraversal::GetImageData(QueueManager* queueManager, BufferManager* bufferManager, ImageManager * imageManager)
	{
		BufferManager::BufferInfo bufferInfo;
		bufferInfo.bufferingCount = 1;
		bufferInfo.pool = BufferManager::MEMORY_TEMP;
		bufferInfo.size = 0;
		for (int i = 0; i < IMAGE_MAX; ++i)
		{
			imageIndices_[i].offset = bufferInfo.size;
			if (imageIndices_[i].refactoring)
			{
				imageIndices_[i].size = imageManager->Ref_GetImageSize(i);
			}
			else
			{
				imageIndices_[i].size = imageManager->GetImageSize(i);
			}
			bufferInfo.size = imageIndices_[i].size + imageIndices_[i].offset;
		}
		bufferInfo.typeBits = BufferManager::BUFFER_TEMP_BIT;
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		tempBufferIndex_ = bufferManager->RequestBuffer(bufferInfo);

		for (int i = 0; i < IMAGE_MAX; ++i)
		{
			const int imageIndex = imageIndices_[i].index;
			const bool refactoring = imageIndices_[i].refactoring;

			VkImageSubresourceLayers subresourceLayers = {};
			if (i == IMAGE_DEPTH || i == IMAGE_SHADOW)
			{
				subresourceLayers.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			}
			else
			{
				subresourceLayers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			}
			subresourceLayers.baseArrayLayer = 0;
			subresourceLayers.layerCount = 1;
			subresourceLayers.mipLevel = 0;

			const auto imageExtent = refactoring ? 
				imageManager->Ref_GetImageInfo(imageIndex).extent : 
				imageManager->GetImageInfo(imageIndex).extent;
			imageIndices_[i].width = imageExtent.width;

			VkBufferImageCopy copyRegion = {};
			copyRegion.bufferOffset = imageIndices_[i].offset;
			copyRegion.bufferRowLength = 0;
			copyRegion.bufferImageHeight = 0;
			copyRegion.imageSubresource = subresourceLayers;
			copyRegion.imageOffset = {};
			copyRegion.imageExtent = imageExtent;
			VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL;
			if (i == IMAGE_DEPTH)
			{
				layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}
			else if (i == IMAGE_NOISE)
			{
				layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			}

			imageManager->CopyImageToBuffer(queueManager,
				bufferManager->GetBuffer(tempBufferIndex_, BufferManager::BUFFER_TEMP_BIT),
				{ copyRegion }, layout, imageIndex, refactoring);
		}

		imageDataPtr_ = bufferManager->Map(tempBufferIndex_, BufferManager::BUFFER_TEMP_BIT);
	}

	void DebugTraversal::ReleaseImageData(BufferManager* bufferManager)
	{
		bufferManager->Unmap(tempBufferIndex_, BufferManager::BUFFER_TEMP_BIT);
		bufferManager->ReleaseTempBuffer(tempBufferIndex_);

		imageDataPtr_ = nullptr;
	}

	float DebugTraversal::imageLoad(int index, glm::ivec2 imagePos)
	{
		const int texelIndex = imagePos.y * imageIndices_[index].width + imagePos.x;
		char* imagePtr = static_cast<char*>(imageDataPtr_) + imageIndices_[index].offset;
		float result = 0.0f;
		if (index == IMAGE_DEPTH)
		{
			const glm::uint16 depthUnorm = reinterpret_cast<glm::uint16*>(imagePtr)[texelIndex];
			result = depthUnorm / 65536.0f;
		}
		return result;
	}

	vec4 DebugTraversal::texelFetch(int index, glm::ivec3 texelPosition, int lod)
	{
		int texelIndex = CalcBitIndex(texelPosition, imageIndices_[index].width);
		char* imagePtr = static_cast<char*>(imageDataPtr_) + imageIndices_[index].offset;
		vec4 result = vec4(0.0f);
		if (index == IMAGE_ATLAS)
		{
			texelIndex *= 2;
			const vec2 xy = unpackHalf2x16(reinterpret_cast<uint*>(imagePtr)[texelIndex]);
			const vec2 zw = unpackHalf2x16(reinterpret_cast<uint*>(imagePtr)[texelIndex + 1]);
			result = vec4(xy, zw);
		}
		else if (index == IMAGE_SHADOW)
		{
			const vec2 xy = unpackHalf2x16(reinterpret_cast<uint*>(imagePtr)[texelIndex / 2]);
			result.x = texelIndex % 2 == 0 ? xy.x : xy.y;
		}
		return result;
	}

	vec3 Calc2DTexPosition(vec3 texCoord, int width)
	{
		return texCoord * static_cast<float>(width);
	}

	vec4 DebugTraversal::textureLod(int index, glm::vec3 texCoord, int lod)
	{
		vec3 texelPosition;
		if (index == IMAGE_ATLAS)
		{
			 texelPosition = CalcTexelPosition(currentGridPos_,
				imageOffset_, currentLevel_);
		}
		else if (index == IMAGE_SHADOW)
		{
			texelPosition = Calc2DTexPosition(texCoord, imageIndices_[index].width);
			texelPosition.z = 0;
		}
		return SampleGridInterpolate(texelPosition, index);
	}
}