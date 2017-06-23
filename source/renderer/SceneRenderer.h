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

#include <memory>

#include "wrapper\Instance.h"
#include "wrapper\Surface.h"

#include "resources\QueueManager.h"
#include "resources\ImageManager.h"
#include "resources\BufferManager.h"

#include "passes\PassManager.h"

#include "scene\UniformGrid.h"
#include "scene\RenderScene.h"


class Scene;

namespace Renderer
{
  class SceneRenderer
  {
  public:
    SceneRenderer();
    bool Init(Scene* scene, HWND window, int width, int height);
    void Release();

    void Update(Scene* scene, float deltaTime);
    void Render(Scene* scene);

    void OnReloadShaders();
    bool OnLoadScene(Scene* scene);
    void OnResize(uint32_t width, uint32_t height);

    //TODO
    void SetResources(Scene* scene);
  private:
    bool CreateFrameFences();
    void WaitForAllFences();
    void ReleaseFences();

    Instance instance_;
    Surface surface_;

    QueueManager queueManager_;
    std::shared_ptr<ImageManager> imageManager_;
    std::shared_ptr<BufferManager> bufferManager_;
    std::shared_ptr<UniformGrid> uniformGrid_;
    std::shared_ptr<RenderScene> renderScene_;

    std::unique_ptr<PassManager> passManager_;

    std::vector<VkFence> frameFences_;
    int frameIndex_ = 0;
  };
}