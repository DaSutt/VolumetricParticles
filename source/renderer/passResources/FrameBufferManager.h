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

#include "..\passes\Passes.h"

namespace Renderer
{
  class RenderPassManager;
  class ImageManager;
  class Surface;

  class FrameBufferManager
  {
  public:
    enum ImageSource
    {
      IMAGE_SOURCE_MANAGER,
      IMAGE_SOURCE_SWAPCHAIN,
      IMAGE_SOURCE_NONE
    };

    struct Attachement
    {
      int imageIndex;
      ImageSource source;
      int arrayOffset;
      bool useArray;
      Attachement();
      Attachement(int image, ImageSource source);
      Attachement(int image, ImageSource source, int offset, bool array);
    };

    struct Info
    {
      std::vector<Attachement> imageAttachements;
      GraphicPassType pass;
      bool resize;
      uint32_t width;
      uint32_t height;
      uint32_t layers;
      bool refactoring;
      Info();
    };

    FrameBufferManager(RenderPassManager* renderPassManager, ImageManager* imageManager,
      Surface* surface);

    int RequestFrameBuffer(VkDevice device, const Info& info);

    void OnResize(VkDevice device, uint32_t width, uint32_t height);
    void Release(VkDevice device);

    VkFramebuffer GetFrameBuffer(int index) { return frameBuffers_[index]; }
  private:
    VkFramebuffer CreateFrameBuffer(VkDevice device, const Info& info);

    ImageManager* imageManager_;
    Surface* surface_;
    RenderPassManager* renderPassManager_;

    std::vector<VkFramebuffer> frameBuffers_;
    std::vector<Info> frameBufferInfos_;
  };
}