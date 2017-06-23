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

#include "RenderPassManager.h"

#include <vector>

#include "..\wrapper\Instance.h"
#include "..\wrapper\Surface.h"

#include "..\resources\ImageManager.h"

namespace Renderer
{
  bool RenderPassManager::CreateRenderPasses(Instance* instance, Surface* surface, ImageManager* imageManager)
  {
    if (instance == nullptr || surface == nullptr || imageManager == nullptr)
    {
      printf("Instance, Surface and ImageManager need to be created before PassManager\n");
      return false;
    }

    renderPasses_.resize(GRAPHIC_PASS_MAX, VK_NULL_HANDLE);
    auto graphicPass = GRAPHIC_PASS_MESH;
    {
      VkAttachmentDescription colorAttachement = {};
      colorAttachement.format = imageManager->GetOffscreenImageFormat();
      colorAttachement.samples = VK_SAMPLE_COUNT_1_BIT;
      colorAttachement.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      colorAttachement.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      colorAttachement.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      colorAttachement.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      colorAttachement.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      colorAttachement.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

      VkAttachmentDescription depthAttachement = {};
      depthAttachement.format = imageManager->GetDepthImageFormat();
      depthAttachement.samples = VK_SAMPLE_COUNT_1_BIT;
      depthAttachement.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      depthAttachement.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      depthAttachement.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      depthAttachement.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      depthAttachement.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
      depthAttachement.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      std::vector<VkAttachmentDescription> attachements = { colorAttachement, depthAttachement };

      VkAttachmentReference colorAttachmentRef = {};
      colorAttachmentRef.attachment = 0;
      colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

      VkAttachmentReference depthAttachementRef = {};
      depthAttachementRef.attachment = 1;
      depthAttachementRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

      VkSubpassDescription subpass = {};
      subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
      subpass.colorAttachmentCount = 1;
      subpass.pColorAttachments = &colorAttachmentRef;
      subpass.pDepthStencilAttachment = &depthAttachementRef;

      VkRenderPassCreateInfo renderPassInfo = {};
      renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
      renderPassInfo.attachmentCount = static_cast<uint32_t>(attachements.size());
      renderPassInfo.pAttachments = attachements.data();
      renderPassInfo.subpassCount = 1;
      renderPassInfo.pSubpasses = &subpass;

      const auto result = vkCreateRenderPass(instance->GetDevice(), &renderPassInfo,
        nullptr, &renderPasses_[GRAPHIC_PASS_MESH]);
      if (result != VK_SUCCESS)
      {
        printf("Failed creating render pass, %d\n", result);
        return false;
      }
    }

    graphicPass = GRAPHIC_PASS_GUI;
    {
      VkAttachmentDescription colorAttachement = {};
      colorAttachement.format = surface->GetImageFormat();
      colorAttachement.samples = VK_SAMPLE_COUNT_1_BIT;
      colorAttachement.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
      colorAttachement.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      colorAttachement.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      colorAttachement.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      colorAttachement.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      colorAttachement.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

      std::vector<VkAttachmentDescription> attachements = { colorAttachement };

      VkAttachmentReference colorAttachmentRef = {};
      colorAttachmentRef.attachment = 0;
      colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

      VkSubpassDescription subpass = {};
      subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
      subpass.colorAttachmentCount = 1;
      subpass.pColorAttachments = &colorAttachmentRef;

      VkRenderPassCreateInfo renderPassInfo = {};
      renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
      renderPassInfo.attachmentCount = static_cast<uint32_t>(attachements.size());
      renderPassInfo.pAttachments = attachements.data();
      renderPassInfo.subpassCount = 1;
      renderPassInfo.pSubpasses = &subpass;

      const auto result = vkCreateRenderPass(instance->GetDevice(), &renderPassInfo,
        nullptr, &renderPasses_[graphicPass]);
      if (result != VK_SUCCESS)
      {
        printf("Failed creating render pass, %d\n", result);
        return false;
      }
    }

    graphicPass = GRAPHIC_PASS_POSTPROCESS;
    {
      VkAttachmentDescription colorAttachement = {};
      colorAttachement.format = surface->GetImageFormat();
      colorAttachement.samples = VK_SAMPLE_COUNT_1_BIT;
      colorAttachement.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      colorAttachement.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      colorAttachement.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      colorAttachement.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      colorAttachement.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      colorAttachement.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

      std::vector<VkAttachmentDescription> attachements = { colorAttachement };

      VkAttachmentReference colorAttachmentRef = {};
      colorAttachmentRef.attachment = 0;
      colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

      VkSubpassDescription subpass = {};
      subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
      subpass.colorAttachmentCount = 1;
      subpass.pColorAttachments = &colorAttachmentRef;

      VkRenderPassCreateInfo renderPassInfo = {};
      renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
      renderPassInfo.attachmentCount = static_cast<uint32_t>(attachements.size());
      renderPassInfo.pAttachments = attachements.data();
      renderPassInfo.subpassCount = 1;
      renderPassInfo.pSubpasses = &subpass;

      const auto result = vkCreateRenderPass(instance->GetDevice(), &renderPassInfo,
        nullptr, &renderPasses_[graphicPass]);
      if (result != VK_SUCCESS)
      {
        printf("Failed creating render pass, %d\n", result);
        return false;
      }
    }

    graphicPass = GRAPHIC_PASS_POSTPROCESS_DEBUG;
    {
      VkAttachmentDescription colorAttachement = {};
      colorAttachement.format = surface->GetImageFormat();
      colorAttachement.samples = VK_SAMPLE_COUNT_1_BIT;
      colorAttachement.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
      colorAttachement.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      colorAttachement.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      colorAttachement.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      colorAttachement.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      colorAttachement.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

      VkAttachmentDescription depthAttachement = {};
      depthAttachement.format = imageManager->GetDepthImageFormat();
      depthAttachement.samples = VK_SAMPLE_COUNT_1_BIT;
      depthAttachement.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
      depthAttachement.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      depthAttachement.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      depthAttachement.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      depthAttachement.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      depthAttachement.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

      VkAttachmentDescription attachements[] = { colorAttachement, depthAttachement };

      VkAttachmentReference colorAttachmentRef = {};
      colorAttachmentRef.attachment = 0;
      colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

      VkAttachmentReference depthAttachementRef = {};
      depthAttachementRef.attachment = 1;
      depthAttachementRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

      VkSubpassDescription subpass = {};
      subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
      subpass.colorAttachmentCount = 1;
      subpass.pColorAttachments = &colorAttachmentRef;
      subpass.pDepthStencilAttachment = &depthAttachementRef;

      VkRenderPassCreateInfo renderPassInfo = {};
      renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
      renderPassInfo.attachmentCount = 2;
      renderPassInfo.pAttachments = attachements;
      renderPassInfo.subpassCount = 1;
      renderPassInfo.pSubpasses = &subpass;

      const auto result = vkCreateRenderPass(instance->GetDevice(), &renderPassInfo,
        nullptr, &renderPasses_[graphicPass]);
      if (result != VK_SUCCESS)
      {
        printf("Failed creating render pass, %d\n", result);
        return false;
      }
    }

    graphicPass = GRAPHIC_PASS_SHADOW_MAP;
    {
      VkAttachmentDescription depthAttachement = {};
      depthAttachement.format = imageManager->GetDepthImageFormat();
      depthAttachement.samples = VK_SAMPLE_COUNT_1_BIT;
      depthAttachement.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      depthAttachement.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      depthAttachement.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      depthAttachement.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      depthAttachement.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
      depthAttachement.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      VkAttachmentDescription attachements[] = { depthAttachement };

      VkAttachmentReference depthAttachementRef = {};
      depthAttachementRef.attachment = 0;
      depthAttachementRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

      VkSubpassDescription subpass = {};
      subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
      subpass.colorAttachmentCount = 0;
      subpass.pDepthStencilAttachment = &depthAttachementRef;

      VkRenderPassCreateInfo renderPassInfo = {};
      renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
      renderPassInfo.attachmentCount = 1;
      renderPassInfo.pAttachments = attachements;
      renderPassInfo.subpassCount = 1;
      renderPassInfo.pSubpasses = &subpass;

      const auto result = vkCreateRenderPass(instance->GetDevice(), &renderPassInfo,
        nullptr, &renderPasses_[graphicPass]);
      if (result != VK_SUCCESS)
      {
        printf("Failed creating render pass, %d\n", result);
        return false;
      }
    }


    return true;
  }

  void RenderPassManager::Release(VkDevice device)
  {
    for (auto& rp : renderPasses_)
    {
      vkDestroyRenderPass(device, rp, nullptr);
    }
  }
}