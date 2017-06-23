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

#include <Windows.h>
#include <memory>
#include <array>

#include "Pass.h"
#include "Passes.h"
#include "..\ShadowMap.h"

class Scene;

namespace Renderer
{
  class Instance;
  class Surface;
  class ImageManager;
  class BufferManager;
  class FrameBufferManager;
  class ResourceManager;
  class UniformGrid;
  class RenderScene;

  class PassManager
  {
  public:
    PassManager(ImageManager* imageManager, Surface* surface);

    void RequestResources(ImageManager* imageManager, BufferManager* bufferManager, Surface* surface, UniformGrid* uniformGrid, RenderScene* renderScene);
    bool Create(HWND hwnd, Instance* instance, Surface* surface, ImageManager* imageManager);

    void UpdateBindings(Instance* instance, ImageManager* imageManager, BufferManager* bufferManager, Surface* surface);
    void OnReloadShaders(VkDevice device, ImageManager* imageManager, Surface* surface);
    void OnResize(VkDevice device, ImageManager* imageManager, BufferManager* bufferManager, uint32_t width, uint32_t height);
    void OnLoadScene(VkDevice device, Scene* scene, ImageManager* imageManager, BufferManager* bufferManager);

    void UpdateFrameData(Instance* instance, RenderScene* renderScene, Surface* surface, Scene* scene,
      ImageManager* imageManager, BufferManager* bufferManager);

    void Update(Instance* instance, BufferManager* bufferManager, float deltaTime);
    void Render(VkDevice device, QueueManager* queueManager, BufferManager* bufferManager,
      ImageManager* imageManager, Surface* surface, int currFrameIndex);

    void Release(VkDevice device);

    bool CreatePasses(HWND hwnd, VkDevice device, ImageManager* imageManager, Surface* surface);

    VkSemaphore GetPassFrameFinished() { return *frameFinished_; }

    //TODO
    void SetResources(RenderScene* renderScene);
    ShaderBindingManager* GetBindingManager() { return shaderBindingManager_.get(); }
  private:
    bool CreateManagers(Instance* instance, Surface* surface, ImageManager* imageManager);
    bool CreateShaderBindings(Instance* instance);

    std::shared_ptr<FrameBufferManager> frameBufferManager_;
    std::shared_ptr<RenderPassManager> renderPassManager_;
    std::shared_ptr<ShaderManager> shaderManager_;
    std::shared_ptr<ShaderBindingManager> shaderBindingManager_;

    std::array<std::unique_ptr<Pass>, PASS_MAX> passes_;

    int offscreenRenderTarget_;
    int raymarchingImage_;
    int depthImageIndex_ = -1;
    int noiseImageIndex_ = -1;
    int noiseMultipleChannels_ = -1;
		int shadowMapImageIndex_ = -1;
    VkSemaphore* frameFinished_ = nullptr;
  };
}