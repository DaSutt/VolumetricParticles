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

namespace Renderer
{
  class ViewPortState
  {
  public:
    enum State
    {
      VIEWPORT_STATE_FULL,
      VIEWPORT_STATE_UNDEFINED,
      VIEWPORT_STATE_MAX
    };

    static void Create(int width, int height);
    static const auto& GetState(State state) { return viewportState_[state]; }
  private:
    enum Viewports
    {
      VIEWPORT_FULL,
      VIEWPORT_MAX
    };
    enum Scissors
    {
      SCISSOR_FULL,
      SCISSOR_MAX
    };

    static std::array<VkPipelineViewportStateCreateInfo, VIEWPORT_STATE_MAX> viewportState_;
    static std::vector<VkViewport> viewports_;
    static std::vector<VkRect2D> scissors_;
  };
}