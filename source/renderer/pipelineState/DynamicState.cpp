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

#include "DynamicState.h"

namespace Renderer
{
  std::vector<VkDynamicState> DynamicState::dynamicStates_[DYNAMIC_MAX];
  VkPipelineDynamicStateCreateInfo DynamicState::dynamicCreateInfo_[DYNAMIC_MAX];

  VkPipelineDynamicStateCreateInfo* DynamicState::GetCreateInfo(State state)
  {
    static bool initialized = false;
    if (!initialized)
    {
      Create();
    }
    return &dynamicCreateInfo_[state];
  }

  void DynamicState::Create()
  {
    auto state = DYNAMIC_VIEWPORT_SCISSOR;
    {
      dynamicStates_[state] =
      {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
      };

      dynamicCreateInfo_[state] = {};
      dynamicCreateInfo_[state].sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
      dynamicCreateInfo_[state].dynamicStateCount = static_cast<uint32_t>(dynamicStates_[state].size());
      dynamicCreateInfo_[state].pDynamicStates = dynamicStates_[state].data();
    }
  }
}