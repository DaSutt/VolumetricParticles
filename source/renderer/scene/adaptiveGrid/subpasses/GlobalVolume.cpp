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

#include "GlobalVolume.h"

#include "..\..\..\resources\ImageManager.h"
#include "..\..\..\passResources\ShaderBindingManager.h"
#include "..\..\..\resources\BufferManager.h"
#include "..\..\..\passes\GuiPass.h"
#include "..\GroundFog.h"
#include "..\AdaptiveGridConstants.h"
#include "..\..\..\wrapper\QueryPool.h"

namespace Renderer
{
	void GlobalVolume::RequestResources(BufferManager* bufferManager, int frameCount, int atlasImageIndex)
	{
		imageAtlasIndex_ = atlasImageIndex;

		BufferManager::BufferInfo cbInfo;
		cbInfo.typeBits = BufferManager::BUFFER_CONSTANT_BIT | BufferManager::BUFFER_SCENE_BIT;
		cbInfo.pool = BufferManager::MEMORY_CONSTANT;
		cbInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		cbInfo.bufferingCount = frameCount;
		cbInfo.data = &cbData_;
		cbInfo.size = sizeof(CBData);
		cbIndex_ = bufferManager->Ref_RequestBuffer(cbInfo);
	}

	int GlobalVolume::GetShaderBinding(ShaderBindingManager* bindingManager, int frameCount)
	{
		ShaderBindingManager::BindingInfo bindingInfo = {};
		bindingInfo.setCount = frameCount;
		bindingInfo.pass = SUBPASS_VOLUME_ADAPTIVE_GLOBAL;
		bindingInfo.resourceIndex = { imageAtlasIndex_, cbIndex_ };
		bindingInfo.stages = { VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_COMPUTE_BIT };
		bindingInfo.types = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER };
		bindingInfo.Ref_image = true;
		bindingInfo.refactoring_ = { true, true };

		return bindingManager->RequestShaderBinding(bindingInfo);
	}

	void GlobalVolume::UpdateCB(GroundFog* groundFog)
	{
		const auto& globalValue = GuiPass::GetVolumeState().globalValue;
		cbData_.globalValue = glm::vec4(0);
		cbData_.groundFogValue = glm::vec4(0);
		cbData_.groundFogTexelStart = GridConstants::imageResolution;
		updated_ = false;

		//Fill volume with global values at each texel
		const float scattering = globalValue.scattering;
		const float extinction = scattering + globalValue.absorption;
		const float phaseG = globalValue.phaseG;
		//No dispatch if volume is not filled with anything
		if (scattering != 0.0f || extinction != 0.0f)
		{
			cbData_.globalValue = glm::vec4(
				scattering, extinction, phaseG, 0.0f);
			updated_ = true;
		}

		/*const int texelStart = groundFog->GetCoarseGridTexel();
		if (texelStart != GridConstants::imageResolution - 1 &&
			(volumeState.scatteringVolumes != 0.0f || volumeState.absorptionVolumes != 0.0f))
		{
			const float scatFog = volumeState.scatteringVolumes + scattering;
			const float extFog = volumeState.scatteringVolumes + volumeState.absorptionVolumes + extinction;
			float phaseFog = volumeState.phaseVolumes;
			//average the phase function
			if (scatFog != 0.0f && scattering != 0.0f)
			{
				phaseFog = (phaseFog + phaseG) * 0.5f;
			}
			//else only use the active one
			else
			{
				phaseFog = scatFog != 0 ? phaseFog : phaseG;
			}

			cbData_.groundFogValue = glm::vec3(scatFog, extFog, phaseFog);
			cbData_.groundFogTexelStart = texelStart;
			updated_ = true;
		}
		*/
	}

	void GlobalVolume::Dispatch(ImageManager* imageManager, VkCommandBuffer commandBuffer, int frameIndex)
	{
		//TODO replace the clear image command with individual clears
		VkClearColorValue clearValue = { 0.0f, 0.0f, 0.0f, 0.0f};
		imageManager->Ref_ClearImage(commandBuffer, imageAtlasIndex_, clearValue);

		if(updated_)
		{
			auto& queryPool = Wrapper::QueryPool::GetInstance();
			queryPool.TimestampStart(commandBuffer, Wrapper::TIMESTAMP_GRID_GLOBAL, frameIndex);

			vkCmdDispatch(commandBuffer, 1, 1, 1);

			queryPool.TimestampEnd(commandBuffer, Wrapper::TIMESTAMP_GRID_GLOBAL, frameIndex);
		}
	}
}