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

#include "VolumePass.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm\gtx\transform.hpp>
#include <glm\gtx\euler_angles.hpp>

#include <random>
#include <functional>

#include "..\passResources\ShaderBindingManager.h"
#include "..\passResources\ShaderManager.h"

#include "..\resources\ImageManager.h"
#include "..\resources\QueueManager.h"
#include "..\resources\BufferManager.h"

#include "..\wrapper\Surface.h"
#include "..\wrapper\Synchronization.h"
#include "..\wrapper\Instance.h"
#include "..\wrapper\Barrier.h"

#include "..\ShadowMap.h"
#include "..\scene\adaptiveGrid\AdaptiveGrid.h"
#include "GuiPass.h"

#include "..\..\scene\Scene.h"

namespace Renderer
{
  VolumePass::VolumePass(ShaderBindingManager* bindingManager, RenderPassManager* renderPassManager) :
    Pass::Pass(bindingManager, renderPassManager)
  {}

  void VolumePass::SetShadowMap(ShadowMap* shadowMap, ImageManager* imageManager)
  {
    shadowMap_ = shadowMap;
    imageManager_ = imageManager;
  }

  void VolumePass::SetAdaptiveGrid(AdaptiveGrid* adaptiveGrid)
  {
    adaptiveGrid_ = adaptiveGrid;
  }

  void VolumePass::SetImageIndices(int raymarchingImage, int depthImage, int noiseArray)
  {
    raymarchingImage_ = raymarchingImage;
    depthImage_ = depthImage;
    noiseImageIndex_ = noiseArray;
  }

  void VolumePass::RequestResources(ImageManager* imageManager, BufferManager* bufferManager, int frameCount)
  {
    computePipelines_.resize(COMPUTE_MAX);

		auto subpass = COMPUTE_GLOBAL;
		{
			computePipelines_[subpass].shaderBinding = adaptiveGrid_->GetShaderBinding(bindingManager_, AdaptiveGrid::GRID_PASS_GLOBAL);
		}
    subpass = COMPUTE_GROUND_FOG;
		{
			computePipelines_[subpass].shaderBinding = adaptiveGrid_->GetShaderBinding(bindingManager_, AdaptiveGrid::GRID_PASS_GROUND_FOG);
		}
		subpass = COMPUTE_PARTICLES;
		{
			computePipelines_[subpass].shaderBinding = adaptiveGrid_->GetShaderBinding(bindingManager_, AdaptiveGrid::GRID_PASS_PARTICLES);
		}
		subpass = COMPUTE_DEBUG_FILLING;
		{
			computePipelines_[subpass].shaderBinding = adaptiveGrid_->GetShaderBinding(bindingManager_, AdaptiveGrid::GRID_PASS_DEBUG_FILLING);
		}
		subpass = COMPUTE_NEIGHBOR_UPDATE;
		{
			computePipelines_[subpass].shaderBinding = adaptiveGrid_->GetShaderBinding(bindingManager_, AdaptiveGrid::GRID_PASS_NEIGHBOR_UPDATE);
		}
    subpass = COMPUTE_MIPMAPPING;
		{
			computePipelines_[subpass].shaderBinding = adaptiveGrid_->GetShaderBinding(bindingManager_, AdaptiveGrid::GRID_PASS_MIPMAPPING);
		}
		subpass = COMPUTE_MIPMAPPING_MERGING;
		{
			computePipelines_[subpass].shaderBinding = adaptiveGrid_->GetShaderBinding(bindingManager_, AdaptiveGrid::GRID_PASS_MIPMAPPING_MERGING);
		}
    subpass = COMPUTE_RAYMARCHING;
    {
      computePipelines_[subpass].shaderBinding = adaptiveGrid_->GetShaderBinding(bindingManager_, AdaptiveGrid::GRID_PASS_RAYMARCHING);
    }
  }

  bool VolumePass::Create(VkDevice device, ShaderManager* shaderManager, FrameBufferManager* frameBufferManager,
    Surface* surface, ImageManager* imageManager)
  {
    if (!CreateComputePipelines(device, shaderManager)) { return false; }
    const int imageCount = surface->GetImageCount();
    if (!CreateCommandBuffers(device, QueueManager::QUEUE_COMPUTE, imageCount)) { return false; }

		for (int i = 0; i < COMPUTE_MAX; ++i)
		{
			if (!Wrapper::CreateVulkanSemaphore(device, &computePipelines_[i].passFinishedSemaphore))
			{
				return false;
			}
		}
		
    return true;
  }

  void VolumePass::UpdateBufferData(Surface* surface, Scene* scene)
  {
    const auto& volumeState = GuiPass::GetVolumeState();

    const float fogHeight = volumeState.groundFogHeight;

    const int centerCell = static_cast<int>((-scene->GetGrid()->GetMin().y) / scene->GetGrid()->GetCellSize().y);
    mediaData_.fogHeight = (1.0f - fogHeight) * scene->GetGrid()->GetResolution().y;

    const auto toWorld = glm::inverse(scene->GetCamera().GetProj() * scene->GetCamera().GetView());
    //raymarchingData_.toLight = shadowMap_->GetViewProj();
    const auto screenSize = surface->GetSurfaceSize();
    raymarchingData_.toWorld = toWorld;

    static std::default_random_engine generator;
    static std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    static auto RandPos = std::bind(distribution, generator);
    raymarchingData_.randomness_ = { RandPos(), RandPos(), RandPos(), RandPos() };
  }

	void VolumePass::Update(float dt)
	{
		adaptiveGrid_->UpdateParticles(dt);
	}

  void VolumePass::Render(Surface* surface, FrameBufferManager* frameBufferManager,
    QueueManager* queueManager, BufferManager* bufferManager,
    std::vector<VkImageMemoryBarrier>& memoryBarrier, VkSemaphore* startSemaphore, int frameIndex)
  {
		auto& commandBuffer = commandBuffers_[frameIndex];

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		auto result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
		assert(result == VK_SUCCESS);

		//imageManager_->Ref_ClearImage(commandBuffer, raymarchingImage_, { 0.0f, 0.0f, 0.0f, 1.0f });

		int parentGridLevel = adaptiveGrid_->GetMaxParentLevel() + 1;

		for (int pass = 0; pass < COMPUTE_MAX; ++pass)
		{
			RecordCommands(queueManager, bufferManager, imageManager_, surface, commandBuffer, 
				static_cast<ComputeSubpasses>(pass), frameIndex, parentGridLevel);

			//Mip mapping has to be performed for each level seperately starting with the most detailed level
			if (pass == COMPUTE_MIPMAPPING_MERGING && parentGridLevel > 0)
			{
				pass = COMPUTE_NEIGHBOR_UPDATE - 1;
			}
			if (pass == COMPUTE_NEIGHBOR_UPDATE)
			{
				parentGridLevel--;
			}
		}

		result = vkEndCommandBuffer(commandBuffer);
		assert(result == VK_SUCCESS);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &computePipelines_[COMPUTE_RAYMARCHING].passFinishedSemaphore;
		std::vector<VkSemaphore> waitSemaphores;
		waitSemaphores = { *startSemaphore };
		submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
		submitInfo.pWaitSemaphores = waitSemaphores.data();
		std::vector<VkPipelineStageFlags> stageMasks = {
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT };
		submitInfo.pWaitDstStageMask = stageMasks.data();
		
		const auto& computeQueue = queueManager->GetQueue(QueueManager::QUEUE_COMPUTE);
		vkQueueSubmit(computeQueue, 1, &submitInfo, VK_NULL_HANDLE);

		adaptiveGrid_->UpdateDebugTraversal(queueManager, bufferManager, imageManager_);
  }

	void VolumePass::OnLoadScene(Scene* scene)
	{
		adaptiveGrid_->OnLoadScene(scene);
		sceneLoaded_ = true;
	}

  void VolumePass::OnReloadShaders(VkDevice device, ShaderManager* shaderManager)
  {
    Pass::OnReloadShaders(device, shaderManager);

    CreateComputePipelines(device, shaderManager);
  }

  void VolumePass::Release(VkDevice device)
  {
    Pass::Release(device);
  }

  VkSemaphore* VolumePass::GetFinishedSemaphore()
  {
    return &computePipelines_[COMPUTE_RAYMARCHING].passFinishedSemaphore;
  }

  bool VolumePass::CreateComputePipelines(VkDevice device, ShaderManager* shaderManager)
  {
    std::vector<VkComputePipelineCreateInfo> createInfos;
    for(int subpass = SUBPASS_VOLUME_ADAPTIVE_GLOBAL; subpass <= SUBPASS_VOLUME_ADAPTIVE_RAYMARCHING; ++subpass)
    {
      VkComputePipelineCreateInfo createInfo = {};
      createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
      auto& shaderStage = shaderManager->GetShaderStageInfo(static_cast<SubPassType>(subpass));
      createInfo.stage = shaderStage[0];
      auto& pipelineLayout = bindingManager_->GetPipelineLayout(static_cast<SubPassType>(subpass));
      createInfo.layout = pipelineLayout;
      createInfos.push_back(createInfo);
    }

    std::vector<VkPipeline> pipelines(createInfos.size(), VK_NULL_HANDLE);
    const auto result = vkCreateComputePipelines(device, VK_NULL_HANDLE,
      static_cast<uint32_t>(createInfos.size()), createInfos.data(), nullptr, pipelines.data());
    if (result != VK_SUCCESS)
    {
      printf("Create volume compute passes failed, %d\n", result);
      return false;
    }

    for (size_t pass = 0; pass < pipelines.size(); ++pass)
    {
      computePipelines_[pass].pipeline = pipelines[pass];
    }

    return true;
  }

  bool VolumePass::RecordCommands(QueueManager* queueManager, BufferManager* bufferManager, ImageManager* imageManager, Surface* surface, VkCommandBuffer commandBuffer,
    ComputeSubpasses subpass, uint32_t currFrameIndex, int gridLevel)
  {
    SubPassType passType;
		AdaptiveGrid::Pass gridPass;
    switch (subpass)
    {
		case COMPUTE_GLOBAL:
			passType = SUBPASS_VOLUME_ADAPTIVE_GLOBAL;
			gridPass = AdaptiveGrid::GRID_PASS_GLOBAL;
			break;
    case COMPUTE_GROUND_FOG:
			passType = SUBPASS_VOLUME_ADAPTIVE_GROUND_FOG;
			gridPass = AdaptiveGrid::GRID_PASS_GROUND_FOG;
			break;
    case COMPUTE_PARTICLES:
			passType = SUBPASS_VOLUME_ADAPTIVE_PARTICLES;
			gridPass = AdaptiveGrid::GRID_PASS_PARTICLES;
			break;
		case COMPUTE_DEBUG_FILLING:
			passType = SUBPASS_VOLUME_ADAPTIVE_DEBUG_FILLING;
			gridPass = AdaptiveGrid::GRID_PASS_DEBUG_FILLING;
			break;
		case COMPUTE_MIPMAPPING:
			passType = SUBPASS_VOLUME_ADAPTIVE_MIPMAPPING;
			gridPass = AdaptiveGrid::GRID_PASS_MIPMAPPING;
			break;
		case COMPUTE_MIPMAPPING_MERGING:
			passType = SUBPASS_VOLUME_ADAPTIVE_MIPMAPPING_MERGING;
			gridPass = AdaptiveGrid::GRID_PASS_MIPMAPPING_MERGING;
			break;
		case COMPUTE_NEIGHBOR_UPDATE:
			passType = SUBPASS_VOLUME_ADAPTIVE_NEIGHBOR_UPDATE;
			gridPass = AdaptiveGrid::GRID_PASS_NEIGHBOR_UPDATE;
			break;
    case COMPUTE_RAYMARCHING:
      passType = SUBPASS_VOLUME_ADAPTIVE_RAYMARCHING;
			gridPass = AdaptiveGrid::GRID_PASS_RAYMARCHING;
      break;
		default:
			return true;
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
      computePipelines_[subpass].pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
      bindingManager_->GetPipelineLayout(passType), 0, 1,
      &bindingManager_->GetDescriptorSet(computePipelines_[subpass].shaderBinding, currFrameIndex), 0, 0);

		if (!sceneLoaded_)
		{
			if (subpass == COMPUTE_RAYMARCHING)
			{
				ImageManager::BarrierInfo shadowMapBarrier{};
				shadowMapBarrier.imageIndex = shadowMap_->GetImageIndex();
				shadowMapBarrier.type = ImageManager::BARRIER_READ_WRITE;
				auto barriers = imageManager->Barrier({ shadowMapBarrier });

				Wrapper::PipelineBarrierInfo pipelineBarrierInfo{};
				pipelineBarrierInfo.src = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
				pipelineBarrierInfo.dst = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
				pipelineBarrierInfo.AddImageBarriers(barriers);
				Wrapper::AddPipelineBarrier(commandBuffer, pipelineBarrierInfo);
			}
			return true;
		}

		switch (subpass)
    {
		case COMPUTE_PARTICLES:
		case COMPUTE_GLOBAL:
		case COMPUTE_DEBUG_FILLING:
    case COMPUTE_GROUND_FOG:
		case COMPUTE_MIPMAPPING:
		case COMPUTE_MIPMAPPING_MERGING:
		case COMPUTE_NEIGHBOR_UPDATE:
			adaptiveGrid_->Dispatch(queueManager, imageManager, bufferManager, commandBuffer, gridPass, currFrameIndex, gridLevel); 
			break;
		case COMPUTE_RAYMARCHING:
    {
      const auto& surfaceSize = surface->GetSurfaceSize();
      adaptiveGrid_->Raymarch(imageManager, commandBuffer, surfaceSize.width, surfaceSize.height, currFrameIndex);
			adaptiveGrid_->Dispatch(queueManager, imageManager, bufferManager, commandBuffer, gridPass, currFrameIndex, gridLevel);
    } break;
    }
		
    return true;
  }
}