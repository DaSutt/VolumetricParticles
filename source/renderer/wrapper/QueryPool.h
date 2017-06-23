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

#pragma once

#include <vulkan\vulkan.h>
#include <vector>
#include <array>
#include <time.h>

#include "TimeStamps.h"

namespace Wrapper
{
	//Singleton to hold all debug queries
	class QueryPool
	{
	public:
		static QueryPool& GetInstance();
		void CreatePool(VkDevice device, VkPhysicalDevice physicalDevice, int frameCount);
		void DestroyPool(VkDevice device);

		void Reset(VkCommandBuffer commandBuffer, int frameIndex);
		void TimestampStart(VkCommandBuffer cmdBuffer, TimeStamps index, int frameIndex);
		void TimestampEnd(VkCommandBuffer cmdBuffer, TimeStamps index, int frameIndex);
	private:
		QueryPool() = default;
		~QueryPool() = default;
		QueryPool(const QueryPool&) = delete;
		QueryPool& operator=(const QueryPool&) = delete;

		void StartSaving(const int count);
		void SaveQueries(int frameIndex);
		void SaveFile();

		std::vector<VkQueryPool> timeQueryPools_;
		uint32_t maxQueryCount_ = TIMESTAMP_MAX * 2;
		float timestampPeriod_ = 0.0f;

		bool saveTimeStamps_ = false;
		bool performTimeStamps_ = false;
		int timeStampCount_ = 0;
		int timeStampMax_ = 0;
		std::array<uint64_t, TIMESTAMP_MAX * 2> timeStamps_;
		std::vector<float> timeIntervalBuffer_;
		std::vector<float> gpuTotalTime_;
		std::vector<clock_t> cpuFrameStart_;
		std::vector<float> cpuTotalTime_;

		//TODO should be removed
		VkDevice device_ = VK_NULL_HANDLE;
	};
}