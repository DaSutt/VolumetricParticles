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

#include "QueryPool.h"
#include "..\passes\GuiPass.h"

#include <assert.h>

#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <ctime>
#include <time.h>
#include <stdio.h>
#include <sstream>


namespace Wrapper
{
	QueryPool& QueryPool::GetInstance()
	{
		static QueryPool queryPool;
		return queryPool;
	}

	void QueryPool::CreatePool(VkDevice device, VkPhysicalDevice physicalDevice, int frameCount)
	{
		VkQueryPoolCreateInfo createInfo = { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
		createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
		createInfo.queryCount = maxQueryCount_;

		for (int i = 0; i < frameCount; ++i)
		{
			VkQueryPool pool = VK_NULL_HANDLE;
			const auto result = vkCreateQueryPool(device, &createInfo, nullptr, &pool);
			assert(result == VK_SUCCESS);
			
			timeQueryPools_.push_back(pool);
		}
		VkPhysicalDeviceProperties deviceProperties = {};
		vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
		timestampPeriod_ = deviceProperties.limits.timestampPeriod;

		cpuFrameStart_.resize(frameCount);

		device_ = device;
	}

	void QueryPool::DestroyPool(VkDevice device)
	{
		for (auto& pool : timeQueryPools_)
		{
			vkDestroyQueryPool(device, pool, nullptr);
			pool = VK_NULL_HANDLE;
		}
	}

	void QueryPool::Reset(VkCommandBuffer commandBuffer, int frameIndex)
	{
		const auto& menuState = Renderer::GuiPass::GetMenuState();
		performTimeStamps_ = menuState.performTimeQueries;

		//save the start time of the rendering
		cpuFrameStart_[frameIndex] = std::clock();
		
		if (menuState.saveTimeQueries)
		{
			StartSaving(menuState.queryFrameCount);
		}

		SaveQueries(frameIndex);

		vkCmdResetQueryPool(commandBuffer, timeQueryPools_[frameIndex], 0, maxQueryCount_);
	}

	void QueryPool::TimestampStart(VkCommandBuffer cmdBuffer, TimeStamps index, int frameIndex)
	{
		if (performTimeStamps_)
		{
			const int startIndex = index * 2;
			VkPipelineStageFlagBits flags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			switch (index)
			{
			case TIMESTAMP_SHADOW_MAP:
			case TIMESTAMP_MESH:
			case TIMESTAMP_POSTPROCESS:
			case TIMESTAMP_PASS_GUI:
				flags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				break;
			case TIMESTAMP_GRID_GLOBAL:
			case TIMESTAMP_GRID_GROUND_FOG:
			case TIMESTAMP_GRID_PARTICLES:
			case TIMESTAMP_GRID_NEIGHBOR_UPDATE:
			case TIMESTAMP_GRID_MIPMAPPING_0:
			case TIMESTAMP_GRID_MIPMAPPING_1:
			case TIMESTAMP_GRID_MIPMAPPING_MERGIN_0:
			case TIMESTAMP_GRID_MIPMAPPING_MERGIN_1:
			case TIMESTAMP_GRID_RAYMARCHING:
				flags = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
				break;
			default:
				printf("Time stamp query not supported.");
				assert(false);
			}
			vkCmdWriteTimestamp(cmdBuffer, flags, timeQueryPools_[frameIndex], startIndex);
		}
	}

	void QueryPool::TimestampEnd(VkCommandBuffer cmdBuffer, TimeStamps index, int frameIndex)
	{
		if (performTimeStamps_)
		{
			const int endIndex = index * 2 + 1;
			VkPipelineStageFlagBits flags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			switch (index)
			{
			case TIMESTAMP_SHADOW_MAP:
			case TIMESTAMP_MESH:
			case TIMESTAMP_POSTPROCESS:
			case TIMESTAMP_PASS_GUI:
				flags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				break;
			case TIMESTAMP_GRID_GLOBAL:
			case TIMESTAMP_GRID_GROUND_FOG:
			case TIMESTAMP_GRID_PARTICLES:
			case TIMESTAMP_GRID_NEIGHBOR_UPDATE:
			case TIMESTAMP_GRID_MIPMAPPING_0:
			case TIMESTAMP_GRID_MIPMAPPING_1:
			case TIMESTAMP_GRID_MIPMAPPING_MERGIN_0:
			case TIMESTAMP_GRID_MIPMAPPING_MERGIN_1:
			case TIMESTAMP_GRID_RAYMARCHING:
				flags = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
				break;
			default:
				printf("Time stamp query not supported.");
				assert(false);
			}
			vkCmdWriteTimestamp(cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, timeQueryPools_[frameIndex], endIndex);
		}
	}

	void QueryPool::StartSaving(const int count)
	{
		if (!saveTimeStamps_)
		{
			printf("Started with saving of timestamps\n");
			saveTimeStamps_ = true;
			timeStampCount_ = 0;
			timeStampMax_ = count;
			timeIntervalBuffer_.clear();
			timeIntervalBuffer_.resize(count * TIMESTAMP_MAX);
			gpuTotalTime_.clear();
			gpuTotalTime_.resize(count);
			cpuTotalTime_.clear();
			cpuTotalTime_.resize(count);
		}
	}

	void QueryPool::SaveQueries(int frameIndex)
	{
		if (saveTimeStamps_)
		{
			if (timeStampCount_ < timeStampMax_)
			{
				auto& timePool = timeQueryPools_[frameIndex];
				const auto result = vkGetQueryPoolResults(device_, timePool, 0, maxQueryCount_,
					sizeof(uint64_t) * maxQueryCount_, timeStamps_.data(), sizeof(uint64_t),
					VK_QUERY_RESULT_64_BIT);

				const int offset = timeStampCount_ * TIMESTAMP_MAX;
				for (int i = 0; i < TIMESTAMP_MAX; ++i)
				{
					const int timeStampIndex = i * 2;
					timeIntervalBuffer_[offset + i] = static_cast<float>(timeStamps_[timeStampIndex + 1] - timeStamps_[timeStampIndex])
						* timestampPeriod_ * 1.0e-6f;
				}
				gpuTotalTime_[timeStampCount_] = static_cast<float>(timeStamps_.back() - timeStamps_.front())
					* timestampPeriod_ * 1.0e-6f;

				cpuTotalTime_[timeStampCount_] = static_cast<float>(std::clock() - cpuFrameStart_[frameIndex]) /
					CLOCKS_PER_SEC;
				timeStampCount_++;
			}
			else
			{
				SaveFile();
				saveTimeStamps_ = false;
			}
		}
	}

	void QueryPool::SaveFile()
	{
		auto t = std::time(nullptr);
		std::tm localTime;
		localtime_s(&localTime, &t);
		
		std::ostringstream fileName;
		fileName << std::put_time(&localTime, "queries_%d_%m_%H%M%S.csv");

		std::ofstream file;
		file.open(fileName.str());
		if (file.good())
		{
			file << "Shadow Map,Mesh,Grid Global,Grid GroundFog,Grid Particles,Grid Neighbors,Grid MipMapping 0," <<
				"Grid MipMapping 1,Grid MipMapping Merging 0,Grid MipMapping Merging 1,Grid Raymarching," <<
				"Grid Postprocess,Grid Gui,GPU total,CPU total,\n";
			for (int i = 0; i < timeStampMax_; ++i)
			{
				const int offset = i * TIMESTAMP_MAX;
				for (int type = 0; type < TIMESTAMP_MAX; ++type)
				{
					file << timeIntervalBuffer_[offset + type] << ",";
				}
				file << gpuTotalTime_[i] << "," << cpuTotalTime_[i] << ",\n";
			}
			printf("Saved queries for %d frames\n", timeStampMax_);
		}
		else
		{
			printf("Failed creating time stamp file");
		}
	}
}