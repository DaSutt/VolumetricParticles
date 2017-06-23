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

#include "ShadowMapPass.h"

#include "..\passResources\ShaderBindingManager.h"
#include "..\passResources\RenderPassManager.h"
#include "..\passResources\FrameBufferManager.h"
#include "..\passResources\ShaderManager.h"

#include "..\resources\ImageManager.h"
#include "..\resources\BufferManager.h"
#include "..\resources\QueueManager.h"

#include "..\wrapper\Surface.h"
#include "..\wrapper\Synchronization.h"

#include "..\MeshRenderable.h"
#include "..\pipelineState\PipelineState.h"

#include "..\ShadowMap.h"
#include "..\..\scene\Scene.h"
#include "GuiPass.h"
#include "..\scene\RenderScene.h"

#include "..\wrapper\QueryPool.h"

namespace Renderer
{
  ShadowMapPass::ShadowMapPass(ShaderBindingManager* bindingManager, RenderPassManager* renderPassManager) :
    Pass::Pass(bindingManager, renderPassManager)
  {}

  void ShadowMapPass::SetShadowMap(ShadowMap* shadowMap)
  {
    shadowMap_ = shadowMap;
  }

  void ShadowMapPass::SetRenderScene(RenderScene* renderScene)
  {
    renderScene_ = renderScene;
  }

  void ShadowMapPass::RequestResources(ImageManager* imageManager, BufferManager* bufferManager, int frameCount)
  {
    auto pass = SUBPASS_SHADOW_MAP;

    const auto& meshData = renderScene_->GetMeshData();

    ShaderBindingManager::BindingInfo bindingInfo = {};
    bindingInfo.types = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER };
    bindingInfo.stages = { VK_SHADER_STAGE_VERTEX_BIT };
    bindingInfo.resourceIndex = { meshData.buffers_[MeshData::BUFFER_WORLD_VIEW_PROJ_LIGHT] };
    bindingInfo.pass = pass;
    bindingInfo.refactoring_ = { true };
    bindingInfo.setCount = frameCount;
    graphicPipelines_.push_back(bindingManager_->RequestShaderBinding(bindingInfo));

    ShaderBindingManager::PushConstantInfo pushConstantInfo = {};
    VkPushConstantRange objectIds = {};
    objectIds.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    objectIds.offset = 0;
    objectIds.size = sizeof(int);
    pushConstantInfo.pushConstantRanges.push_back(objectIds);
    pushConstantInfo.pass = pass;
    perObjectPushConstantIndex_ = bindingManager_->RequestPushConstants(pushConstantInfo)[0];
  }

  bool ShadowMapPass::Create(VkDevice device, ShaderManager* shaderManager, FrameBufferManager* frameBufferManager,
    Surface* surface, ImageManager* imageManager)
  {
    if (!CreateFrameBuffers(device, frameBufferManager, surface)) { return false; }
    if (!CreateGraphicPipeline(device, shaderManager)) { return false; }
    if (!CreateCommandBuffers(device, QueueManager::QUEUE_GRAPHICS, surface->GetImageCount())) { return false; }
    if (!Wrapper::CreateVulkanSemaphore(device, &graphicPipelines_[GRAPHICS_SHADOW_DIR].passFinishedSemaphore)) { return false; }

    return true;
  }

  void ShadowMapPass::UpdateBufferData(Scene* scene)
  {

  }

  void ShadowMapPass::Render(Surface* surface, FrameBufferManager* frameBufferManager,
    QueueManager* queueManager, BufferManager* bufferManager,
    std::vector<VkImageMemoryBarrier>& memoryBarrier, VkSemaphore* startSemaphore, int frameIndex)
  {
    const auto pass = SUBPASS_SHADOW_MAP;

    {
      VkCommandBufferBeginInfo beginInfo = {};
      beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

      vkBeginCommandBuffer(commandBuffers_[frameIndex], &beginInfo);

			auto& queryPool = Wrapper::QueryPool::GetInstance();
			queryPool.Reset(commandBuffers_[frameIndex], frameIndex);
			queryPool.TimestampStart(commandBuffers_[frameIndex], 
				Wrapper::TIMESTAMP_SHADOW_MAP, frameIndex);

      shadowMap_->BindViewportScissor(commandBuffers_[frameIndex]);

      const auto& meshData = renderScene_->GetMeshData();
      const auto& currMeshIndices = renderScene_->GetMeshIndices(SUBPASS_SHADOW_MAP);

      renderScene_->BindMeshData(bufferManager, commandBuffers_[frameIndex]);

      int offset = 0;
      for (size_t i = 0; i < shadowMap_->GetCascadeCount(); ++i)
      {
        {
          //Begin rendering by clearing depth
          VkRenderPassBeginInfo renderPassInfo = {};
          renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
          renderPassInfo.renderPass = renderPassManager_->GetRenderPass(GRAPHIC_PASS_SHADOW_MAP);
          renderPassInfo.framebuffer = frameBufferManager->GetFrameBuffer(frameBufferIndices_[i]);
          renderPassInfo.renderArea.offset = { 0,0 };
          renderPassInfo.renderArea.extent = shadowMap_->GetSize();

          VkClearValue clearValue = { 1.0f, 0 };
          renderPassInfo.clearValueCount = 1;
          renderPassInfo.pClearValues = &clearValue;

          vkCmdBeginRenderPass(commandBuffers_[frameIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
          vkCmdBindPipeline(commandBuffers_[frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicPipelines_[GRAPHICS_SHADOW_DIR].pipeline);
          vkCmdBindDescriptorSets(commandBuffers_[frameIndex],
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            bindingManager_->GetPipelineLayout(pass), 0, 1,
            &bindingManager_->GetDescriptorSet(graphicPipelines_[GRAPHICS_SHADOW_DIR].shaderBinding, frameIndex), 0, nullptr);
        }

        if (!currMeshIndices.empty() && meshData.vertexCount_ > 0)
        {
          const auto& pipelineLayout = bindingManager_->GetPipelineLayout(pass);
          for (const auto i : currMeshIndices)
          {
            int index = i + offset;
            vkCmdPushConstants(commandBuffers_[frameIndex], pipelineLayout,
              VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(int), &index);
            vkCmdDrawIndexed(commandBuffers_[frameIndex],
              meshData.indexCounts_[i], 1, meshData.indexOffsets_[i],
              meshData.vertexOffsets_[i], 0);
          }
          offset += static_cast<int>(currMeshIndices.size());
        }

        vkCmdEndRenderPass(commandBuffers_[frameIndex]);
      }

			queryPool.TimestampEnd(commandBuffers_[frameIndex],
				Wrapper::TIMESTAMP_SHADOW_MAP, frameIndex);

      vkEndCommandBuffer(commandBuffers_[frameIndex]);
      {
				VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers_[frameIndex];
				
				const VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				submitInfo.waitSemaphoreCount = 1;
				submitInfo.pWaitSemaphores = startSemaphore;
				submitInfo.pWaitDstStageMask = &waitDstStageMask;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &graphicPipelines_[GRAPHICS_SHADOW_DIR].passFinishedSemaphore;
        vkQueueSubmit(queueManager->GetQueue(QueueManager::QUEUE_GRAPHICS), 1, &submitInfo, VK_NULL_HANDLE);
      }
    }
  }

  void ShadowMapPass::OnReloadShaders(VkDevice device, ShaderManager* shaderManager)
  {
    Pass::OnReloadShaders(device, shaderManager);

    CreateGraphicPipeline(device, shaderManager);
  }

  VkSemaphore* ShadowMapPass::GetFinishedSemaphore()
  {
    return &graphicPipelines_[GRAPHICS_SHADOW_DIR].passFinishedSemaphore;
  }

  bool ShadowMapPass::CreateFrameBuffers(VkDevice device, FrameBufferManager* frameBufferManager, Surface* surface)
  {
    FrameBufferManager::Info frameBufferInfo;
    frameBufferInfo.pass = GRAPHIC_PASS_SHADOW_MAP;
    const auto& size = shadowMap_->GetSize();
    frameBufferInfo.width = size.width;
    frameBufferInfo.height = size.height;
    frameBufferInfo.resize = false;
    frameBufferInfo.refactoring = true;
    frameBufferInfo.layers = 1;

    for (int i = 0; i < shadowMap_->GetCascadeCount(); ++i)
    {
      frameBufferInfo.imageAttachements =
      {
        { shadowMap_->GetImageIndex(), FrameBufferManager::IMAGE_SOURCE_MANAGER, i, true }
      };

      const auto index = frameBufferManager->RequestFrameBuffer(device, frameBufferInfo);
      if (index == -1)
      {
        return false;
      }
      frameBufferIndices_.push_back(index);
    }

    return true;
  }

  bool ShadowMapPass::CreateGraphicPipeline(VkDevice device, ShaderManager* shaderManager)
  {
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    const auto shaderStageInfo = shaderManager->GetShaderStageInfo(Renderer::SUBPASS_SHADOW_MAP);
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStageInfo.size());
    pipelineInfo.pStages = shaderStageInfo.data();

    const auto vertexInputState = MeshRenderable::GetVertexInputState();
    pipelineInfo.pVertexInputState = &vertexInputState;

    pipelineInfo.pInputAssemblyState = &g_inputAssemblyStates[ASSEMBLY_TRIANGLE_LIST];
    pipelineInfo.pRasterizationState = &g_rasterizerState[RASTERIZER_SOLID];
    pipelineInfo.pMultisampleState = &g_mulitsampleStates[MULTISAMPLE_DISABLED];
    pipelineInfo.pDepthStencilState = &g_depthStencilStates[DEPTH_WRITE];
    pipelineInfo.pViewportState = &ViewPortState::GetState(ViewPortState::VIEWPORT_STATE_UNDEFINED);
    pipelineInfo.pDynamicState = DynamicState::GetCreateInfo(DynamicState::DYNAMIC_VIEWPORT_SCISSOR);

    pipelineInfo.layout = bindingManager_->GetPipelineLayout(SUBPASS_SHADOW_MAP);
    pipelineInfo.renderPass = renderPassManager_->GetRenderPass(GRAPHIC_PASS_SHADOW_MAP);
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

    graphicPipelines_[GRAPHICS_SHADOW_DIR].pipeline = pipeline;

    return true;
  }
}