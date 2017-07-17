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

#include "AdaptiveGrid.h"

#include "..\..\passResources\ShaderBindingManager.h"
#include "..\..\passes\Passes.h"
#include "..\..\ShadowMap.h"
#include "..\..\resources\BufferManager.h"
#include "..\..\resources\ImageManager.h"

#include "..\..\passes\GuiPass.h"

#include "..\..\wrapper\Surface.h"
#include "..\..\wrapper\Barrier.h"
#include "..\..\wrapper\QueryPool.h"

#include "..\..\..\scene\Scene.h"

#include "..\..\..\utility\Status.h"

#include "AdaptiveGridConstants.h"
#include "debug\DebugData.h"

#include <glm\gtc\matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm\gtx\component_wise.hpp>
#include <random>
#include <functional>

namespace Renderer
{
	constexpr int debugScreenDivision_ = 1;

	AdaptiveGrid::AdaptiveGrid(float worldCellSize) :
		worldCellSize_{ worldCellSize }
	{
		//min count for grid level data
		gridLevelData_.resize(3);
	}

	void AdaptiveGrid::SetWorldExtent(const glm::vec3& min, const glm::vec3& max)
	{
		glm::vec3 scale;
		const auto worldScale = max - min;

		//grid resolution of root node is 1
		std::vector<int> resolutions = { 1, GridConstants::nodeResolution, GridConstants::nodeResolution };
		glm::vec3 maxScale = CalcMaxScale(resolutions, GridConstants::nodeResolution);

		//add additional levels until the whole world is covered
		/*while (glm::compMax(worldScale) > glm::compMax(maxScale))
		{
			resolutions.push_back(gridResolution_);
			maxScale = CalcMaxScale(resolutions, imageResolution_);
		}*/
		scale = maxScale;
		printf("Scene size %f %f %f\n", scale.x, scale.y, scale.z);

		//Calculate world extent
		const auto center = glm::vec3(0,0,0);
		const auto halfScale = scale * 0.5f;
		worldBoundingBox_.min = center - halfScale;
		worldBoundingBox_.max = center + halfScale;

		raymarchingData_.gridMinPosition = worldBoundingBox_.min;
		radius_ = glm::distance(worldBoundingBox_.max, glm::vec3(0.0f));

		int globalResolution = resolutions[0];
		//Initialize all grid levels
		for (size_t i = 0; i < resolutions.size(); ++i)
		{
			if (i > 0)
			{
				globalResolution *= resolutions[i];
			}
			gridLevels_.push_back({ resolutions[i] });
			gridLevels_.back().SetWorldExtend(worldBoundingBox_.min, scale, static_cast<float>(globalResolution));
		}
		for (size_t i = 1; i < gridLevels_.size(); ++i)
		{
			gridLevels_[i].SetParentLevel(&gridLevels_[i - 1]);
		}
		gridLevels_.back().SetLeafLevel();
		mostDetailedParentLevel_ = static_cast<int>(gridLevels_.size() - 2);
		raymarchingData_.maxLevel = static_cast<int>(resolutions.size() - 1);

		gridLevelData_.resize(resolutions.size());
		for (size_t i = 0; i < resolutions.size(); ++i)
		{
			gridLevelData_[i].gridCellSize = gridLevels_[i].GetGridCellSize();
			gridLevelData_[i].gridResolution = GridConstants::nodeResolution;
			gridLevelData_[i].childCellSize = gridLevels_[i].GetGridCellSize() / GridConstants::nodeResolution;
		}

		groundFog_.SetSizes(gridLevels_[1].GetGridCellSize(), scale.x);
		particleSystems_.SetGridOffset(worldBoundingBox_.min);

		debugFilling_.SetWorldOffset(worldBoundingBox_.min);
		//debugFilling_.AddDebugNode({0,-16,0}, 0.0f, 10.0f, 0.0f, 1 );
		//debugFilling_.AddDebugNode({10,0,14 }, 0.0f, 0.0f, 0.0f, 2);

		Status::UpdateGrid(scale, worldBoundingBox_.min);
		Status::SetParticleSystems(&particleSystems_);
	}

	void AdaptiveGrid::ResizeConstantBuffers(BufferManager* bufferManager)
	{
		const auto levelBufferSize = sizeof(LevelData) * gridLevelData_.size();
		bufferManager->Ref_RequestBufferResize(cbIndices_[CB_RAYMARCHING_LEVELS], levelBufferSize);
	}

	void AdaptiveGrid::RequestResources(ImageManager* imageManager, BufferManager* bufferManager, int frameCount)
	{
		BufferManager::BufferInfo bufferInfo;
		bufferInfo.typeBits = BufferManager::BUFFER_CONSTANT_BIT | BufferManager::BUFFER_SCENE_BIT;
		bufferInfo.pool = BufferManager::MEMORY_CONSTANT;
		bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		bufferInfo.bufferingCount = frameCount;
		frameCount_ = frameCount;

		auto cbIndex = CB_RAYMARCHING;
		{
			bufferInfo.data = &raymarchingData_;
			bufferInfo.size = sizeof(RaymarchingData);
			cbIndices_[cbIndex] = bufferManager->Ref_RequestBuffer(bufferInfo);
		}
		cbIndex = CB_RAYMARCHING_LEVELS;
		{
			bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			bufferInfo.data = gridLevelData_.data();
			cbIndices_[cbIndex] = bufferManager->Ref_RequestBuffer(bufferInfo);
		}

		{
			BufferManager::BufferInfo gridBufferInfo;
			gridBufferInfo.pool = BufferManager::MEMORY_GRID;
			gridBufferInfo.size = sizeof(uint32_t);
			gridBufferInfo.typeBits = BufferManager::BUFFER_GRID_BIT;
			gridBufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			gridBufferInfo.bufferingCount = frameCount;
			for (size_t i = GPU_BUFFER_NODE_INFOS; i < GPU_MAX; ++i)
			{
				gpuResources_[i] = { bufferManager->Ref_RequestBuffer(gridBufferInfo), 1 };
			}
		}

		imageAtlas_.RequestResources(imageManager, bufferManager, ImageManager::MEMORY_POOL_GRID, frameCount);
		const int atlasImageIndex = imageAtlas_.GetImageIndex();
		const int atlasBufferIndex = imageAtlas_.GetBufferIndex();
		
		debugFilling_.SetGpuResources(atlasImageIndex, atlasBufferIndex);

		globalVolume_.RequestResources(bufferManager, frameCount, atlasImageIndex);
		groundFog_.RequestResources(imageManager, bufferManager, frameCount, atlasImageIndex);
		particleSystems_.RequestResources(bufferManager, frameCount, atlasImageIndex);
		mipMapping_.RequestResources(imageManager, bufferManager, frameCount, atlasImageIndex);
		neighborCells_.RequestResources(bufferManager, frameCount, atlasImageIndex);

		for (size_t i = 0; i < gpuResources_.size(); ++i)
		{
			gpuResources_[i].maxSize = sizeof(int);
		}
	}

	void AdaptiveGrid::SetImageIndices(int raymarching, int depth, int shadowMap, int noise)
	{
		raymarchingImageIndex_ = raymarching;
		depthImageIndex_ = depth;
		shadowMapIndex_ = shadowMap;
		noiseImageIndex_ = noise;

		debugTraversal_.SetImageIndices(
			imageAtlas_.GetImageIndex(),
			depthImageIndex_,
			shadowMap,
			noise
		);
	}

	void AdaptiveGrid::OnLoadScene(const Scene* scene)
	{
		particleSystems_.OnLoadScene(scene);
	}

	void AdaptiveGrid::UpdateParticles(float dt)
	{
		particleSystems_.Update(dt);
	}

	int AdaptiveGrid::GetShaderBinding(ShaderBindingManager* bindingManager, Pass pass)
	{
		ShaderBindingManager::BindingInfo bindingInfo = {};
		switch (pass)
		{
		case GRID_PASS_GLOBAL:
			return globalVolume_.GetShaderBinding(bindingManager, frameCount_);
		case GRID_PASS_GROUND_FOG:
			return groundFog_.GetShaderBinding(bindingManager, frameCount_);
		case GRID_PASS_PARTICLES:
			return particleSystems_.GetShaderBinding(bindingManager, frameCount_);
		case GRID_PASS_DEBUG_FILLING:
			return debugFilling_.GetShaderBinding(bindingManager, frameCount_);
		case GRID_PASS_MIPMAPPING:
			return mipMapping_.GetShaderBinding(bindingManager, frameCount_, pass - GRID_PASS_MIPMAPPING);
		case GRID_PASS_MIPMAPPING_MERGING:
			return mipMapping_.GetShaderBinding(bindingManager, frameCount_, pass - GRID_PASS_MIPMAPPING);
		case GRID_PASS_NEIGHBOR_UPDATE:
			return neighborCells_.GetShaderBinding(bindingManager, frameCount_);
		case GRID_PASS_RAYMARCHING:
		{
			bindingInfo.pass = SUBPASS_VOLUME_ADAPTIVE_RAYMARCHING;
			bindingInfo.resourceIndex = {
				raymarchingImageIndex_,
				depthImageIndex_,
				imageAtlas_.GetImageIndex(),
				shadowMapIndex_,
				noiseImageIndex_,
				gpuResources_[GPU_BUFFER_NODE_INFOS].index,
				gpuResources_[GPU_BUFFER_ACTIVE_BITS].index,
				gpuResources_[GPU_BUFFER_BIT_COUNTS].index,
				gpuResources_[GPU_BUFFER_CHILDS].index,
				cbIndices_[CB_RAYMARCHING],
				cbIndices_[CB_RAYMARCHING_LEVELS]
			};
			bindingInfo.stages = {
				VK_SHADER_STAGE_COMPUTE_BIT,
				VK_SHADER_STAGE_COMPUTE_BIT,
				VK_SHADER_STAGE_COMPUTE_BIT,
				VK_SHADER_STAGE_COMPUTE_BIT,
				VK_SHADER_STAGE_COMPUTE_BIT,
				VK_SHADER_STAGE_COMPUTE_BIT,
				VK_SHADER_STAGE_COMPUTE_BIT,
				VK_SHADER_STAGE_COMPUTE_BIT,
				VK_SHADER_STAGE_COMPUTE_BIT,
				VK_SHADER_STAGE_COMPUTE_BIT,
				VK_SHADER_STAGE_COMPUTE_BIT };
			bindingInfo.types = {
				VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER };
			bindingInfo.refactoring_ = { false, false, true, true, true, true, true, true, true, true, true };
			bindingInfo.setCount = frameCount_;
		}break;
		default:
			printf("Get shader bindings from adaptive grid for invalid type\n");
			break;
		}
		return bindingManager->RequestShaderBinding(bindingInfo);
	}

	void AdaptiveGrid::Update(BufferManager* bufferManager, ImageManager* imageManager, Scene* scene, Surface* surface, ShadowMap* shadowMap, int frameIndex)
	{
		UpdateCBData(scene, surface, shadowMap);

		UpdateGrid(scene);
		ResizeGpuResources(bufferManager, imageManager);
		UpdateGpuResources(bufferManager, frameIndex);

		if (GuiPass::GetPostProcessState().debugRendering)
		{
			UpdateBoundingBoxes();
		}
	}

	void AdaptiveGrid::Dispatch(QueueManager* queueManager, ImageManager* imageManager, BufferManager* bufferManager,
		VkCommandBuffer commandBuffer, Pass pass, int frameIndex, int level)
	{
		switch (pass)
		{
		case GRID_PASS_GLOBAL:
		{
			globalVolume_.Dispatch(imageManager, commandBuffer, frameIndex);
		}
		break;
		case GRID_PASS_GROUND_FOG:
		{			
			groundFog_.Dispatch(queueManager, imageManager, bufferManager, commandBuffer, frameIndex);
		}	break;
		case GRID_PASS_PARTICLES:
			break;
		case GRID_PASS_DEBUG_FILLING:
			debugFilling_.Dispatch(imageManager, commandBuffer);
			break;
		case GRID_PASS_MIPMAPPING:
			mipMappingStarted_ = true;
		case GRID_PASS_MIPMAPPING_MERGING:
			mipMapping_.Dispatch(imageManager, bufferManager, commandBuffer, frameIndex, level, pass - GRID_PASS_MIPMAPPING);
			break;
		case GRID_PASS_NEIGHBOR_UPDATE:
		{
			neighborCells_.Dispatch(commandBuffer, imageManager, level, mipMappingStarted_, frameIndex);
		}
		break;
		case GRID_PASS_RAYMARCHING:
			mipMappingStarted_ = false;
			break;
		default:
			break;
		}
	}

	void AdaptiveGrid::UpdateParticles(ImageManager* imageManager, VkCommandBuffer commandBuffer, int frameIndex)
	{
		const int dispatchSize = particleSystems_.GetDispatchCount();
		if (dispatchSize > 0)
		{
			auto& queryPool = Wrapper::QueryPool::GetInstance();
			queryPool.TimestampStart(commandBuffer, Wrapper::TIMESTAMP_GRID_PARTICLES, frameIndex);

			vkCmdDispatch(commandBuffer, dispatchSize, 1, 1);

			queryPool.TimestampEnd(commandBuffer, Wrapper::TIMESTAMP_GRID_PARTICLES, frameIndex);
		}

		ImageManager::BarrierInfo barrierInfo{};
		barrierInfo.imageIndex = imageAtlas_.GetImageIndex();
		barrierInfo.type = ImageManager::BARRIER_WRITE_WRITE;
		auto barriers = imageManager->Barrier({ barrierInfo });

		Wrapper::PipelineBarrierInfo pipelineBarrierInfo{};
		pipelineBarrierInfo.src = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		pipelineBarrierInfo.dst = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		pipelineBarrierInfo.AddImageBarriers(barriers);
		Wrapper::AddPipelineBarrier(commandBuffer, pipelineBarrierInfo);
	}

	namespace
  {
    uint32_t CalcDispatchSize(uint32_t number, uint32_t multiple)
    {
      if (number % multiple == 0)
      {
        return number / multiple;
      }
      else
      {
        return number / multiple + 1;
      }
    }
  }

  void AdaptiveGrid::Raymarch(ImageManager* imageManager, VkCommandBuffer commandBuffer, uint32_t width, uint32_t height, int frameIndex)
  {
    if (initialized_)
    {
			const uint32_t dispatchX = CalcDispatchSize(width / debugScreenDivision_, 16);
      const uint32_t dispatchY = CalcDispatchSize(height / debugScreenDivision_, 16);

			auto& queryPool = Wrapper::QueryPool::GetInstance();
			queryPool.TimestampStart(commandBuffer, Wrapper::TIMESTAMP_GRID_RAYMARCHING, frameIndex);

			vkCmdDispatch(commandBuffer, dispatchX, dispatchY, 1);

			queryPool.TimestampEnd(commandBuffer, Wrapper::TIMESTAMP_GRID_RAYMARCHING, frameIndex);

			ImageManager::BarrierInfo imageAtlasBarrier{};
			imageAtlasBarrier.imageIndex = imageAtlas_.GetImageIndex();
			imageAtlasBarrier.type = ImageManager::BARRIER_READ_WRITE;
			ImageManager::BarrierInfo shadowMapBarrier{};
			shadowMapBarrier.imageIndex = shadowMapIndex_;
			shadowMapBarrier.type = ImageManager::BARRIER_READ_WRITE;
			auto barriers = imageManager->Barrier({ imageAtlasBarrier, shadowMapBarrier });

			Wrapper::PipelineBarrierInfo pipelineBarrierInfo{};
			pipelineBarrierInfo.src = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			pipelineBarrierInfo.dst = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			pipelineBarrierInfo.AddImageBarriers(barriers);
			Wrapper::AddPipelineBarrier(commandBuffer, pipelineBarrierInfo);
    }
  }

	void AdaptiveGrid::UpdateDebugTraversal(QueueManager* queueManager, BufferManager* bufferManager, ImageManager* imageManager)
	{
		const auto& volumeState = GuiPass::GetVolumeState();
		if (volumeState.debugTraversal && !previousFrameTraversal_)
		{
			previousFrameTraversal_ = true;
			const glm::ivec2 position = static_cast<glm::ivec2>(volumeState.cursorPosition);
			debugTraversal_.Traversal(queueManager, bufferManager, imageManager, position.x, position.y);
		}
		else
		{
			previousFrameTraversal_ = false;
		}
	}

	void AdaptiveGrid::UpdateCBData(Scene* scene, Surface* surface, ShadowMap* shadowMap)
  {
		{
			const auto& camera = scene->GetCamera();
			raymarchingData_.viewPortToWorld = glm::inverse(camera.GetViewProj());
			raymarchingData_.nearPlane = camera.nearZ_;
			raymarchingData_.farPlane = camera.farZ_;
			raymarchingData_.shadowCascades = shadowMap->GetShadowMatrices();

			const auto& screenSize = surface->GetSurfaceSize();
			raymarchingData_.screenSize = { screenSize.width, screenSize.height};
			static bool printedScreenSize = false;
			if (!printedScreenSize)
			{
				printf("Raymarching resolution %f %f\n", raymarchingData_.screenSize.x, raymarchingData_.screenSize.y);
				printedScreenSize = true;
			}
		}

		{
			const auto& lightingState = GuiPass::GetLightingState();
			const auto& lv = lightingState.lightVector;
			const auto& li = lightingState.irradiance;
			raymarchingData_.irradiance = glm::vec3(li[0], li[1], li[2]);
			const auto lightDir = glm::vec4(lv.x, lv.y, lv.z, 0.0f);
			raymarchingData_.lightDirection = lightDir;

			const glm::vec4 shadowRayNormalPos = glm::vec4(0, 0, 0, 1) 
				+ radius_ * lightDir;
			raymarchingData_.shadowRayPlaneNormal = lightDir;
			raymarchingData_.shadowRayPlaneDistance = glm::dot(-lightDir, shadowRayNormalPos);
		}

		{
			const auto& volumeState = GuiPass::GetVolumeState();
			globalMediumData_.scattering = volumeState.scattering;
			globalMediumData_.extinction = volumeState.absorption + volumeState.scattering;
			globalMediumData_.phaseG = volumeState.phaseG;

			raymarchingData_.lodScale_Reciprocal = 1.0f / volumeState.lodScale;
			raymarchingData_.globalScattering = { globalMediumData_.scattering, globalMediumData_.extinction, globalMediumData_.phaseG, 0.0f };
			raymarchingData_.maxDepth = volumeState.maxDepth;
			raymarchingData_.jitteringScale = volumeState.jitteringScale;
			raymarchingData_.maxSteps = volumeState.stepCount;
			raymarchingData_.exponentialScale = log(volumeState.maxDepth);

			//TODO check why these values are double
			volumeMediaData_.scattering = volumeState.scatteringVolumes;
			volumeMediaData_.extinction = volumeState.absorptionVolumes + volumeState.scatteringVolumes;
			volumeMediaData_.phaseG = volumeState.phaseVolumes;
			
			if (volumeState.debugTraversal)
			{
				DebugData::raymarchData_ = raymarchingData_;
				DebugData::levelData_.data = gridLevelData_;
			}

			groundFog_.UpdateCBData(volumeState.fogHeight,
				volumeState.scatteringVolumes,
				volumeState.absorptionVolumes,
				volumeState.phaseVolumes,
				volumeState.noiseScale);
			globalVolume_.UpdateCB(&groundFog_);

			for (auto& levelData : gridLevelData_)
			{
				//levelData.minStepSize = levelData.gridCellSize / volumeState.lodScale;
				levelData.shadowRayStepSize = levelData.gridCellSize / 
					static_cast<float>(volumeState.shadowRayPerLevel);
				levelData.texelScale = GridConstants::nodeResolution /
					((GridConstants::nodeResolution + 2) * imageAtlas_.GetSideLength() *
						levelData.gridCellSize);
			}
		}

		{
			static std::default_random_engine generator;
			static std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
			static auto RandPos = std::bind(distribution, generator);
			raymarchingData_.randomness = { RandPos(), RandPos(), RandPos() };
		}

		{
			const float nodeResolutionWithBorder = static_cast<float>(GridConstants::nodeResolution + 2);
			raymarchingData_.textureFraction = GridConstants::nodeResolution / nodeResolutionWithBorder / raymarchingData_.atlasSideLength;
			raymarchingData_.textureOffset = 1.0f / nodeResolutionWithBorder / raymarchingData_.atlasSideLength;
			raymarchingData_.atlasTexelToNodeTexCoord = 1.0f / GridConstants::imageResolution / 
				static_cast<float>(raymarchingData_.atlasSideLength);
		}
    
		particleSystems_.UpdateCBData(&gridLevels_[2]);
  }

  void AdaptiveGrid::UpdateGrid(Scene* scene)
  {
		for (auto& gridLevel : gridLevels_)
		{
			gridLevel.Reset();
		}

		gridLevels_[0].AddNode({ 0,0,0 });

		//gridLevels_[1].AddNode({ 256, 256, 256 });
		//gridLevels_[2].AddNode({ 256, 258,256 });
		//gridLevels_[2].AddNode({ 258, 256, 256 });
		//gridLevels_[2].AddNode({ 256, 256, 258 });
		//gridLevels_[2].AddNode({ 254, 256,256 });

		debugFilling_.InsertDebugNodes(gridLevels_);
		
		groundFog_.UpdateGridCells(&gridLevels_[1]);
		particleSystems_.GridInsertParticles(raymarchingData_.gridMinPosition, &gridLevels_[2]);

		for (auto& level : gridLevels_)
		{
			level.Update();
		}
		imageAtlas_.UpdateSize(gridLevels_.back().GetImageOffset());
		const int atlasSideLength = imageAtlas_.GetSideLength();
		for (auto& level : gridLevels_)
		{
			level.UpdateImageIndices(atlasSideLength);
		}
		particleSystems_.UpdateGpuData(&gridLevels_[1], &gridLevels_[2], atlasSideLength);

		
		const int maxParentLevel = static_cast<int>(gridLevels_.size() - 1);
		mipMapping_.UpdateAtlasProperties(atlasSideLength);
		for (size_t i = 0; i < maxParentLevel; ++i)
		{
			mipMapping_.UpdateMipMapNodes(&gridLevels_[i], &gridLevels_[i + 1]);
		}
		//neighborCells_.CalculateNeighbors(gridLevels_);

		debugFilling_.SetImageCount(&gridLevels_[2]);

		debugFilling_.UpdateDebugNodes(gridLevels_);

		volumeMediaData_.atlasSideLength = atlasSideLength;
		volumeMediaData_.imageResolutionOffset = GridConstants::imageResolution;
		raymarchingData_.atlasSideLength = atlasSideLength;
		//neighborCells_.UpdateAtlasProperties(atlasSideLength);
  }

	

	std::array<VkDeviceSize, AdaptiveGrid::GPU_MAX> AdaptiveGrid::GetGpuResourceSize()
	{
		std::array<VkDeviceSize, GPU_MAX> totalSizes{};
		for (const auto& level : gridLevels_)
		{
			totalSizes[GPU_BUFFER_NODE_INFOS] += level.GetNodeInfoSize();
			totalSizes[GPU_BUFFER_ACTIVE_BITS] += level.GetActiveBitSize();
			totalSizes[GPU_BUFFER_BIT_COUNTS] += level.GetBitCountsSize();
			totalSizes[GPU_BUFFER_CHILDS] += level.GetChildSize();
		}
		
		return totalSizes;
	}

	void AdaptiveGrid::ResizeGpuResources(BufferManager* bufferManager, ImageManager* imageManager)
  {
    bool resize = false;
		std::vector<ResourceResize>resourceResizes;
		const auto totalSizes =	GetGpuResourceSize();
		imageAtlas_.Resize(imageManager);

		for (int i = 0; i < GPU_MAX; ++i)
		{
			const auto totalSize = totalSizes[i];
			auto& maxSize = gpuResources_[i].maxSize;
			if (maxSize < totalSize)
			{
				resize = true;
				maxSize = totalSize;
			}
			resourceResizes.push_back({ maxSize, gpuResources_[i].index });
		}

		resourceResizes.push_back({ static_cast<VkDeviceSize>(groundFog_.GetPerCellBufferSize()), groundFog_.GetPerCellBufferIndex() });
		resize = particleSystems_.ResizeGpuResources(resourceResizes) ? true : resize;
		resize = mipMapping_.ResizeGpuResources(imageManager, resourceResizes) ? true : resize;
		resize = neighborCells_.ResizeGpuResources(resourceResizes) ? true : resize;

		if (!initialized_)
		{
			resize = true;
			initialized_ = true;
		}

    if (resize)
    {
      bufferManager->Ref_ResizeBuffers(resourceResizes, BufferManager::MEMORY_GRID);
    }
    resizing_ = resize;
  }
	
	namespace 
	{
		struct ResourceInfo
		{
			void* dataPtr;
			int offset;
		};
	}

	void* AdaptiveGrid::GetDebugBufferCopyDst(GridLevel::BufferType type, int size)
	{
		switch (type)
		{
		case GridLevel::BUFFER_NODE_INFOS:
			DebugData::nodeInfos_.data.resize(size / sizeof(NodeInfo));
			return DebugData::nodeInfos_.data.data();
		case GridLevel::BUFFER_ACTIVE_BITS:
			DebugData::activeBits_.data.resize(size / sizeof(uint32_t));
			return DebugData::activeBits_.data.data();
		case GridLevel::BUFFER_BIT_COUNTS:
			DebugData::bitCounts_.data.resize(size / sizeof(int));
			return DebugData::bitCounts_.data.data();
		case GridLevel::BUFFER_CHILDS:
			DebugData::childIndices_.indices.resize(size / sizeof(int));
			return DebugData::childIndices_.indices.data();
		default:
			printf("Invalid buffer type to get copy dst\n");
			break;
		}
		return nullptr;
	}

  void AdaptiveGrid::UpdateGpuResources(BufferManager* bufferManager, int frameIndex)
  {
		const auto& volumeState = GuiPass::GetVolumeState();

		std::array<ResourceInfo, GPU_MAX> offsets{};
		for (int i = 0; i < GPU_MAX; ++i)
		{
			offsets[i].dataPtr =
				bufferManager->Ref_Map(gpuResources_[i].index, frameIndex, BufferManager::BUFFER_GRID_BIT);
		}

		for (size_t i = 0; i < gridLevels_.size(); ++i)
		{
			//TODO check if neccessary
			gridLevelData_[i].nodeArrayOffset = offsets[GridLevel::BUFFER_NODE_INFOS].offset;
			gridLevelData_[i].childArrayOffset = offsets[GridLevel::BUFFER_CHILDS].offset;
			gridLevelData_[i].nodeOffset = gridLevelData_[i].nodeArrayOffset / sizeof(NodeInfo);
			gridLevelData_[i].childOffset = gridLevelData_[i].childArrayOffset / sizeof(int);

			for (int resourceIndex = 0; resourceIndex < GridLevel::BUFFER_MAX; ++resourceIndex)
			{
				const auto bufferType = static_cast<GridLevel::BufferType>(resourceIndex);
				const int offset = offsets[resourceIndex].offset;

				offsets[resourceIndex].offset = gridLevels_[i].CopyBufferData(offsets[resourceIndex].dataPtr,
					static_cast<GridLevel::BufferType>(resourceIndex), offsets[resourceIndex].offset);
				
				if (volumeState.debugTraversal)
				{
					gridLevels_[i].CopyBufferData(GetDebugBufferCopyDst(bufferType, offsets[resourceIndex].offset),
						bufferType, offset);
				}
			}
		}

		for (int i = GPU_BUFFER_NODE_INFOS; i < GPU_MAX; ++i)
		{
			bufferManager->Ref_Unmap(gpuResources_[i].index, frameIndex, BufferManager::BUFFER_GRID_BIT);
		}

		groundFog_.UpdatePerCellBuffer(bufferManager, &gridLevels_[1], frameIndex, imageAtlas_.GetSideLength());
		particleSystems_.UpdateGpuResources(bufferManager, frameIndex);
		mipMapping_.UpdateGpuResources(bufferManager, frameIndex);
		neighborCells_.UpdateGpuResources(bufferManager, frameIndex);
	}

  void AdaptiveGrid::UpdateBoundingBoxes()
  {
		const bool fillImageIndices = GuiPass::GetVolumeState().debugFilling;
    debugBoundingBoxes_.clear();
		for (auto& level : gridLevels_)
    {
			level.UpdateBoundingBoxes();
      
			const auto& nodeData = level.GetNodeData();

			glm::vec4 color = glm::vec4(0, 1, 0, 1);
			for (size_t i = 0; i < level.nodeBoundingBoxes_.size(); ++i)
			{
				if (fillImageIndices)
				{
					color = glm::vec4(nodeData.imageInfos_[i].image, 1.0f)
						/ static_cast<float>((imageAtlas_.GetSideLength() * GridConstants::imageResolution));
				}
				debugBoundingBoxes_.push_back({
					WorldMatrix(level.nodeBoundingBoxes_[i]), color
				});
			}
    }
		debugBoundingBoxes_.push_back({ WorldMatrix(worldBoundingBox_), glm::vec4(0,1,0,0) });
  }

  glm::vec3 AdaptiveGrid::CalcMaxScale(std::vector<int> levelResolutions, int textureResolution)
  {
    glm::vec3 scale = glm::vec3(worldCellSize_) * static_cast<float>(textureResolution);
    for (const auto res : levelResolutions)
    {
      scale *= static_cast<float>(res);
    }
    return scale;
  }
}