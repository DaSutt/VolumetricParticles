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
#include <array>
#include <vulkan\vulkan.h>
#include "..\..\..\scene\Components.h"
#include "..\..\ShadowMap.h"

namespace Renderer
{
	struct RaymarchingData
	{
		glm::mat4 viewPortToWorld;
		std::array<glm::mat4,4> shadowCascades;
		
		glm::vec3 shadowRayPlaneNormal;
		float shadowRayPlaneDistance;

		glm::vec3 irradiance;
		float lodScale_Reciprocal;
		
		glm::vec4 globalScattering;
		glm::vec3 gridMinPosition;
		float jitteringScale;
		
		glm::vec3 randomness;
		float textureOffset;
		
		glm::vec3 lightDirection;
		float atlasSideLength_Reciprocal;
		
		glm::vec2 screenSize;
		float nearPlane;
		float farPlane;
		
		float exponentialScale;
		float atlasTexelToNodeTexCoord;
		float maxDepth;
		int maxSteps;
		
		int atlasSideLength;
		int maxLevel;
		glm::ivec2 padding;
	};

	struct LevelData
	{
		float gridCellSize;
		int gridResolution;
		int nodeArrayOffset;
		int nodeSize;

		int childArrayOffset;
		float childCellSize;
		int nodeOffset;
		int childOffset;
		
		float shadowRayStepSize;
		float texelScale;
		glm::vec2 PADDING;
	};

	struct GpuResource
	{
		int index;
		int nodeOffset;
		VkDeviceSize maxSize;
	};

	struct ResourceResize
	{
		VkDeviceSize size;
		int index;
	};
}