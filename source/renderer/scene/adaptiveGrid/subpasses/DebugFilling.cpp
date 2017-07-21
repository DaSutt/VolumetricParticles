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

#include "DebugFilling.h"

#include "..\GridLevel.h"
#include "..\AdaptiveGridConstants.h"

#include "..\..\..\resources\BufferManager.h"
#include "..\..\..\resources\ImageManager.h"

#include "..\..\..\passResources\ShaderBindingManager.h"

#include "..\..\..\wrapper\Barrier.h"

#include "..\..\..\passes\GuiPass.h"


namespace Renderer
{
	std::array<glm::vec4, 3> levelColors = {
		glm::vec4(0,0,1,0),
		glm::vec4(0,1,0,0),
		glm::vec4(1,0,0,0)
	};

	std::array<glm::vec4, 3> mipMapColors = {
		glm::vec4(0,1,1,0),
		glm::vec4(1,1,0,0),
		glm::vec4(1,0,1,0)
	};

	void DebugFilling::SetGpuResources(int atlasImageIndex, int atlasBufferIndex)
	{
		atlasImageIndex_ = atlasImageIndex;
		atlasBufferIndex_ = atlasBufferIndex;
	}

	int DebugFilling::GetShaderBinding(ShaderBindingManager* bindingManager, int frameCount)
	{
		ShaderBindingManager::PushConstantInfo pushConstantInfo = {};
		VkPushConstantRange objectIds = {};
		objectIds.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		objectIds.offset = 0;
		objectIds.size = sizeof(PushConstantData);
		pushConstantInfo.pushConstantRanges.push_back(objectIds);
		pushConstantInfo.pass = SUBPASS_VOLUME_ADAPTIVE_DEBUG_FILLING;
		pushConstantIndex_ = bindingManager->RequestPushConstants(pushConstantInfo)[0];
		
		ShaderBindingManager::BindingInfo bindingInfo = {};
		bindingInfo.pass = SUBPASS_VOLUME_ADAPTIVE_DEBUG_FILLING;
		bindingInfo.resourceIndex = { atlasImageIndex_, atlasBufferIndex_};
		bindingInfo.stages = { VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_COMPUTE_BIT };
		bindingInfo.types = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER };
		bindingInfo.Ref_image = true;
		bindingInfo.refactoring_ = { true, true };
		bindingInfo.setCount = frameCount;
		
		bindingManager_ = bindingManager;

		return bindingManager->RequestShaderBinding(bindingInfo);
	}

	void DebugFilling::SetImageCount(GridLevel* mostDetailedLevel)
	{
		dispatchCount_ = mostDetailedLevel->GetMaxImageOffset();
	}

	void DebugFilling::AddDebugNodes(std::vector<GridLevel>& gridLevels)
	{
		if (GuiPass::GetVolumeState().debugFilling_levelIndex)
		{
			debugNodes_.clear();
			for (size_t i = 0; i < gridLevels.size(); ++i)
			{
				const auto& nodeInfos = gridLevels[i].GetNodeData().GetNodeInfos();
				for (const auto& node : nodeInfos)
				{
					debugNodes_.push_back(
						{ levelColors[i], node.textureOffset, LEVEL_INDEX}
					);
					if (NodeData::MipMapSet(node.textureOffset))
					{
						assert(node.textureOffsetMipMap != 0 && i < gridLevels.size() - 1);
						debugNodes_.push_back(
							{ mipMapColors[i], node.textureOffsetMipMap, LEVEL_INDEX }
						);
					}
				}
			}
		}
	}

	void DebugFilling::Dispatch(ImageManager* imageManager, VkCommandBuffer commandBuffer)
	{
		if (dispatchCount_ <= 0)
		{
			return;
		}

		VkPipelineLayout pipelineLayout = bindingManager_->GetPipelineLayout(SUBPASS_VOLUME_ADAPTIVE_DEBUG_FILLING);

		if (GuiPass::GetVolumeState().debugFilling_imageIndex)
		{
			PushConstantData pushData;
			pushData.nodeType = IMAGE_INDEX;
			vkCmdPushConstants(commandBuffer, pipelineLayout,
				VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantData), &pushData);

			vkCmdDispatch(commandBuffer, dispatchCount_, 1, 1);
		}
		else if (GuiPass::GetVolumeState().debugFilling_levelIndex)
		{
			for (const auto& debugNode : debugNodes_)
			{
				vkCmdPushConstants(commandBuffer, pipelineLayout,
					VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantData), &debugNode);

				vkCmdDispatch(commandBuffer, 1, 1, 1);
			}
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
}