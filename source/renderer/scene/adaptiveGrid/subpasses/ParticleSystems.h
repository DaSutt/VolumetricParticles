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
	//For each node store the particles that cover it and fill the grid accordingly
	class ParticleSystems
	{
	public:
		//Fill with random particles for testing
		ParticleSystems();
		//Set offset of the adaptive grid in world space
		void SetGridOffset(const glm::vec3& gridOffset);
		//Resources:
		//	- Constant buffer with texel size and particle volumetric data
		//	- Storage buffer for each particle
		//	- Storage buffer for each node containing particles
		//	- Storage buffer with all particle indices for the nodes
		void RequestResources(BufferManager* bufferManager, int frameCount, int atlasImageIndex);
		//Bindings
		//	- Out: image atlas
		//	- In: Constant buffer
		//	- In: Storage buffer - particles
		//	- In: Storage buffer - nodes
		//	- In: Storage buffer - particle indices
		int GetShaderBinding(ShaderBindingManager* bindingManager, int frameCount);

		//TODO Change the maximum of particles based on the particle systems in the scene
		void OnLoadScene(const Scene* scene);
		//TODO Update the particle systems
		void Update(float dt);
		
		//Loop over all particles and add all nodes into the grid that are covered by one
		//Store the mapping from nodes to particles
		void GridInsertParticleNodes(const glm::vec3& worldOffset, GridLevel* gridLevel);
		//Needs to be called after the grid was updated with the inserted nodes
		//Fills the storage buffers with data
		void UpdateGpuData(GridLevel* parentLevel, GridLevel* childLevel, int atlasResolution);
		//Update the texel size and volumetric data for particles
		void UpdateCBData(const GridLevel* gridLevel);
		
		bool ResizeGpuResources(std::vector<ResourceResize>& resourceResizes);
		void UpdateGpuResources(BufferManager* bufferManager, int frameIndex);

		void Dispatch(ImageManager* imageManager, VkCommandBuffer commandBuffer, int frameIndex);

		//Used to expose the scene data when exporting the scene to pbrt
		const auto& GetParticles() const { return particles_; }
		const auto& GetRadi() const { return radi_; }
		const auto& GetWorldOffset() const { return gridOffset_; }
	private:
		enum GpuBufferType
		{
			GPU_STORAGE_PARTICLE,
			GPU_STORAGE_NODE,
			GPU_STORAGE_NODE_PARTICLE,
			GPU_MAX
		};
		struct CBData
		{
			glm::vec4 textureValue;
			float texelSize;
			glm::vec3 padding;
		};
		struct Node
		{
			glm::vec3 worldOffset;
			int particleCount;
			int particleOffset;
			uint32_t imageOffset;
			glm::vec2 padding;
		};

		VkDeviceSize CalcStorageBufferSize(GpuBufferType bufferType);

		//World space offset of the adaptive grid
		glm::vec3 gridOffset_;

		std::vector<float> radi_;
		std::vector<Particle> particles_;
		std::vector<Node> nodeData_;
		std::vector<int> nodeParticleIndices_;

		//Map the child index of each node to the particle indices that intersect with it
		std::map<int, std::vector<int>> nodeParticleMapping;
		
		int debugParticleCount_ = 0;
		int maxParticles_ = 0;

		CBData cbData_;
		int cbIndex_ = -1;
		std::array<ResourceResize, GPU_MAX> storageBuffers_;
		int atlasImageIndex_ = -1;	
	};
}