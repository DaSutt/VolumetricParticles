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

#include "MeshPass.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm\gtx\euler_angles.hpp>

#include "..\passResources\ShaderBindingManager.h"
#include "..\passResources\RenderPassManager.h"
#include "..\passResources\FrameBufferManager.h"
#include "..\passResources\ShaderManager.h"

#include "..\resources\ImageManager.h"
#include "..\resources\BufferManager.h"
#include "..\resources\QueueManager.h"

#include "..\wrapper\Surface.h"
#include "..\wrapper\Synchronization.h"
#include "..\wrapper\QueryPool.h"

#include "..\MeshRenderable.h"
#include "..\ShadowMap.h"
#include "..\scene\UniformGrid.h"
#include "..\pipelineState\PipelineState.h"

#include "..\scene\RenderScene.h"

#include "..\..\scene\Scene.h"

#include "GuiPass.h"

namespace Renderer
{
  MeshPass::MeshPass(ShaderBindingManager* bindingManager, RenderPassManager* renderPassManager) :
    Pass::Pass(bindingManager, renderPassManager)
  {}

  void MeshPass::RequestResources(ImageManager* imageManager, BufferManager* bufferManager, int frameCount)
  {
    auto pass = SUBPASS_MESH;
    {
      BufferManager::BufferInfo perFrameCBInfo = {};
      perFrameCBInfo.typeBits = BufferManager::BUFFER_CONSTANT_BIT | BufferManager::BUFFER_SCENE_BIT;
      perFrameCBInfo.pool = BufferManager::MEMORY_CONSTANT;
      perFrameCBInfo.size = sizeof(PerFrameData);
      perFrameCBInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
      perFrameCBInfo.data = &frameData_;
      perFrameCBInfo.bufferingCount = frameCount;
      perFrameBuffer_ = bufferManager->Ref_RequestBuffer(perFrameCBInfo);

      ShaderBindingManager::BindingInfo bindingInfo = {};
      bindingInfo.types = {
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
      };
      bindingInfo.stages = {
        VK_SHADER_STAGE_VERTEX_BIT,
        VK_SHADER_STAGE_VERTEX_BIT,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_SHADER_STAGE_FRAGMENT_BIT
      };
      bindingInfo.refactoring_ =
      {
        true, true, true, true
      };

      const auto& meshData = renderScene_->GetMeshData();
      bindingInfo.resourceIndex = {
        meshData.buffers_[MeshData::BUFFER_WORLD_VIEW_PROJ],
        meshData.buffers_[MeshData::BUFFER_WORLD_INV_TRANSPOSE],
        perFrameBuffer_,
        shadowMap_->GetImageIndex()
      };
      bindingInfo.pass = pass;
      bindingInfo.setCount = frameCount;
      graphicPipelines_.push_back(bindingManager_->RequestShaderBinding(bindingInfo));

      ShaderBindingManager::PushConstantInfo pushConstantInfo = {};
      VkPushConstantRange objectIds = {};
      objectIds.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
      objectIds.offset = 0;
      objectIds.size = sizeof(int);
      pushConstantInfo.pushConstantRanges.push_back(objectIds);
      pushConstantInfo.pass = pass;
      pushConstantIndex_ = bindingManager_->RequestPushConstants(pushConstantInfo)[0];

      ShaderBindingManager::SceneBindingInfo sceneBindingInfo = {};
      sceneBindingInfo.pass = pass;
      sceneBindingInfo.stages = { VK_SHADER_STAGE_FRAGMENT_BIT };
      sceneBindingInfo.types = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER };
      perMaterialSceneBinding_ = bindingManager_->RequestSceneShaderBinding(sceneBindingInfo);
    }

    pass = SUBPASS_MESH_DEPTH_ONLY;
    {
      BufferManager::BufferInfo perFrameCBInfo = {};
      perFrameCBInfo.typeBits = BufferManager::BUFFER_CONSTANT_BIT | BufferManager::BUFFER_SCENE_BIT;
      perFrameCBInfo.pool = BufferManager::MEMORY_CONSTANT;
      perFrameCBInfo.size = sizeof(glm::mat4);
      perFrameCBInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
      perFrameCBInfo.data = &worldViewProjGrid_;
      perFrameCBInfo.bufferingCount = frameCount;
      perFrameGridBuffer_ = bufferManager->Ref_RequestBuffer(perFrameCBInfo);

      ShaderBindingManager::BindingInfo bindingInfo = {};
      bindingInfo.pass = SUBPASS_MESH_DEPTH_ONLY;
      bindingInfo.resourceIndex = { perFrameGridBuffer_ };
      bindingInfo.stages = { VK_SHADER_STAGE_VERTEX_BIT };
      bindingInfo.types = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER };
      bindingInfo.refactoring_ = { true };
      bindingInfo.setCount = frameCount;
      graphicPipelines_.push_back(bindingManager_->RequestShaderBinding(bindingInfo));
    }
  }

  void MeshPass::SetShadowMap(ShadowMap* shadowMap)
  {
    shadowMap_ = shadowMap;
  }

  void MeshPass::SetImageIndices(int offscreenImage, int depthImage)
  {
    offscreenImageIndex_ = offscreenImage;
    depthImageIndex_ = depthImage;
  }

  void MeshPass::SetRenderScene(RenderScene* renderScene)
  {
    renderScene_ = renderScene;
  }

  bool MeshPass::Create(VkDevice device, ShaderManager* shaderManager, FrameBufferManager* frameBufferManager,
    Surface* surface, ImageManager* imageManager)
  {
    if (!CreateFrameBuffers(device, frameBufferManager, surface)) { return false; }
    if (!CreateGraphicPipeline(device, shaderManager)) { return false; }
    if (!CreateCommandBuffers(device, QueueManager::QUEUE_GRAPHICS, surface->GetImageCount())) { return false; }
    if (!Wrapper::CreateVulkanSemaphore(device, &graphicPipelines_[GRAPHIC_MESH].passFinishedSemaphore)) { return false; }

    return true;
  }

  void MeshPass::UpdateBufferData(Scene* scene, RenderScene* renderScene)
  {
    const auto& lightingState = GuiPass::GetLightingState();
		frameData_.dirLight.lightVector = glm::vec4(lightingState.lightVector, 0.0);
		
		frameData_.dirLight.irradiance = { lightingState.irradiance[0], lightingState.irradiance[1], lightingState.irradiance[2] };
    frameData_.cameraPos_ = scene->GetCamera().GetPosition();

    frameData_.shadowCascades_ = renderScene_->GetShadowMap()->GetShadowMatrices();
  }

  void MeshPass::Render(Surface* surface, FrameBufferManager* frameBufferManager,
    QueueManager* queueManager, BufferManager* bufferManager,
    std::vector<VkImageMemoryBarrier>& memoryBarrier, VkSemaphore* startSemaphore, int frameIndex)
  {
		auto& queryPool = Wrapper::QueryPool::GetInstance();
    const auto pass = SUBPASS_MESH;
    {
      VkCommandBufferBeginInfo beginInfo = {};
      beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

      vkBeginCommandBuffer(commandBuffers_[frameIndex], &beginInfo);
			queryPool.TimestampStart(commandBuffers_[frameIndex], Wrapper::TIMESTAMP_MESH, frameIndex);

      //Begin rendering by clearing depth and color
      VkRenderPassBeginInfo renderPassInfo = {};
      renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
      renderPassInfo.renderPass = renderPassManager_->GetRenderPass(GRAPHIC_PASS_MESH);
      renderPassInfo.framebuffer = frameBufferManager->GetFrameBuffer(frameBufferIndices_[0]);
      renderPassInfo.renderArea.offset = { 0,0 };
      renderPassInfo.renderArea.extent = surface->GetSurfaceSize();

      VkClearValue clearValues[] = {
        { 1.0f, 1.0f, 1.0f, 0.0f },
        { 1.0f, 0 }
      };
      renderPassInfo.clearValueCount = 2;
      renderPassInfo.pClearValues = clearValues;

      vkCmdBeginRenderPass(commandBuffers_[frameIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
      vkCmdBindPipeline(commandBuffers_[frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicPipelines_[GRAPHIC_MESH].pipeline);
      vkCmdBindDescriptorSets(commandBuffers_[frameIndex],
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        bindingManager_->GetPipelineLayout(pass), 0, 1,
        &bindingManager_->GetDescriptorSet(graphicPipelines_[GRAPHIC_MESH].shaderBinding, frameIndex), 0, nullptr);
      surface->BindViewportScissor(commandBuffers_[frameIndex]);
    }

    {
      const auto& currMeshIndices = renderScene_->GetMeshIndices(SUBPASS_MESH);

      if (!currMeshIndices.empty())
      {
        renderScene_->BindMeshData(bufferManager, commandBuffers_[frameIndex]);

        const auto& meshData = renderScene_->GetMeshData();

        const auto pipelineLayout = bindingManager_->GetPipelineLayout(SUBPASS_MESH);
        for (const auto index : currMeshIndices)
        {
          vkCmdPushConstants(commandBuffers_[frameIndex], pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(int), &index);
          renderScene_->BindMeshMaterial(commandBuffers_[frameIndex], bindingManager_, index);
          vkCmdDrawIndexed(commandBuffers_[frameIndex],
            meshData.indexCounts_[index], 1, meshData.indexOffsets_[index],
            meshData.vertexOffsets_[index], 0);
        }
      }
    }

    {
      vkCmdEndRenderPass(commandBuffers_[frameIndex]);
			queryPool.TimestampEnd(commandBuffers_[frameIndex], Wrapper::TIMESTAMP_MESH, frameIndex);

			vkEndCommandBuffer(commandBuffers_[frameIndex]);

      const VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      VkSubmitInfo submitInfo = {};
      submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
      submitInfo.waitSemaphoreCount = 1;
      submitInfo.pWaitSemaphores = startSemaphore;
      submitInfo.pWaitDstStageMask = &waitDstStageMask;
      submitInfo.commandBufferCount = 1;
      submitInfo.pCommandBuffers = &commandBuffers_[frameIndex];
      submitInfo.signalSemaphoreCount = 1;
      submitInfo.pSignalSemaphores = &graphicPipelines_[GRAPHIC_MESH].passFinishedSemaphore;
      vkQueueSubmit(queueManager->GetQueue(QueueManager::QUEUE_GRAPHICS), 1, &submitInfo, VK_NULL_HANDLE);
    }
  }

  void MeshPass::OnReloadShaders(VkDevice device, ShaderManager* shaderManager)
  {
    Pass::OnReloadShaders(device, shaderManager);

    CreateGraphicPipeline(device, shaderManager);
  }

  VkSemaphore* MeshPass::GetFinishedSemaphore()
  {
    return &graphicPipelines_[GRAPHIC_MESH].passFinishedSemaphore;
  }

  bool MeshPass::CreateFrameBuffers(VkDevice device, FrameBufferManager* frameBufferManager, Surface* surface)
  {
    const auto& surfaceSize = surface->GetSurfaceSize();

    FrameBufferManager::Info frameBufferInfo;
    frameBufferInfo.imageAttachements =
    {
      { offscreenImageIndex_, FrameBufferManager::IMAGE_SOURCE_MANAGER },
      { depthImageIndex_, FrameBufferManager::IMAGE_SOURCE_MANAGER}
    };
    frameBufferInfo.pass = GRAPHIC_PASS_MESH;
    frameBufferInfo.width = surfaceSize.width;
    frameBufferInfo.height = surfaceSize.height;
    frameBufferInfo.resize = true;

    auto index = frameBufferManager->RequestFrameBuffer(device, frameBufferInfo);
    if (index == -1)
    {
      return false;
    }
    frameBufferIndices_.push_back(index);

    return true;
  }

  bool MeshPass::CreateGraphicPipeline(VkDevice device, ShaderManager* shaderManager)
  {
    {
      VkGraphicsPipelineCreateInfo pipelineInfo = {};
      pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

      const auto shaderStageInfo = shaderManager->GetShaderStageInfo(Renderer::SUBPASS_MESH);
      pipelineInfo.stageCount = static_cast<uint32_t>(shaderStageInfo.size());
      pipelineInfo.pStages = shaderStageInfo.data();

      const auto vertexInputState = MeshRenderable::GetVertexInputState();
      pipelineInfo.pVertexInputState = &vertexInputState;

      pipelineInfo.pInputAssemblyState = &g_inputAssemblyStates[ASSEMBLY_TRIANGLE_LIST];
      pipelineInfo.pRasterizationState = &g_rasterizerState[RASTERIZER_SOLID];
      pipelineInfo.pMultisampleState = &g_mulitsampleStates[MULTISAMPLE_DISABLED];
      pipelineInfo.pDepthStencilState = &g_depthStencilStates[DEPTH_WRITE];
      pipelineInfo.pColorBlendState = BlendState::GetCreateInfo(BlendState::BLEND_DISABLED);
      pipelineInfo.pViewportState = &ViewPortState::GetState(ViewPortState::VIEWPORT_STATE_UNDEFINED);
      pipelineInfo.pDynamicState = DynamicState::GetCreateInfo(DynamicState::DYNAMIC_VIEWPORT_SCISSOR);

      pipelineInfo.layout = bindingManager_->GetPipelineLayout(SUBPASS_MESH);
      pipelineInfo.renderPass = renderPassManager_->GetRenderPass(GRAPHIC_PASS_MESH);
      pipelineInfo.subpass = 0;
      pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
      pipelineInfo.basePipelineIndex = 0;

      VkPipeline pipeline = VK_NULL_HANDLE;
      const auto result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE,
        1, &pipelineInfo,
        nullptr, &pipeline);
      if (result != VK_SUCCESS)
      {
        printf("Failed creating mesh pass pipeline, %d\n", result);
        return false;
      }

      graphicPipelines_[GRAPHIC_MESH].pipeline = pipeline;
    }

    return true;
  }
}