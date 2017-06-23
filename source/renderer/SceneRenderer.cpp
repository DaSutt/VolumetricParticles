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

#include "SceneRenderer.h"

#include "wrapper\Instance.h"
#include "pipelineState\PipelineState.h"

#include "..\scene\Scene.h"

#include "wrapper\QueryPool.h"

namespace Renderer
{
  SceneRenderer::SceneRenderer()
  {
    renderScene_ = std::make_shared<RenderScene>();
  }

  bool SceneRenderer::Init(Scene* scene, HWND window, int width, int height)
  {
    InitPipelineState(width, height);
    uniformGrid_ = std::make_shared<UniformGrid>(scene->GetGrid());

    bufferManager_ = std::make_shared<BufferManager>(&instance_);

    if (
      !instance_.Create() ||
      !queueManager_.Create(&instance_) ||
      !surface_.Create(window, &instance_, width, height) ||
      !bufferManager_->Create(&surface_) ||
      !CreateFrameFences()
      )
    {
      printf("Scene Renderer Init failed\n");
      return false;
    }

    imageManager_ = std::make_shared<ImageManager>(&instance_);
    if (!imageManager_->Create()) { return false; }

    if (!uniformGrid_->Init(imageManager_.get())) { return false; }
    renderScene_->RequestResources(bufferManager_.get(), imageManager_.get(), surface_.GetImageCount());

    passManager_ = std::make_unique<PassManager>(imageManager_.get(), &surface_);
    passManager_->SetResources(renderScene_.get());

    passManager_->RequestResources(imageManager_.get(), bufferManager_.get(), &surface_, uniformGrid_.get(), renderScene_.get());
    bufferManager_->BindBuffers(BufferManager::MEMORY_CONSTANT);
		bufferManager_->BindBuffers(BufferManager::MEMORY_SCENE);
		bufferManager_->BindBuffers(BufferManager::MEMORY_GRID);
    if (!imageManager_->InitializeImages(&queueManager_, false)) { return false; }
    if(!imageManager_->InitData(&queueManager_, instance_.GetDevice(), instance_.GetPhysicalDevice()))
    {
      printf("init image data failed\n");
      return false;
    }
    imageManager_->Ref_UploadImages(bufferManager_.get(), &queueManager_);

    if (!passManager_->Create(window, &instance_, &surface_, imageManager_.get()))
    {
      printf("Pass Manager creation failed\n");
      return false;
    }
    passManager_->UpdateBindings(&instance_, imageManager_.get(), bufferManager_.get(), &surface_);

		auto& queryPool = Wrapper::QueryPool::GetInstance();
		queryPool.CreatePool(instance_.GetDevice(), instance_.GetPhysicalDevice(), surface_.GetImageCount());

    printf("Scene renderer initialized successfully\n");
    return true;
  }

  void SceneRenderer::Release()
  {
    const auto& device = instance_.GetDevice();
    WaitForAllFences();
    ReleaseFences();

		auto& queryPool = Wrapper::QueryPool::GetInstance();
		queryPool.DestroyPool(device);
    passManager_->Release(device);

    bufferManager_->Release(device);
    imageManager_->Release(device);
    surface_.Release(instance_.GetInstance(), device);
    instance_.Release();
  }

  void SceneRenderer::Update(Scene* scene, float deltaTime)
  {
    const auto& device = instance_.GetDevice();
    vkWaitForFences(device, 1, &frameFences_[frameIndex_], VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &frameFences_[frameIndex_]);

    renderScene_->Update(scene, bufferManager_.get(), imageManager_.get(), &surface_, frameIndex_);
    if (renderScene_->GetAdaptiveGrid()->Resized())
    {
      passManager_->UpdateBindings(&instance_, imageManager_.get(), bufferManager_.get(), &surface_);
    }
    bufferManager_->SetActiveBuffer(frameIndex_);

    bufferManager_->UpdateConstantBuffers(frameIndex_);
    passManager_->UpdateFrameData(&instance_, renderScene_.get(), &surface_, scene, imageManager_.get(), bufferManager_.get());
    passManager_->Update(&instance_, bufferManager_.get(), deltaTime);

    uniformGrid_->UpdateData(bufferManager_.get());
    bufferManager_->PerformCopies(&queueManager_);
  }

  void SceneRenderer::Render(Scene* scene)
  {
    const auto& device = instance_.GetDevice();
    const auto& physicalDevice = instance_.GetPhysicalDevice();
    surface_.GetNextImage(device);

    passManager_->Render(device, &queueManager_, bufferManager_.get(), imageManager_.get(), &surface_, frameIndex_);

    std::vector<VkSemaphore> signalSemaphores =
    {
      passManager_->GetPassFrameFinished()
    };
    surface_.Present(&queueManager_, signalSemaphores);

    vkQueueSubmit(queueManager_.GetQueue(QueueManager::QUEUE_GRAPHICS), 0, nullptr, frameFences_[frameIndex_]);
    frameIndex_ = (frameIndex_ + 1) % surface_.GetImageCount();
  }

  void SceneRenderer::OnReloadShaders()
  {
    WaitForAllFences();
    const auto& device = instance_.GetDevice();

    passManager_->OnReloadShaders(device, imageManager_.get(), &surface_);
  }

  bool SceneRenderer::OnLoadScene(Scene* scene)
  {
    WaitForAllFences();

    const auto device = instance_.GetDevice();
    const auto physicalDevice = instance_.GetPhysicalDevice();

    imageManager_->Ref_ReleaseScene();

    bufferManager_->OnLoadScene();
    renderScene_->OnLoadScene(scene, bufferManager_.get(), imageManager_.get(), &queueManager_, surface_.GetImageCount());
    imageManager_->Ref_OnLoadScene(bufferManager_.get(), &queueManager_);

    SetResources(scene);
    passManager_->OnLoadScene(device, scene, imageManager_.get(), bufferManager_.get());
    renderScene_->UpdateBindings(device, passManager_->GetBindingManager(), imageManager_.get());
    uniformGrid_->OnLoadScene(device, scene->GetGrid(), bufferManager_.get(), passManager_->GetBindingManager(), scene);

    printf("Scene loaded\n");

    return true;
  }

  void SceneRenderer::OnResize(uint32_t width, uint32_t height)
  {
    WaitForAllFences();
    if (!imageManager_->OnResize(&queueManager_, width, height))
    {
      printf("Image resizing failed\n");
    }
    surface_.OnResize(&instance_, width, height);

    passManager_->OnResize(instance_.GetDevice(), imageManager_.get(), bufferManager_.get(), width, height);
  }

  bool SceneRenderer::CreateFrameFences()
  {
    const auto backBufferCount = surface_.GetImageCount();
    for (uint32_t i = 0; i < backBufferCount; ++i)
    {
      VkFenceCreateInfo fenceCreateInfo = {};
      fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

      VkFence fence = VK_NULL_HANDLE;
      const auto result = vkCreateFence(instance_.GetDevice(), &fenceCreateInfo, nullptr, &fence);
      if (result != VK_SUCCESS)
      {
        printf("Failed creating frame fence, %d\n", result);
        return false;
      }
      frameFences_.push_back(fence);
      }
    return true;
  }

  void SceneRenderer::WaitForAllFences()
  {
    const auto& device = instance_.GetDevice();
    vkWaitForFences(device, static_cast<uint32_t>(frameFences_.size()), frameFences_.data(), VK_TRUE, UINT64_MAX);
  }

  void SceneRenderer::ReleaseFences()
  {
    for (auto& fence : frameFences_)
    {
      vkDestroyFence(instance_.GetDevice(), fence, nullptr);
    }
  }

  void SceneRenderer::SetResources(Scene* scene)
  {
  }
}