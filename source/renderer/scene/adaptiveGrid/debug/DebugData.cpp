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

#include "DebugData.h"

namespace DebugData
{
	Renderer::RaymarchingData raymarchData_;
	NodeInfoContainer nodeInfos_;
	LevelDataContainer levelData_;
	AcitveBitsContainer activeBits_;
	BitCountsContainer bitCounts_;
	ChildIndexContainer childIndices_;

	int textureAtlas_;

	glm::vec4 raymarchingResults_;

	glm::vec4 texture(int imageIndex, const glm::vec3& texCoord)
	{
		printf("WARNING: debug texture sampling not implemented yet\n");
		return glm::vec4(0);
	}

	glm::vec4 texelFetch(int imageIndex, const glm::vec3& texelCoordinate, int lod)
	{
		printf("WARNING: debug texture fetch not implemented yet\n");
		return glm::vec4(0);
	}

	glm::vec3 clamp(const glm::vec3& value, float min, float max)
	{
		return glm::clamp(value, glm::vec3(min), glm::vec3(max));
	}
}