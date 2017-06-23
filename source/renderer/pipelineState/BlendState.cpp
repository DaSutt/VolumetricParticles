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

#include "BlendState.h"

namespace Renderer
{
  VkPipelineColorBlendAttachmentState BlendState::blendAttachements_[BLEND_ATTACHEMENT_MAX];
  VkPipelineColorBlendStateCreateInfo BlendState::blendStates_[BLEND_MAX];

  VkPipelineColorBlendStateCreateInfo* BlendState::GetCreateInfo(State state)
  {
    static bool initialized = false;
    if (!initialized)
    {
      Create();
    }
    return &blendStates_[state];
  }

  void BlendState::Create()
  {
    SetBlendAttachements();
    SetBlendStates();
  }

  void BlendState::SetBlendAttachements()
  {
    auto attachementType = BLEND_ATTACHEMENT_DISABLE;
    {
      auto& attachement = blendAttachements_[attachementType];
      attachement = {};
      attachement.blendEnable = VK_FALSE;
      attachement.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    }
    attachementType = BLEND_ATTACHEMENT_ALPHA;
    {
      auto& attachement = blendAttachements_[attachementType];
      attachement.blendEnable = VK_TRUE;
      attachement.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
      attachement.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      attachement.colorBlendOp = VK_BLEND_OP_ADD;
      attachement.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
      attachement.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
      attachement.alphaBlendOp = VK_BLEND_OP_ADD;
      attachement.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    }
    attachementType = BLEND_ATTACHEMENT_GUI;
    {
      auto& attachement = blendAttachements_[attachementType];
      attachement.blendEnable = VK_TRUE;
      attachement.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
      attachement.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      attachement.colorBlendOp = VK_BLEND_OP_ADD;
      attachement.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      attachement.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
      attachement.alphaBlendOp = VK_BLEND_OP_ADD;
      attachement.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    }
  }

  void BlendState::SetBlendStates()
  {
    {
      auto& state = blendStates_[BLEND_DISABLED];
      state = {};
      state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
      state.logicOpEnable = VK_FALSE;
      state.attachmentCount = 1;
      state.pAttachments = &blendAttachements_[BLEND_ATTACHEMENT_DISABLE];
    }
    {
      auto& state = blendStates_[BLEND_ALPHA];
      state = {};
      state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
      state.attachmentCount = 1;
      state.pAttachments = &blendAttachements_[BLEND_ATTACHEMENT_ALPHA];
      state.blendConstants[0] = 1.0f;
      state.blendConstants[1] = 1.0f;
      state.blendConstants[2] = 1.0f;
      state.blendConstants[3] = 1.0f;
    }
    {
      auto& state = blendStates_[BLEND_GUI];
      state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
      state.attachmentCount = 1;
      state.pAttachments = &blendAttachements_[BLEND_ATTACHEMENT_GUI];
    }
  }
}