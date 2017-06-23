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
#include <array>
#include <glm\glm.hpp>
#include <map>
#include <vulkan\vulkan.h>

#include "ParticleSystem.h"
#include "..\AdaptiveGridData.h"

class Scene;

namespace Renderer
{
	class GridLevel;
	class BufferManager;
	class ShaderBindingManager;

	//Container for all particle systems
	//Adds them to the grid and performs the splatting into the grid
	class ParticleSystems
	{
	public:
		ParticleSystems();
		void SetGridOffset(const glm::vec3& gridOffset);
		void RequestResources(BufferManager* bufferManager, int frameCount, int atlasImageIndex);
		int GetShaderBinding(ShaderBindingManager* bindingManager, int frameCount);

		void OnLoadScene(const Scene* scene);

		void Update(float dt);

		void GridInsertParticles(const glm::vec3& worldOffset, GridLevel* gridLevel);
		//Needs to be called after the grid was updated with the inserted nodes
		void UpdateGpuData(GridLevel* parentLevel, GridLevel* childLevel, int atlasResolution);
		void UpdateCBData(const GridLevel* gridLevel);
		bool ResizeGpuResources(std::vector<ResourceResize>& resourceResizes);
		void UpdateGpuResources(BufferManager* bufferManager, int frameIndex);

		int GetDispatchCount() const { return static_cast<int>(nodeData_.size()); }

		const auto& GetParticles() const { return particles_; }
		const auto& GetRadi() const { return radi_; }
		const auto& GetWorldOffset() const { return gridOffset_; }
	private:
		struct CBData
		{
			float texelSize;
			glm::vec3 padding;
		};

		struct Node
		{
			glm::vec3 gridOffset;
			int maxParticles;
			int particleOffset;
			uint32_t imageOffset;
			glm::ivec2 padding;
		};

		enum GpuBuffers
		{
			GPU_STORAGE_NODE,
			GPU_STORAGE_NODE_PARTICLE,
			GPU_STORAGE_PARTICLE,
			GPU_MAX
		};

		struct GpuResource
		{
			int index;
			VkDeviceSize totalSize;
		};

		int CalcSize(int bufferType);

		glm::vec3 min;
		glm::vec3 max;
		glm::vec3 gridOffset_;
		//key node index in childLevel, vector with particle indices
		std::map<int, std::vector<int>> nodeParticleMapping;

		std::vector<Node> nodeData_;
		std::vector<int> nodeParticleIndices_;

		std::vector<ParticleSystem> particleSystems_;
		std::vector<Particle> particles_;
		std::vector<float> radi_;
		int maxParticles_ = 1;

		CBData cbData_;
		int cbIndex_ = -1;
		std::array<GpuResource, GPU_MAX> storageBuffers_;
		int atlasImageIndex_ = -1;

		int debugParticleCount_ = 0;
	};
}