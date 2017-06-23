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

namespace Renderer
{
  enum DepthStencilState
  {
    DEPTH_WRITE,
    DEPTH_DISABLED,
    DEPTH_READ
  };

  static const VkPipelineDepthStencilStateCreateInfo g_depthStencilStates[] =
  {
    {
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      nullptr,            //pNext
      0,                  //flags
      VK_TRUE,            //depthTestEnable
      VK_TRUE,            //depthWriteEnable
      VK_COMPARE_OP_LESS, //depthCompareOp
      VK_FALSE,           //depthBoundsTestEnable
      VK_FALSE,           //stencilTestEnable
      {},                 //front
      {},                 //back
      0.0f,               //minDepthBounds
      0.0f                //maxDepthBounds
    },
    {
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      nullptr,            //pNext
      0,                  //flags
      VK_FALSE,           //depthTestEnable
      VK_FALSE,           //depthWriteEnable
      VK_COMPARE_OP_NEVER,//depthCompareOp
      VK_FALSE,           //depthBoundsTestEnable
      VK_FALSE,           //stencilTestEnable
      {},                 //front
      {},                 //back
      0.0f,               //minDepthBounds
      0.0f                //maxDepthBounds
    },
    {
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      nullptr,            //pNext
      0,                  //flags
      VK_TRUE,            //depthTestEnable
      VK_FALSE,           //depthWriteEnable
      VK_COMPARE_OP_LESS_OR_EQUAL, //depthCompareOp
      VK_FALSE,           //depthBoundsTestEnable
      VK_FALSE,           //stencilTestEnable
      {},                 //front
      {},                 //back
      0.0f,               //minDepthBounds
      0.0f                //maxDepthBounds
    }
  };
}