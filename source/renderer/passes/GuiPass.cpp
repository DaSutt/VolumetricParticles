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

#include "GuiPass.h"

#include <imgui\imgui.h>
#define _USE_MATH_DEFINES
#include <math.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm\gtx\euler_angles.hpp>

#include "Passes.h"
#include "..\passResources\ShaderBindingManager.h"
#include "..\passResources\RenderPassManager.h"
#include "..\passResources\ShaderManager.h"
#include "..\passResources\FrameBufferManager.h"

#include "..\resources\QueueManager.h"
#include "..\resources\ImageManager.h"
#include "..\resources\BufferManager.h"

#include "..\pipelineState\PipelineState.h"

#include "..\wrapper\Surface.h"
#include "..\wrapper\Instance.h"
#include "..\wrapper\Synchronization.h"
#include "..\wrapper\QueryPool.h"

#include "..\..\input\InputHandler.h"
#include "..\..\utility\Status.h"

namespace Renderer
{
  GuiPass::MenuState GuiPass::menuState_ = {};
  GuiPass::LightingState GuiPass::lightingState_ = {};
  GuiPass::VolumeState GuiPass::volumeState_ = {};
	GuiPass::ParticleState GuiPass::particleState_ = {};
	GuiPass::DebugVisState GuiPass::debugVisState_ = {};
	GuiPass::ConfigState GuiPass::configState_ = {};

  GuiPass::MenuState::MenuState() :
    saveConfiguration{ false },
    loadScene{ false },
    debugRendering{ false },
    reloadShaders{ false },
    fixCameraFrustum{ false },
		exportFogTexture{ false },
		performTimeQueries{false},
		saveTimeQueries{false},
		queryFrameCount{100}
  {}

  GuiPass::LightingState::LightingState() :
    irradiance{4.0f, 4.0f, 4.0f},
    lightYRotation{ 0.87f },
    lightZRotation{ 0.67f },
    shadowLogWeight{0.5f}
  {}

	GuiPass::TextureValue::TextureValue() :
		absorption{0.0f},
		scattering{0.0f},
		phaseG{0.0f}
	{}

	GuiPass::VolumeState::VolumeState() :
		globalValue{},
		absorptionVolumes{ 0.0f },
		scatteringVolumes{ 0.03f },
		phaseVolumes{-0.4f},
    emission{0.0f, 0.0f, 0.0f},
    fogHeight{ 0.0f },
    stepCount { 200 },
		lightStepDepth{50.0f},
		jitteringScale { 0.2f },
		lodScale { 1.0f },
		noiseScale { 0.1f },
		minTransmittance{0.0f},
		maxDepth{ 500.0f},
		shadowRayPerLevel{16}
  {}

	GuiPass::ParticleState::ParticleState() :
		particleCount{40},
		spawnRadius{2.0f},
		minParticleRadius{0.2f},
		maxParticleRadius{1.0f}
	{}


	GuiPass::DebugVisState::DebugVisState() :
		nodeRendering{false},
		debugFillingType{GuiPass::DebugVisState::DEBUG_FILL_NONE}
	{}

	bool GuiPass::DebugVisState::FillingStateSet(GuiPass::DebugVisState::DebugFillingType type) const
	{
		return debugFillingType == type;
	}

	const char * menuNames_debugFilling[] = 
	{
		"Disable Debug Filling",
		"Fill with Image Indices",
		"Fill With Level Indices"
	};

	GuiPass::ConfigState::ConfigState() :
		showDebugVis{ true }
	{}

  GuiPass::GuiPass(ShaderBindingManager* bindingManager, RenderPassManager* renderPassManager) :
    Pass::Pass(bindingManager, renderPassManager)
  {}

  void GuiPass::RequestResources(ImageManager* imageManager, BufferManager* bufferManager, int frameCount)
  {
    auto pass = SUBPASS_GUI;

    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    size_t upload_size = width*height * 4 * sizeof(char);

    ImageManager::ImageInfo imageInfo = {};
    imageInfo.extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.sampler = ImageManager::SAMPLER_LINEAR;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.initData.size = upload_size;
    imageInfo.initData.data = pixels;
    imageInfo.type = ImageManager::IMAGE_IMMUTABLE;
    fontTextureIndex_ = imageManager->RequestImage(imageInfo);

    ShaderBindingManager::BindingInfo bindingInfo = {};
    bindingInfo.types = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER };
    bindingInfo.stages = { VK_SHADER_STAGE_FRAGMENT_BIT };
    bindingInfo.resourceIndex = { fontTextureIndex_ };
    bindingInfo.pass = pass;
    bindingInfo.setCount = frameCount;
    graphicPipelines_.push_back(bindingManager_->RequestShaderBinding(bindingInfo));

    ShaderBindingManager::PushConstantInfo pushConstantInfo = {};
    VkPushConstantRange objectIds = {};
    objectIds.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    objectIds.offset = sizeof(float) * 0;
    objectIds.size = sizeof(float) * 4;
    pushConstantInfo.pushConstantRanges.push_back(objectIds);
    pushConstantInfo.pass = pass;
    pushConstantIndex_ = bindingManager_->RequestPushConstants(pushConstantInfo)[0];
  }

  bool GuiPass::Create(VkDevice device, ShaderManager* shaderManager,
    FrameBufferManager* frameBufferManager, Surface* surface, ImageManager* imageManager)
  {
    if (!CreateFrameBuffers(device, frameBufferManager, surface)) { return false; }
    if (!CreateGraphicPipeline(device, shaderManager)) { return false; }
    if (!CreateCommandBuffers(device, QueueManager::QUEUE_GRAPHICS, surface->GetImageCount())) { return false; }
    if (!Wrapper::CreateVulkanSemaphore(device, &graphicPipelines_[GRAPHIC_GUI].passFinishedSemaphore)) { return false; }

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->TexID = (void *)(intptr_t)imageManager->GetImage(fontTextureIndex_);

    return true;
  }

  bool GuiPass::CreateFrameBuffers(VkDevice device, FrameBufferManager* frameBufferManager, Surface* surface)
  {
    int backBufferCount = surface->GetImageCount();
    const auto& surfaceSize = surface->GetSurfaceSize();

    for (int i = 0; i < backBufferCount; ++i)
    {
      FrameBufferManager::Info frameBufferInfo;
      frameBufferInfo.imageAttachements =
      {
        { i, FrameBufferManager::IMAGE_SOURCE_SWAPCHAIN }
      };
      frameBufferInfo.pass = GRAPHIC_PASS_GUI;
      frameBufferInfo.width = surfaceSize.width;
      frameBufferInfo.height = surfaceSize.height;
      frameBufferInfo.resize = true;

      const auto index = frameBufferManager->RequestFrameBuffer(device, frameBufferInfo);
      if (index == -1)
      {
        return false;
      }
      frameBufferIndices_.push_back(index);
    }
    return true;
  }

  bool GuiPass::CreateGraphicPipeline(VkDevice device, ShaderManager* shaderManager)
  {
    auto subpass = SUBPASS_GUI;

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    const auto shaderStageInfo = shaderManager->GetShaderStageInfo(subpass);
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStageInfo.size());
    pipelineInfo.pStages = shaderStageInfo.data();

    const auto vertexInputState = GetVertexInputState();
    pipelineInfo.pVertexInputState = &vertexInputState;

    pipelineInfo.pInputAssemblyState = &g_inputAssemblyStates[ASSEMBLY_TRIANGLE_LIST];
    pipelineInfo.pRasterizationState = &g_rasterizerState[RASTERIZER_SOLID_NO_CULLING];
    pipelineInfo.pMultisampleState = &g_mulitsampleStates[MULTISAMPLE_DISABLED];
    pipelineInfo.pDepthStencilState = &g_depthStencilStates[DEPTH_DISABLED];
    pipelineInfo.pColorBlendState = BlendState::GetCreateInfo(BlendState::BLEND_GUI);
    pipelineInfo.pViewportState = &ViewPortState::GetState(ViewPortState::VIEWPORT_STATE_UNDEFINED);
    pipelineInfo.pDynamicState = DynamicState::GetCreateInfo(DynamicState::DYNAMIC_VIEWPORT_SCISSOR);

    pipelineInfo.layout = bindingManager_->GetPipelineLayout(subpass);
    pipelineInfo.renderPass = renderPassManager_->GetRenderPass(GetGraphicPass(subpass));
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

    graphicPipelines_[GRAPHIC_GUI].pipeline = pipeline;
    return true;
  }

  VkPipelineVertexInputStateCreateInfo GuiPass::GetVertexInputState()
  {
    static VkVertexInputBindingDescription binding_desc[1] = {};
    binding_desc[0].stride = sizeof(ImDrawVert);
    binding_desc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    static VkVertexInputAttributeDescription attribute_desc[3] = {};
    attribute_desc[0].location = 0;
    attribute_desc[0].binding = binding_desc[0].binding;
    attribute_desc[0].format = VK_FORMAT_R32G32_SFLOAT;
    attribute_desc[0].offset = (size_t)(&((ImDrawVert*)0)->pos);
    attribute_desc[1].location = 1;
    attribute_desc[1].binding = binding_desc[0].binding;
    attribute_desc[1].format = VK_FORMAT_R32G32_SFLOAT;
    attribute_desc[1].offset = (size_t)(&((ImDrawVert*)0)->uv);
    attribute_desc[2].location = 2;
    attribute_desc[2].binding = binding_desc[0].binding;
    attribute_desc[2].format = VK_FORMAT_R8G8B8A8_UNORM;
    attribute_desc[2].offset = (size_t)(&((ImDrawVert*)0)->col);

    VkPipelineVertexInputStateCreateInfo vertex_info = {};
    vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_info.vertexBindingDescriptionCount = 1;
    vertex_info.pVertexBindingDescriptions = binding_desc;
    vertex_info.vertexAttributeDescriptionCount = 3;
    vertex_info.pVertexAttributeDescriptions = attribute_desc;
    return vertex_info;
  }

  void GuiPass::HandleInput(InputHandler* inputHandler)
  {
    auto& io = ImGui::GetIO();
    const auto mousePos = inputHandler->MousePosition();
    io.MousePos = { mousePos.x, mousePos.y };
    io.MouseDown[0] = inputHandler->MouseDown(VirtualKey::MS_LB);
    io.MouseDown[1] = inputHandler->MouseDown(VirtualKey::MS_RB);
    io.MouseDown[2] = inputHandler->MouseDown(VirtualKey::MS_MB);

    // Read keyboard modifiers inputs
    io.KeyCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    io.KeyShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    io.KeyAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    io.KeySuper = false;

    // Hide OS mouse cursor if ImGui is drawing it
    if (io.MouseDrawCursor)
      SetCursor(NULL);

		//Update the state for debugTraversal
		volumeState_.debugTraversal = inputHandler->KeyPressed(VirtualKey::KB_F1);
		volumeState_.cursorPosition = inputHandler->MousePosition();
  }

  void GuiPass::Update(Instance* instance, BufferManager* bufferManager, float deltaTime)
  {
    ImGuiIO& io = ImGui::GetIO();
    io.DeltaTime = deltaTime;
    deltaTime_ = deltaTime;

    ImGui::NewFrame();

		//{
		//	ImGui::Begin("Configuration");
		//	{
		//		ImGui::Checkbox("Debug Visualization", &configState_.showDebugVis);
		//	}
		//	ImGui::End();
		//}

		menuState_.loadScene = false;
		menuState_.reloadShaders = false;

		ImGuiWindowFlags window_flags = {};
		window_flags |= ImGuiWindowFlags_MenuBar;
		if (ImGui::Begin("Configuration", nullptr, window_flags))
		{
			if (ImGui::BeginMenuBar())
			{
				if (ImGui::BeginMenu("Menu"))
				{
					if (ImGui::MenuItem("Load Scene"))
					{
						menuState_.loadScene = true;
					}
					if (ImGui::MenuItem("Reload Shaders"))
					{
						menuState_.reloadShaders = true;
					}
					ImGui::EndMenu();
				}
				ImGui::EndMenuBar();
			}
			
			ImGui::Spacing();
			if (ImGui::CollapsingHeader("Grid Data"))
			{
				if (ImGui::TreeNode("Global Data"))
				{
					ImGui::DragFloat("Absorption", &volumeState_.globalValue.absorption, 0.00001,
						0.0f, 1.0f, "%.06f");
					ImGui::DragFloat("Scattering", &volumeState_.globalValue.scattering, 0.00001,
						0.0f, 1.0f, "%.06f");
					ImGui::DragFloat("PhaseG", &volumeState_.globalValue.phaseG, 0.01,
						-1.0f, 1.0f);
					ImGui::TreePop();
				}
			}

			if (ImGui::CollapsingHeader("Debug Visualization"))
			{
				ImGui::Checkbox("Render Grid Nodes", &debugVisState_.nodeRendering);
				int type = debugVisState_.debugFillingType;
				ImGui::Text("Debug Filling Type");
				if (ImGui::Combo("##debugFilling_type", &type, menuNames_debugFilling,
					DebugVisState::DEBUG_FILL_MAX))
				{
					debugVisState_.debugFillingType = static_cast<DebugVisState::DebugFillingType>(type);
				}
			}

			if (ImGui::CollapsingHeader("Performance"))
			{
				ImGui::Text("Application\t%.4f ms/frame ", deltaTime_ * 1000.0f);
			}
		}
		ImGui::End();

    //menuState_.reloadShaders = ImGui::RadioButton("Reload shaders", true);
    //menuState_.loadScene = ImGui::RadioButton("Load Scene", true);
		//
    //ImGui::DragFloat3("Light irradiance", lightingState_.irradiance.data(), 0.1f);
    //ImGui::SliderFloat("Light y rotation", &lightingState_.lightYRotation,
    //  static_cast<float>(-M_PI), static_cast<float>(M_PI));
    //ImGui::SliderFloat("Light z rotation", &lightingState_.lightZRotation,
    //  static_cast<float>(-M_PI_2), static_cast<float>(M_PI_2));
		//
		//{
		//	glm::mat4 lightRotation = glm::eulerAngleYZ(
		//		lightingState_.lightYRotation,
		//		lightingState_.lightZRotation);
		//	lightingState_.lightVector = lightRotation * glm::vec4(-1.0f, 0.0f, 0.0f, 0.0f);
		//
		//	const auto& ir = lightingState_.irradiance;
		//	Status::UpdateDirectionalLight(lightingState_.lightVector, glm::vec3(ir[0], ir[1], ir[2]));
		//}
		//
    //static float globalValue[2];
    //ImGui::DragFloat2("Global scattering, absorption", globalValue, 0.001f, 0.0f, 0.1f);
    //ImGui::DragFloat("Global Phase G", &volumeState_.phaseG, 0.01f, -1.0f, 1.0f);
    //volumeState_.scattering = globalValue[0];
    //volumeState_.absorption = globalValue[1];
		//
		//ImGui::SliderFloat("Fog height percentage", &volumeState_.fogHeight, 0.0f, 1.0f);
		//ImGui::SliderFloat("Fog noise scale", &volumeState_.noiseScale, 0.01f, 1.0f);
		//
    //static float volumeValues[2];
    //ImGui::DragFloat2("Fog scattering, absorption", volumeValues, 0.001f, 0.0f);
    //ImGui::DragFloat("Fog Phase G", &volumeState_.phaseVolumes, 0.01f, -1.0f, 1.0f);
    //volumeState_.scatteringVolumes = volumeValues[0];
    //volumeState_.absorptionVolumes = volumeValues[1];
		//
		//ImGui::DragFloat("Raymarching max depth", &volumeState_.maxDepth, 1.0f, 1.0f, 1000.0f);
    //ImGui::DragInt("Raymarching max steps", &volumeState_.stepCount, 1);
		//ImGui::DragInt("Max Shadow rays per level", &volumeState_.shadowRayPerLevel, 1, 1);
		//ImGui::SliderFloat("Lighting step depth", &volumeState_.lightStepDepth, 1.0f, 200.0f);
		//ImGui::SliderFloat("Jittering scale", &volumeState_.jitteringScale, 0.01f, 1.0f);
		//ImGui::SliderFloat("LOD scale", &volumeState_.lodScale, 0.01f, 32.0f);
		//
		//ImGui::SliderInt("Particle count", &particleState_.particleCount, 1, 100);
		//static float radiusValues[] = { particleState_.minParticleRadius, particleState_.maxParticleRadius };
		//ImGui::SliderFloat2("Particle min, max radius", radiusValues, 0.1f, 5.0f);
		//particleState_.minParticleRadius = radiusValues[0];
		//particleState_.maxParticleRadius = radiusValues[1];
		//ImGui::SliderFloat("Particle spawn radius", &particleState_.spawnRadius, 0.2f, 10.0f);
		//
    //menuState_.saveConfiguration = ImGui::RadioButton("Save configuration", true);
		//menuState_.exportFogTexture = ImGui::RadioButton("Save fog density texture", true);
		//
		//ImGui::Checkbox("Perform time queries", &menuState_.performTimeQueries);
		//menuState_.saveTimeQueries = ImGui::RadioButton("Save time queries", menuState_.performTimeQueries);
		//ImGui::SliderInt("Number of time query frames", &menuState_.queryFrameCount, 1, 300);
		//
    //ImGui::Checkbox("Fix Camera Frustum", &menuState_.fixCameraFrustum);
    //ImGui::DragFloat("Shadow map log weight", &lightingState_.shadowLogWeight, 0.01f, 0.0f, 1.0f);
    
		//
		//ImGui::End();
		//
		//if (configState_.showDebugVis)
		//{
		//	ImGui::Begin("Debug Visualization");
		//	{
		//		ImGui::Checkbox("Render Grid Nodes", &debugVisState_.nodeRendering);
		//		int type = debugVisState_.debugFillingType;
		//		if (ImGui::Combo("Debug Filling of Volumes", &type, menuNames_debugFilling,
		//			DebugVisState::DEBUG_FILL_MAX))
		//		{
		//			debugVisState_.debugFillingType = static_cast<DebugVisState::DebugFillingType>(type);
		//			printf("New Type %s\n", menuNames_debugFilling[type]);
		//		}
		//	}
		//	ImGui::End();
		//}
    ImGui::Render();

    const auto device = instance->GetDevice();
    const auto physicalDevice = instance->GetPhysicalDevice();

    ImDrawData* draw_data = ImGui::GetDrawData();
    if (vertexBufferIndex_ == -1)
    {
      BufferManager::BufferInfo bufferInfo = {};
      bufferInfo.pool = BufferManager::MEMORY_SCENE;
      bufferInfo.typeBits = BufferManager::BUFFER_SCENE_BIT | BufferManager::BUFFER_STAGING_BIT;
      bufferInfo.size = std::max(draw_data->TotalVtxCount * sizeof(ImDrawVert), 140000ull);
      bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      vertexBufferIndex_ = bufferManager->RequestBuffer(bufferInfo);
    }
    if (indexBufferIndex_ == -1)
    {
      BufferManager::BufferInfo bufferInfo = {};
      bufferInfo.pool = BufferManager::MEMORY_SCENE;
      bufferInfo.typeBits = BufferManager::BUFFER_SCENE_BIT | BufferManager::BUFFER_STAGING_BIT;
      bufferInfo.size = std::max(draw_data->TotalIdxCount * sizeof(ImDrawIdx), 26000ull);
      bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      indexBufferIndex_ = bufferManager->RequestBuffer(bufferInfo);
    }

    // Upload Vertex and index Data:
    {
      auto vtx_dst = bufferManager->Map(vertexBufferIndex_, BufferManager::BUFFER_STAGING_BIT);
      auto idx_dst = bufferManager->Map(indexBufferIndex_, BufferManager::BUFFER_STAGING_BIT);

      for (int n = 0; n < draw_data->CmdListsCount; n++)
      {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtx_dst = reinterpret_cast<ImDrawVert*>(vtx_dst) + cmd_list->VtxBuffer.Size;
        idx_dst = reinterpret_cast<ImDrawIdx*>(idx_dst) + cmd_list->IdxBuffer.Size;
      }

      bufferManager->Unmap(vertexBufferIndex_, BufferManager::BUFFER_STAGING_BIT);
      bufferManager->Unmap(indexBufferIndex_, BufferManager::BUFFER_STAGING_BIT);

      bufferManager->RequestBufferCopy(vertexBufferIndex_, BufferManager::BUFFER_STAGING_BIT);
      bufferManager->RequestBufferCopy(indexBufferIndex_, BufferManager::BUFFER_STAGING_BIT);
    }
  }

  void GuiPass::Render(Surface* surface, FrameBufferManager* frameBufferManager,
    QueueManager* queueManager, BufferManager* bufferManager,
    std::vector<VkImageMemoryBarrier>& memoryBarrier, VkSemaphore* startSemaphore, int frameIndex)
  {
		auto& queryPool = Wrapper::QueryPool::GetInstance();

    const auto pass = SUBPASS_GUI;
    {
      VkCommandBufferBeginInfo beginInfo = {};
      beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      vkBeginCommandBuffer(commandBuffers_[frameIndex], &beginInfo);

			queryPool.TimestampStart(commandBuffers_[frameIndex], Wrapper::TIMESTAMP_PASS_GUI, frameIndex);

      VkRenderPassBeginInfo renderPassInfo = {};
      renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
      renderPassInfo.renderPass = renderPassManager_->GetRenderPass(GetGraphicPass(pass));
      renderPassInfo.framebuffer = frameBufferManager->GetFrameBuffer(frameBufferIndices_[frameIndex]);
      renderPassInfo.renderArea.offset = { 0,0 };
      renderPassInfo.renderArea.extent = surface->GetSurfaceSize();
      renderPassInfo.clearValueCount = 0;

      vkCmdBeginRenderPass(commandBuffers_[frameIndex], &renderPassInfo,
        VK_SUBPASS_CONTENTS_INLINE);
    }
    // Bind pipeline and descriptor sets:
    {
      vkCmdBindPipeline(commandBuffers_[frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS,
        graphicPipelines_[GRAPHIC_GUI].pipeline);
      vkCmdBindDescriptorSets(commandBuffers_[frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS,
        bindingManager_->GetPipelineLayout(pass), 0, 1,
        &bindingManager_->GetDescriptorSet(graphicPipelines_[GRAPHIC_GUI].shaderBinding, frameIndex), 0, NULL);
    }
    // Bind Vertex And Index Buffer:
    {
      bufferManager->Bind(commandBuffers_[frameIndex], vertexBufferIndex_);
      bufferManager->Bind(commandBuffers_[frameIndex], indexBufferIndex_);
    }
    // Setup viewport:
    {
      ImGuiIO& io = ImGui::GetIO();
      VkViewport viewport;
      viewport.x = 0;
      viewport.y = 0;
      viewport.width = io.DisplaySize.x;
      viewport.height = io.DisplaySize.y;
      viewport.minDepth = 0.0f;
      viewport.maxDepth = 1.0f;
      vkCmdSetViewport(commandBuffers_[frameIndex], 0, 1, &viewport);
    }
    // Setup scale and translation:
    {
      ImGuiIO& io = ImGui::GetIO();
      float scale[2];
      scale[0] = 2.0f / io.DisplaySize.x;
      scale[1] = 2.0f / io.DisplaySize.y;
      float translate[2];
      translate[0] = -1.0f;
      translate[1] = -1.0f;
      vkCmdPushConstants(commandBuffers_[frameIndex], bindingManager_->GetPipelineLayout(pass), VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 0, sizeof(float) * 2, scale);
      vkCmdPushConstants(commandBuffers_[frameIndex], bindingManager_->GetPipelineLayout(pass), VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 2, sizeof(float) * 2, translate);
    }
    // Render the command lists:
    ImDrawData* draw_data = ImGui::GetDrawData();
    int vtx_offset = 0;
    int idx_offset = 0;
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
      const ImDrawList* cmd_list = draw_data->CmdLists[n];
      for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
      {
        const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
        if (pcmd->UserCallback)
        {
          pcmd->UserCallback(cmd_list, pcmd);
        }
        else
        {
          VkRect2D scissor;
          scissor.offset.x = (int32_t)(pcmd->ClipRect.x);
          scissor.offset.y = (int32_t)(pcmd->ClipRect.y);
          scissor.extent.width = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
          scissor.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y + 1); // TODO: + 1??????
          vkCmdSetScissor(commandBuffers_[frameIndex], 0, 1, &scissor);
          vkCmdDrawIndexed(commandBuffers_[frameIndex], pcmd->ElemCount, 1, idx_offset, vtx_offset, 0);
        }
        idx_offset += pcmd->ElemCount;
      }
      vtx_offset += cmd_list->VtxBuffer.Size;
    }

    {
      if (!memoryBarrier.empty())
      {
        vkCmdPipelineBarrier(commandBuffers_[frameIndex],
          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
          0,
          0, nullptr,
          0, nullptr,
          static_cast<uint32_t>(memoryBarrier.size()), memoryBarrier.data());
      }
      vkCmdEndRenderPass(commandBuffers_[frameIndex]);

			queryPool.TimestampEnd(commandBuffers_[frameIndex], Wrapper::TIMESTAMP_PASS_GUI, frameIndex);

      vkEndCommandBuffer(commandBuffers_[frameIndex]);

      const VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      VkSubmitInfo submitInfo = {};
      submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
      submitInfo.waitSemaphoreCount = 1;
      submitInfo.pWaitSemaphores = startSemaphore;
      submitInfo.pWaitDstStageMask = &waitDstStageMask;
      submitInfo.signalSemaphoreCount = 1;
      submitInfo.pSignalSemaphores = &graphicPipelines_[GRAPHIC_GUI].passFinishedSemaphore;
      submitInfo.commandBufferCount = 1;
      submitInfo.pCommandBuffers = &commandBuffers_[frameIndex];
      vkQueueSubmit(queueManager->GetQueue(QueueManager::QUEUE_GRAPHICS), 1, &submitInfo,
        VK_NULL_HANDLE);
    }
  }

  VkSemaphore* GuiPass::GetFinishedSemaphore()
  {
    return &graphicPipelines_[GRAPHIC_GUI].passFinishedSemaphore;
  }

  void GuiPass::OnResize(uint32_t width, uint32_t height)
  {
    ImGui::GetIO().DisplaySize = { static_cast<float>(width), static_cast<float>(height)};
  }

  void GuiPass::OnReloadShaders(VkDevice device, ShaderManager* shaderManager)
  {
    Pass::OnReloadShaders(device, shaderManager);

    CreateGraphicPipeline(device, shaderManager);
  }

  void GuiPass::Release(VkDevice device)
  {
    Pass::Release(device);
    ImGui::Shutdown();
  }

  void GuiPass::SetWindow(HWND window)
  {
    ImGuiIO& io = ImGui::GetIO();
    io.ImeWindowHandle = window;

    RECT rect;
    GetClientRect(window, &rect);
    io.DisplaySize = ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));
  }
}