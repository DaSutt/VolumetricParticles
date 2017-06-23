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

#include "Barrier.h"

namespace Wrapper
{
	Barriers::Barriers() :
		memoryBarrierCount{ 0 },
		pMemoryBarriers{ nullptr },
		bufferMemoryBarrierCount{ 0 },
		pBufferMemoryBarriers{ nullptr },
		imageMemoryBarrierCount{ 0 },
		pImageMemoryBarriers{ nullptr }
	{}

	PipelineBarrierInfo::PipelineBarrierInfo() :
		src{ 0 },
		dst{ 0 }	,
		barriers{}
	{}

	void PipelineBarrierInfo::AddMemoryBarriers(std::vector<VkMemoryBarrier>& memoryBarriers)
	{
		barriers.memoryBarrierCount = static_cast<uint32_t>(memoryBarriers.size());
		barriers.pMemoryBarriers = memoryBarriers.data();
	}

	void PipelineBarrierInfo::AddBufferBarriers(std::vector<VkBufferMemoryBarrier>& bufferBarriers)
	{
		barriers.bufferMemoryBarrierCount = static_cast<uint32_t>(bufferBarriers.size());
		barriers.pBufferMemoryBarriers = bufferBarriers.data();
	}

	void PipelineBarrierInfo::AddImageBarriers(std::vector<VkImageMemoryBarrier>& imageBarriers)
	{
		barriers.imageMemoryBarrierCount = static_cast<uint32_t>(imageBarriers.size());
		barriers.pImageMemoryBarriers = imageBarriers.data();
	}

	void AddPipelineBarrier(VkCommandBuffer cmdBuffer, const PipelineBarrierInfo& barrierInfo)
	{
		vkCmdPipelineBarrier(cmdBuffer,
			barrierInfo.src,
			barrierInfo.dst,
			0,
			barrierInfo.barriers.memoryBarrierCount,
			barrierInfo.barriers.pMemoryBarriers,
			barrierInfo.barriers.bufferMemoryBarrierCount,
			barrierInfo.barriers.pBufferMemoryBarriers,
			barrierInfo.barriers.imageMemoryBarrierCount,
			barrierInfo.barriers.pImageMemoryBarriers
		);
	}
}
			
			