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

#include "ParticleSystems.h"

#include <random>
#include <iterator>

#include "..\GridLevel.h"
#include "..\AdaptiveGridConstants.h"

#include "..\..\..\passes\GuiPass.h"

#include "..\..\..\resources\BufferManager.h"
#include "..\..\..\resources\ImageManager.h"
#include "..\..\..\passResources\ShaderBindingManager.h"

#include "..\..\..\..\scene\Scene.h"
#include "..\Node.h"
#include "..\..\..\wrapper\QueryPool.h"
#include "..\..\..\wrapper\Barrier.h"

namespace Renderer
{
	std::vector<glm::vec3> clusterPositions = 
	{
		glm::vec3(0,-5,20),
		glm::vec3(50,-15,40),
		glm::vec3(10,-10,60),
		glm::vec3(30,-10,60),
	};

	ParticleSystems::ParticleSystems()
	{
		std::default_random_engine generator;
		std::uniform_real_distribution<float> distribution(0.25f, 1.0f);
		std::uniform_real_distribution<float> posDistribution(-5.0f, 5.0f);
		//std::uniform_real_distribution<float> posDistributionY(-20.0f, 0.0f);
		
		//debugParticleCount_ = 1;
		
		const int clusterCount = 1;
		glm::vec3 clusterOffset = glm::vec3(-clusterCount * 10.0f, 0.0f, 0.0f);
		const int clusterSteps = debugParticleCount_ / clusterCount;
		int currentCluster = -1;
		for (int i = 0; i < debugParticleCount_; ++i)
		{
			const float radius = distribution(generator);
			
			//const float yPosition = posDistributionY(generator);
			//const float scale = 1.0f;
			if (i % clusterSteps == 0)
			{
				currentCluster++;
			}
			const glm::vec3 position = glm::vec3(256, 256, 256) +
				clusterPositions[currentCluster] +
				glm::vec3(posDistribution(generator), posDistribution(generator), posDistribution(generator));
			const float radiusSquared = radius * radius;

			radi_.push_back(radius);
			particles_.push_back({ position, radiusSquared });
		}

		//radi_.push_back(1.5f);	particles_.push_back({ glm::vec3(272, 270, 273), radi_.back() * radi_.back() });
		radi_.push_back(0.99f); particles_.push_back({ glm::vec3(259 - 3, 255, 270), radi_.back() * radi_.back() });
		//radi_.push_back(0.5f);	particles_.push_back({ glm::vec3(272, 270, 281), radi_.back() * radi_.back() });
		//radi_.push_back(0.25f);
		//particles_.push_back({ glm::vec3(257, 273, 265), radi_.back() * radi_.back() });
		debugParticleCount_ = static_cast<int>(particles_.size());
	}

	void ParticleSystems::SetGridOffset(const glm::vec3& gridOffset)
	{
		gridOffset_ = gridOffset;
	}

	void ParticleSystems::RequestResources(BufferManager* bufferManager, int frameCount, int atlasImageIndex)
	{
		{
			BufferManager::BufferInfo cbInfo;
			cbInfo.typeBits = BufferManager::BUFFER_CONSTANT_BIT | BufferManager::BUFFER_SCENE_BIT;
			cbInfo.pool = BufferManager::MEMORY_CONSTANT;
			cbInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			cbInfo.bufferingCount = frameCount;
			cbInfo.data = &cbData_;
			cbInfo.size = sizeof(CBData);
			cbIndex_ = bufferManager->Ref_RequestBuffer(cbInfo);
		}
		
		{
			BufferManager::BufferInfo storageBufferInfo;
			storageBufferInfo.typeBits = BufferManager::BUFFER_GRID_BIT;
			storageBufferInfo.pool = BufferManager::MEMORY_GRID;
			storageBufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			storageBufferInfo.bufferingCount = frameCount;
			for (int i = 0; i < GPU_MAX; ++i)
			{
				//fill with one element, will later be resized properly
				storageBufferInfo.size = CalcStorageBufferSize(static_cast<GpuBufferType>(i));
				storageBuffers_[i].index = bufferManager->Ref_RequestBuffer(storageBufferInfo);
				storageBuffers_[i].size = storageBufferInfo.size;
			}
		}
		
		atlasImageIndex_ = atlasImageIndex;
	}

	int ParticleSystems::GetShaderBinding(ShaderBindingManager* bindingManager, int frameCount)
	{
		ShaderBindingManager::BindingInfo bindingInfo = {};
		bindingInfo.pass = SUBPASS_VOLUME_ADAPTIVE_PARTICLES;
		bindingInfo.resourceIndex = { atlasImageIndex_, cbIndex_ };
		bindingInfo.stages = { VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_COMPUTE_BIT };
		bindingInfo.types = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER };
		bindingInfo.Ref_image = true;
		bindingInfo.refactoring_ = { true, true };
		for (const auto& storageBuffer : storageBuffers_)
		{
			bindingInfo.resourceIndex.push_back(storageBuffer.index);
			bindingInfo.stages.push_back(VK_SHADER_STAGE_COMPUTE_BIT);
			bindingInfo.types.push_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
			bindingInfo.refactoring_.push_back(true);
		}
		bindingInfo.setCount = frameCount;
		
		return bindingManager->RequestShaderBinding(bindingInfo);
	}

	void ParticleSystems::OnLoadScene(const Scene* scene)
	{
		//const auto& particleSystemInfos = scene->GetParticleTransforms();
		int offset = 0;
		//for (const auto& systemInfo : particleSystemInfos)
		//{
		//	ParticleSystem particleSystem;
		//	particleSystem.SetTransform(systemInfo, gridOffset_);
		//	particleSystem.SetParticleOffset(offset);
		//
		//	const int particleCount = particleSystem.GetParticleCount();
		//	particleSystems_.push_back(particleSystem);
		//	offset += particleCount;
		//}
		offset += debugParticleCount_;
		maxParticles_ = offset;
	}

	void ParticleSystems::Update(float dt)
	{
		////if debug particles are used do not update with dynamic particle systems
		//if (debugParticleCount_ < 0)
		//{
		//	particles_.clear();
		//	radi_.clear();
		//
		//	for (auto& particleSystem : particleSystems_)
		//	{
		//		particleSystem.Update(dt);
		//
		//		const int activeCount = particleSystem.GetActiveParticleCount();
		//		const auto& activeParticles = particleSystem.GetParticles();
		//		particles_.insert(particles_.end(), activeParticles.begin(), std::next(activeParticles.begin(), activeCount));
		//		const auto& radi = particleSystem.GetRadi();
		//		radi_.insert(radi_.end(), radi.begin(), std::next(radi.begin(), activeCount));
		//	}
		//}
	}

	void ParticleSystems::GridInsertParticleNodes(const glm::vec3& worldOffset, GridLevel* childLevel)
	{
		nodeParticleMapping.clear();

		const float cellSize = childLevel->GetGridCellSize();

		for (size_t i = 0; i < particles_.size(); ++i)
		{
			//Calculate the minimum and maximum covered nodes
			const glm::vec3 radiusOffset = glm::vec3(radi_[i]);
			const glm::vec3 min = particles_[i].position - radiusOffset;
			const glm::vec3 max = ceil((particles_[i].position + radiusOffset) / cellSize) * cellSize;

			for (float x = min.x; x < max.x; x += cellSize)
			{
				for (float y = min.y; y < max.y; y += cellSize)
				{
					for (float z = min.z; z < max.z; z += cellSize)
					{
						const int indexNode = childLevel->AddNode(glm::vec3(x, y, z));
						nodeParticleMapping[indexNode].push_back(static_cast<int>(i));
					}
				}
			}
		}
	}

	namespace
	{
		//Calculates the center of the first texel of the node in grid space
		glm::vec3 CalcGridOffset(int index, const NodeData& nodeData, GridLevel* parentLevel, float cellSize, float texelSize)
		{
			//position of node without parent offset
			glm::vec3 gridPos = glm::vec3(nodeData.gridPos_[index]) * cellSize;
			int parentIndex = nodeData.parentIndices_[index];

			glm::vec3 parentOffset = glm::vec3(parentLevel->GetNodeData().gridPos_[parentIndex]) * parentLevel->GetGridCellSize();
			return gridPos + parentOffset + texelSize * 0.5f;
		}
	}

	void ParticleSystems::UpdateGpuData(GridLevel* parentLevel, GridLevel* childLevel, int atlasResolution)
	{
		nodeData_.clear();
		nodeParticleIndices_.clear();
		
		int particleOffset = 0;
		const auto& childNodeData = childLevel->GetNodeData();
		
		for (const auto& nodeParticles : nodeParticleMapping)
		{
			Node nodeData;
			nodeData.worldOffset = CalcGridOffset(nodeParticles.first, childNodeData,
				parentLevel, childLevel->GetGridCellSize(), cbData_.texelSize);
			nodeData.particleCount = static_cast<int>(nodeParticles.second.size());
			nodeData.imageOffset = childNodeData.GetNodeInfos()[nodeParticles.first].textureOffset;
			nodeData.particleOffset = particleOffset;
			nodeData_.push_back(nodeData);
			
			particleOffset += nodeData.particleCount;
			nodeParticleIndices_.insert(nodeParticleIndices_.end(), nodeParticles.second.begin(), nodeParticles.second.end());
		}
	}

	void ParticleSystems::UpdateCBData(const GridLevel* gridLevel)
	{
		cbData_.texelSize = gridLevel->GetGridCellSize() / GridConstants::imageResolution;

		const auto& particleValue = GuiPass::GetVolumeState().particleValue;
		cbData_.textureValue = glm::vec4(particleValue.scattering, particleValue.scattering + particleValue.absorption,
			particleValue.phaseG, 0.0f);
	}

	bool ParticleSystems::ResizeGpuResources(std::vector<ResourceResize>& resourceResizes)
	{
		bool resize = false;
		for (int i = 0; i < GPU_MAX; ++i)
		{
			const auto newSize = CalcStorageBufferSize(static_cast<GpuBufferType>(i));
			if (storageBuffers_[i].size < newSize)
			{
				resize = true;
				storageBuffers_[i].size = newSize;
			}
			resourceResizes.push_back(storageBuffers_[i]);
		}
		return resize;
	}

	void ParticleSystems::UpdateGpuResources(BufferManager* bufferManager, int frameIndex)
	{
		const int bufferTypeBits = BufferManager::BUFFER_GRID_BIT;
		for (int i = 0; i < GPU_MAX; ++i)
		{
			const int bufferIndex = storageBuffers_[i].index;
			auto dataPtr = bufferManager->Ref_Map(bufferIndex, frameIndex, bufferTypeBits);
		
			void* source = nullptr;
			VkDeviceSize copySize = CalcStorageBufferSize(static_cast<GpuBufferType>(i));
		
			switch (i)
			{
			case GPU_STORAGE_NODE:
				source = nodeData_.data();
				break;
			case GPU_STORAGE_NODE_PARTICLE:
				source = nodeParticleIndices_.data();
				break;
			case GPU_STORAGE_PARTICLE:
				source = particles_.data();
				break;
			default:
				printf("Invalid storage buffer index %d while updating\n", i);
			}
		
			memcpy(dataPtr, source, copySize);
			bufferManager->Ref_Unmap(bufferIndex, frameIndex, bufferTypeBits);
		}
	}

	void ParticleSystems::Dispatch(ImageManager* imageManager, VkCommandBuffer commandBuffer, int frameIndex)
	{
		const uint32_t dispatchSize = static_cast<uint32_t>(nodeData_.size());
		if (dispatchSize > 0)
		{
			auto& queryPool = Wrapper::QueryPool::GetInstance();
			queryPool.TimestampStart(commandBuffer, Wrapper::TIMESTAMP_GRID_PARTICLES, frameIndex);

			vkCmdDispatch(commandBuffer, dispatchSize, 1, 1);

			queryPool.TimestampEnd(commandBuffer, Wrapper::TIMESTAMP_GRID_PARTICLES, frameIndex);
		}

		ImageManager::BarrierInfo barrierInfo{};
		barrierInfo.imageIndex = atlasImageIndex_;
		barrierInfo.type = ImageManager::BARRIER_WRITE_WRITE;
		auto barriers = imageManager->Barrier({ barrierInfo });

		Wrapper::PipelineBarrierInfo pipelineBarrierInfo{};
		pipelineBarrierInfo.src = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		pipelineBarrierInfo.dst = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		pipelineBarrierInfo.AddImageBarriers(barriers);
		Wrapper::AddPipelineBarrier(commandBuffer, pipelineBarrierInfo);
	}

	VkDeviceSize ParticleSystems::CalcStorageBufferSize(GpuBufferType bufferType)
	{
		switch (bufferType)
		{
			case GPU_STORAGE_NODE:
				return sizeof(NodeData) * std::max(nodeData_.size(), 1ull);
			case GPU_STORAGE_NODE_PARTICLE:
			{
				return sizeof(int) * std::max(nodeParticleIndices_.size(), 1ull);
			}
			case GPU_STORAGE_PARTICLE:
				return sizeof(Particle) * std::max(maxParticles_, 1);
			default:
				printf("Invalide particle systems buffer type %d\n", bufferType);
				return 0;
		}
	}	
}