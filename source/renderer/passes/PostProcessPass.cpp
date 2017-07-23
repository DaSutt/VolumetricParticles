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

#include "PostProcessPass.h"

#include "Passes.h"
#include "GuiPass.h"

#include "..\passResources\ShaderBindingManager.h"
#include "..\passResources\RenderPassManager.h"
#include "..\passResources\ShaderManager.h"
#include "..\passResources\FrameBufferManager.h"

#include "..\resources\QueueManager.h"
#include "..\resources\BufferManager.h"
#include "..\resources\ImageManager.h"

#include "..\pipelineState\PipelineState.h"

#include "..\wrapper\Surface.h"
#include "..\wrapper\Synchronization.h"
#include "..\wrapper\Barrier.h"
#include "..\wrapper\QueryPool.h"

#include "..\scene\RenderScene.h"
#include "..\ShadowMap.h"
#include "..\..\scene\Scene.h"

#include <random>
#include <functional>

namespace Renderer
{
  PostProcessPass::PostProcessPass(ShaderBindingManager* bindingManager, RenderPassManager* renderPassManager) :
    Pass::Pass(bindingManager, renderPassManager)
  {}

  void PostProcessPass::SetImageIndices(int meshRenderingIndex, int raymarchingIndex, int depthImageIndex, int noiseImage)
  {
    meshRenderingImageIndex_ = meshRenderingIndex;
    raymarchingImageIndex_ = raymarchingIndex;
    depthImageIndex_ = depthImageIndex;
    noiseImageIndex_ = noiseImage;
  }

  void PostProcessPass::RequestResources(ImageManager* imageManager, BufferManager* bufferManager, int frameCount)
  {
    auto pass = SUBPASS_POSTPROCESS;
    {
      BufferManager::BufferInfo frameBufferInfo = {};
      frameBufferInfo.typeBits = BufferManager::BUFFER_CONSTANT_BIT | BufferManager::BUFFER_SCENE_BIT;
      frameBufferInfo.pool = BufferManager::MEMORY_CONSTANT;
      frameBufferInfo.size = sizeof(FrameData);
      frameBufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
      frameBufferInfo.data = &frameData_;
      frameBufferInfo.bufferingCount = frameCount;
      frameBuffer_ = bufferManager->Ref_RequestBuffer(frameBufferInfo);

      ShaderBindingManager::BindingInfo bindingInfo = {};
      bindingInfo.types = {
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
      };
      bindingInfo.stages = {
        VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_SHADER_STAGE_FRAGMENT_BIT
      };
      bindingInfo.resourceIndex = {
        meshRenderingImageIndex_,
        raymarchingImageIndex_,
        noiseImageIndex_,
        frameBuffer_
      };
      bindingInfo.refactoring_ = {
        false,
        false,
        true,
        true
      };
      bindingInfo.pass = pass;
      bindingInfo.setCount = frameCount;
      graphicPipelines_.push_back(bindingManager_->RequestShaderBinding(bindingInfo));
    }
    pass = SUBPASS_POSTPROCESS_DEBUG;
    {
      VkPushConstantRange pushConstantRange = {};
      pushConstantRange.offset = 0;
      pushConstantRange.size = sizeof(DebugData);
      pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

      ShaderBindingManager::PushConstantInfo pushConstantInfo = {};
      pushConstantInfo.pass = pass;
      pushConstantInfo.pushConstantRanges.push_back(pushConstantRange);
      debugPushConstantIndex_ = bindingManager_->RequestPushConstants(pushConstantInfo)[0];

      ShaderBindingManager::BindingInfo bindingInfo = {};
      bindingInfo.pass = pass;
      //TODO: check if this is needed
      bindingInfo.setCount = frameCount;
      graphicPipelines_.push_back(bindingManager_->RequestShaderBinding(bindingInfo));
    }
  }

  bool PostProcessPass::Create(VkDevice device, ShaderManager* shaderManager,
    FrameBufferManager* frameBufferManager, Surface* surface, ImageManager* imageManager)
  {
    if (!CreateFrameBuffers(device, frameBufferManager, surface)) { return false; }
    if (!CreateGraphicPipeline(device, shaderManager)) { return false; }
    if (!CreateCommandBuffers(device, QueueManager::QUEUE_GRAPHICS, surface->GetImageCount())) { return false; }
    if (!Wrapper::CreateVulkanSemaphore(device, &graphicPipelines_[GRAPHICS_BLENDING].passFinishedSemaphore)) { return false; }

		imageManager_ = imageManager;

    return true;
  }

  bool PostProcessPass::CreateFrameBuffers(VkDevice device, FrameBufferManager* frameBufferManager, Surface* surface)
  {
    int backBufferCount = surface->GetImageCount();
    const auto& surfaceSize = surface->GetSurfaceSize();

    frameBufferIndices_.resize(backBufferCount * GRAPHICS_MAX);

    for (int i = 0; i < backBufferCount; ++i)
    {
      FrameBufferManager::Info frameBufferInfo;
      frameBufferInfo.imageAttachements =
      {
        {i, FrameBufferManager::IMAGE_SOURCE_SWAPCHAIN}
      };
      frameBufferInfo.pass = GRAPHIC_PASS_POSTPROCESS;
      frameBufferInfo.width = surfaceSize.width;
      frameBufferInfo.height = surfaceSize.height;
      frameBufferInfo.resize = true;

      auto index = frameBufferManager->RequestFrameBuffer(device, frameBufferInfo);
      if (index == -1)
      {
        return false;
      }
      frameBufferIndices_[i] = index;

      frameBufferInfo.imageAttachements.push_back(
        {depthImageIndex_, FrameBufferManager::IMAGE_SOURCE_MANAGER}
      );
      frameBufferInfo.pass = GRAPHIC_PASS_POSTPROCESS_DEBUG;
      index = frameBufferManager->RequestFrameBuffer(device, frameBufferInfo);
      if (index == -1)
      {
        return false;
      }
      frameBufferIndices_[backBufferCount + i] = index;
    }
    return true;
  }

  bool PostProcessPass::CreateGraphicPipeline(VkDevice device, ShaderManager* shaderManager)
  {
    auto globalSubpass = SUBPASS_POSTPROCESS;
    auto graphicSubpass = GRAPHICS_BLENDING;
    {
      VkGraphicsPipelineCreateInfo pipelineInfo = {};
      pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

      const auto shaderStageInfo = shaderManager->GetShaderStageInfo(globalSubpass);
      pipelineInfo.stageCount = static_cast<uint32_t>(shaderStageInfo.size());
      pipelineInfo.pStages = shaderStageInfo.data();

      VkPipelineVertexInputStateCreateInfo vertexInputState = {};
      vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
      vertexInputState.vertexAttributeDescriptionCount = 0;
      vertexInputState.vertexBindingDescriptionCount = 0;
      pipelineInfo.pVertexInputState = &vertexInputState;

      pipelineInfo.pInputAssemblyState = &g_inputAssemblyStates[ASSEMBLY_TRIANGLE_LIST];
      pipelineInfo.pRasterizationState = &g_rasterizerState[RASTERIZER_SOLID];
      pipelineInfo.pMultisampleState = &g_mulitsampleStates[MULTISAMPLE_DISABLED];
      pipelineInfo.pDepthStencilState = &g_depthStencilStates[DEPTH_WRITE];
      pipelineInfo.pColorBlendState = BlendState::GetCreateInfo(BlendState::BLEND_ALPHA);
      pipelineInfo.pViewportState = &ViewPortState::GetState(ViewPortState::VIEWPORT_STATE_UNDEFINED);
      pipelineInfo.pDynamicState = DynamicState::GetCreateInfo(DynamicState::DYNAMIC_VIEWPORT_SCISSOR);

      pipelineInfo.layout = bindingManager_->GetPipelineLayout(globalSubpass);
      pipelineInfo.renderPass = renderPassManager_->GetRenderPass(GetGraphicPass(globalSubpass));
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

      graphicPipelines_[graphicSubpass].pipeline = pipeline;
    }

    globalSubpass = SUBPASS_POSTPROCESS_DEBUG;
    graphicSubpass = GRAPHICS_DEBUG;
    {
      VkGraphicsPipelineCreateInfo pipelineInfo = {};
      pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

      const auto shaderStageInfo = shaderManager->GetShaderStageInfo(globalSubpass);
      pipelineInfo.stageCount = static_cast<uint32_t>(shaderStageInfo.size());
      pipelineInfo.pStages = shaderStageInfo.data();

      VkPipelineVertexInputStateCreateInfo vertexInputState = {};
      vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
      vertexInputState.vertexAttributeDescriptionCount = 0;
      vertexInputState.vertexBindingDescriptionCount = 0;
      pipelineInfo.pVertexInputState = &vertexInputState;

      pipelineInfo.pInputAssemblyState = &g_inputAssemblyStates[ASSEMBLY_LINE_LIST];
      pipelineInfo.pRasterizationState = &g_rasterizerState[RASTERIZER_LINE];
      pipelineInfo.pMultisampleState = &g_mulitsampleStates[MULTISAMPLE_DISABLED];
      pipelineInfo.pDepthStencilState = &g_depthStencilStates[DEPTH_READ];
      pipelineInfo.pColorBlendState = BlendState::GetCreateInfo(BlendState::BLEND_DISABLED);
      pipelineInfo.pViewportState = &ViewPortState::GetState(ViewPortState::VIEWPORT_STATE_UNDEFINED);
      pipelineInfo.pDynamicState = DynamicState::GetCreateInfo(DynamicState::DYNAMIC_VIEWPORT_SCISSOR);

      pipelineInfo.layout = bindingManager_->GetPipelineLayout(globalSubpass);
      pipelineInfo.renderPass = renderPassManager_->GetRenderPass(GetGraphicPass(globalSubpass));
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

      graphicPipelines_[graphicSubpass].pipeline = pipeline;
    }

    return true;
  }

  void PostProcessPass::UpdateBufferData(Scene* scene, Surface* surface, RenderScene* renderScene)
  {
    if (GuiPass::GetDebugVisState().nodeRendering)
    {
      debugBoundingBoxes_.clear();
      const auto viewProj = scene->GetCamera().GetProj() * scene->GetCamera().GetView();

      //add mesh bounding boxes
      const auto& boundingBoxes = scene->GetBoundingBoxes();
      auto color = glm::vec4(1, 0, 0, 1);
      for (auto& bb : boundingBoxes)
      {
        //debugBoundingBoxes_.push_back({ viewProj * WorldMatrix(bb), color });
      }
      //debugBoundingBoxes_.push_back({ viewProj * WorldMatrix(scene->GetSceneBoundingBox()), glm::vec4(1,1,0,1) });

      //add grid bounding boxes
      const auto& gridBoundingBoxes = renderScene->GetAdaptiveGrid()->GetDebugBoundingBoxes();
      color = glm::vec4(0, 1, 0, 1);
      for (auto& bb : gridBoundingBoxes)
      {
        debugBoundingBoxes_.push_back({ viewProj * bb.world, bb.color});
      }
      //const auto& shadowMapBoundingBoxes = renderScene->GetShadowMap()->debugBoundingBoxes_;
      //for (size_t i = 0; i < shadowMapBoundingBoxes.size(); ++i)
      //{
      //  debugBoundingBoxes_.push_back({ viewProj * shadowMapBoundingBoxes[i],
      //    glm::vec4(i / static_cast<float>(shadowMapBoundingBoxes.size()), 0 , 1, 1) });
      //}

    static std::default_random_engine generator;
    static std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    static auto RandPos = std::bind(distribution, generator);
    frameData_.randomness = { RandPos(), RandPos(), RandPos(), RandPos() };

    const auto& surfaceSize = surface->GetSurfaceSize();
    frameData_.screenSize = { surfaceSize.width, surfaceSize.height };
    }

    static std::default_random_engine generator;
    static std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    static auto RandPos = std::bind(distribution, generator);
    frameData_.randomness = { RandPos(), RandPos(), RandPos(), RandPos() };

    const auto& surfaceSize = surface->GetSurfaceSize();
    frameData_.screenSize = { surfaceSize.width, surfaceSize.height };
  }

  void PostProcessPass::Render(Surface* surface, FrameBufferManager* frameBufferManager,
    QueueManager* queueManager, BufferManager* bufferManager,
    std::vector<VkImageMemoryBarrier>& memoryBarrier, VkSemaphore* startSemaphore, int frameIndex)
  {
    auto pass = SUBPASS_POSTPROCESS;

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(commandBuffers_[frameIndex], &beginInfo);

		auto& queryPool = Wrapper::QueryPool::GetInstance();
		queryPool.TimestampStart(commandBuffers_[frameIndex], Wrapper::TIMESTAMP_POSTPROCESS, frameIndex);

    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPassManager_->GetRenderPass(GetGraphicPass(pass));
    renderPassInfo.framebuffer = frameBufferManager->GetFrameBuffer(frameBufferIndices_[frameIndex]);
    renderPassInfo.renderArea.offset = { 0,0 };
    renderPassInfo.renderArea.extent = surface->GetSurfaceSize();
    std::vector<VkClearValue> clearValues = {
      { 0.0f, 0.0f, 0.0f, 1.0f }
    };
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffers_[frameIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffers_[frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicPipelines_[GRAPHICS_BLENDING].pipeline);
    vkCmdBindDescriptorSets(commandBuffers_[frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS,
      bindingManager_->GetPipelineLayout(pass), 0, 1,
      &bindingManager_->GetDescriptorSet(graphicPipelines_[GRAPHICS_BLENDING].shaderBinding, frameIndex), 0, 0);

    surface->BindViewportScissor(commandBuffers_[frameIndex]);

    vkCmdDraw(commandBuffers_[frameIndex], 3, 1, 0, 0);

    vkCmdEndRenderPass(commandBuffers_[frameIndex]);

    const auto state = GuiPass::GetDebugVisState();
    if (state.nodeRendering)
    {
      pass = SUBPASS_POSTPROCESS_DEBUG;
      auto graphicsSubpass = GRAPHICS_DEBUG;

      VkRenderPassBeginInfo renderPassInfo = {};
      renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
      renderPassInfo.renderPass = renderPassManager_->GetRenderPass(GetGraphicPass(pass));
      const auto offset = frameBufferIndices_.size() / GRAPHICS_MAX;
      renderPassInfo.framebuffer = frameBufferManager->GetFrameBuffer(frameBufferIndices_[frameIndex + offset]);
      renderPassInfo.renderArea.offset = { 0,0 };
      renderPassInfo.renderArea.extent = surface->GetSurfaceSize();

      vkCmdBeginRenderPass(commandBuffers_[frameIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
      vkCmdBindPipeline(commandBuffers_[frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS,
        graphicPipelines_[graphicsSubpass].pipeline);

      const auto pipelineLayout = bindingManager_->GetPipelineLayout(pass);
      vkCmdBindDescriptorSets(commandBuffers_[frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout, 0, 1,
        &bindingManager_->GetDescriptorSet(graphicPipelines_[graphicsSubpass].shaderBinding, frameIndex), 0, 0);

      for (auto& mat : debugBoundingBoxes_)
      {
        vkCmdPushConstants(commandBuffers_[frameIndex], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
          0, sizeof(DebugData), &mat);

        vkCmdDraw(commandBuffers_[frameIndex], 24, 1, 0, 0);
      }

      vkCmdEndRenderPass(commandBuffers_[frameIndex]);
    }
		else
		{
			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = imageManager_->GetImage(depthImageIndex_);
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(commandBuffers_[frameIndex], VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
				0, nullptr, 1, &barrier);
		}

		queryPool.TimestampEnd(commandBuffers_[frameIndex], Wrapper::TIMESTAMP_POSTPROCESS, frameIndex);

    vkEndCommandBuffer(commandBuffers_[frameIndex]);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers_[frameIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &graphicPipelines_[GRAPHICS_BLENDING].passFinishedSemaphore;
    submitInfo.waitSemaphoreCount = 1;
    const VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submitInfo.pWaitDstStageMask = &waitDstStageMask;
    submitInfo.pWaitSemaphores = startSemaphore;

    const auto& graphicsQueue = queueManager->GetQueue(QueueManager::QUEUE_GRAPHICS);
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
  }

  VkSemaphore* PostProcessPass::GetFinishedSemaphore()
  {
    return &graphicPipelines_[GRAPHICS_BLENDING].passFinishedSemaphore;
  }

  void PostProcessPass::OnReloadShaders(VkDevice device, ShaderManager* shaderManager)
  {
    Pass::OnReloadShaders(device, shaderManager);

    CreateGraphicPipeline(device, shaderManager);
  }
}