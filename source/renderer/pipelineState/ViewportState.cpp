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

#include "ViewportState.h"

namespace Renderer
{
  std::array<VkPipelineViewportStateCreateInfo, ViewPortState::VIEWPORT_STATE_MAX> ViewPortState::viewportState_;
  std::vector<VkViewport> ViewPortState::viewports_;
  std::vector<VkRect2D> ViewPortState::scissors_;

  void ViewPortState::Create(int width, int height)
  {
    viewports_.resize(VIEWPORT_MAX);
    scissors_.resize(SCISSOR_MAX);

    auto& vp = viewports_[VIEWPORT_FULL];
    vp = {};
    vp.x = 0.0f;
    vp.y = 0.0f;
    vp.width = static_cast<float>(width);
    vp.height = static_cast<float>(height);
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;

    auto& sc = scissors_[SCISSOR_FULL];
    sc = {};
    sc.offset = { 0,0 };
    sc.extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

    auto state = VIEWPORT_STATE_FULL;
    {
      viewportState_[state] = {};
      viewportState_[state].sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
      viewportState_[state].viewportCount = 1;
      viewportState_[state].pViewports = &viewports_[VIEWPORT_FULL];
      viewportState_[state].scissorCount = 1;
      viewportState_[state].pScissors = &scissors_[SCISSOR_FULL];
    }
    state = VIEWPORT_STATE_UNDEFINED;
    {
      viewportState_[state] = {};
      viewportState_[state].sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
      viewportState_[state].viewportCount = 1;
      viewportState_[state].scissorCount = 1;
    }
  }
}