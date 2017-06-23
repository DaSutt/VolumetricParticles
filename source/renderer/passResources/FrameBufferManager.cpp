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

#include "FrameBufferManager.h"

#include "RenderPassManager.h"

#include "..\resources\ImageManager.h"

#include "..\wrapper\Surface.h"

namespace Renderer
{
  FrameBufferManager::Attachement::Attachement() :
    imageIndex{ -1},
    source{IMAGE_SOURCE_NONE},
    arrayOffset{0},
    useArray{false}
  {}

  FrameBufferManager::Attachement::Attachement(int image, ImageSource source) :
    imageIndex{ image },
    source{ source },
    arrayOffset{ 0 },
    useArray{ false }
  {}

  FrameBufferManager::Attachement::Attachement(int image, ImageSource source, int offset, bool array) :
    imageIndex{ image },
    source{ source },
    arrayOffset{ offset },
    useArray{ array }
  {}

  FrameBufferManager::Info::Info() :
    layers{1},
    refactoring{false}
  {}

  FrameBufferManager::FrameBufferManager(RenderPassManager* renderPassManager, ImageManager* imageManager, Surface* surface) :
    imageManager_{imageManager},
    surface_{surface},
    renderPassManager_{renderPassManager}
  {}

  int FrameBufferManager::RequestFrameBuffer(VkDevice device, const Info& info)
  {
    auto frameBuffer = CreateFrameBuffer(device, info);
    if (frameBuffer == VK_NULL_HANDLE)
    {
      return -1;
    }

    const int index = static_cast<int>(frameBuffers_.size());
    frameBuffers_.push_back(frameBuffer);
    frameBufferInfos_.push_back(info);
    return index;
  }

  void FrameBufferManager::Release(VkDevice device)
  {
    for (auto& fb : frameBuffers_)
    {
      vkDestroyFramebuffer(device, fb, nullptr);
    }
  }

  void FrameBufferManager::OnResize(VkDevice device, uint32_t width, uint32_t height)
  {
    for (size_t i = 0; i < frameBufferInfos_.size(); ++i)
    {
      if (frameBufferInfos_[i].resize)
      {
        vkDestroyFramebuffer(device, frameBuffers_[i], nullptr);
        frameBufferInfos_[i].width = width;
        frameBufferInfos_[i].height = height;

        auto frameBuffer = CreateFrameBuffer(device, frameBufferInfos_[i]);
        if (frameBuffer == VK_NULL_HANDLE)
        {
          printf("Frame buffer resizing failed\n");
          return;
        }

        frameBuffers_[i] = frameBuffer;
      }
    }
  }

  VkFramebuffer FrameBufferManager::CreateFrameBuffer(VkDevice device, const Info& info)
  {
    std::vector<VkImageView> viewAttachements;
    for (auto attachement : info.imageAttachements)
    {
      if (attachement.source == IMAGE_SOURCE_MANAGER)
      {
        if (info.refactoring)
        {
          if (attachement.useArray)
          {
            viewAttachements.push_back(imageManager_->Ref_GetArrayImageView(
              attachement.imageIndex, attachement.arrayOffset
            ));
          }
          else
          {
            viewAttachements.push_back(imageManager_->Ref_GetImageView(attachement.imageIndex));
          }
        }
        else
        {
          viewAttachements.push_back(imageManager_->GetImageView(attachement.imageIndex));
        }
      }
      else
      {
        viewAttachements.push_back(surface_->GetImageView(attachement.imageIndex));
      }
    }

    VkFramebufferCreateInfo framebufferInfo = {};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPassManager_->GetRenderPass(info.pass);
    framebufferInfo.attachmentCount = static_cast<uint32_t>(viewAttachements.size());
    framebufferInfo.pAttachments = viewAttachements.data();
    framebufferInfo.width = info.width;
    framebufferInfo.height = info.height;
    framebufferInfo.layers = info.layers;

    VkFramebuffer frameBuffer = VK_NULL_HANDLE;
    const auto result = vkCreateFramebuffer(device, &framebufferInfo, nullptr,
      &frameBuffer);
    if (result != VK_SUCCESS)
    {
      printf("Failed creating frame buffer, %d\n", result);
    }
    return frameBuffer;
  }
}