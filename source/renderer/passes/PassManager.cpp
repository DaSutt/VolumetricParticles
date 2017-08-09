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

#include "PassManager.h"

#include "..\scene\UniformGrid.h"
#include "..\scene\RenderScene.h"

#include "..\resources\ImageManager.h"
#include "..\resources\BufferManager.h"

#include "..\passResources\ShaderBindingManager.h"
#include "..\passResources\ShaderManager.h"
#include "..\passResources\RenderPassManager.h"
#include "..\passResources\FrameBufferManager.h"

#include "..\wrapper\Instance.h"
#include "..\wrapper\Surface.h"
#include "..\wrapper\Barrier.h"

#include "PostProcessPass.h"
#include "GuiPass.h"
#include "MeshPass.h"
#include "VolumePass.h"
#include "ShadowMapPass.h"

namespace Renderer
{
  PassManager::PassManager(ImageManager* imageManager, Surface* surface)
  {
    shaderManager_ = std::make_shared<ShaderManager>();
    shaderBindingManager_ = std::make_shared<ShaderBindingManager>();
    renderPassManager_ = std::make_shared<RenderPassManager>();
    frameBufferManager_ = std::make_shared<FrameBufferManager>(renderPassManager_.get(), imageManager,
      surface);

    passes_[PASS_SHADOW_MAP] = std::make_unique<ShadowMapPass>(shaderBindingManager_.get(), renderPassManager_.get());
    passes_[PASS_MESH] = std::make_unique<MeshPass>(shaderBindingManager_.get(), renderPassManager_.get());
    passes_[PASS_VOLUME] = std::make_unique<VolumePass>(shaderBindingManager_.get(), renderPassManager_.get());
    passes_[PASS_POSTPROCESS] = std::make_unique<PostProcessPass>(shaderBindingManager_.get(), renderPassManager_.get());
    passes_[PASS_GUI] = std::make_unique<GuiPass>(shaderBindingManager_.get(), renderPassManager_.get());
  }

  void PassManager::RequestResources(ImageManager* imageManager, BufferManager* bufferManager, Surface* surface, UniformGrid* uniformGrid, RenderScene* renderScene)
  {
    const auto& surfaceSize = surface->GetSurfaceSize();
		{
			ImageManager::ImageInfo imageInfo = {};
			imageInfo.extent = { surfaceSize.width, surfaceSize.height, 1 };
			imageInfo.format = imageManager->GetOffscreenImageFormat();
			imageInfo.type = ImageManager::IMAGE_RESIZE;
			imageInfo.sampler = ImageManager::SAMPLER_LINEAR;
			imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
			imageInfo.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			imageInfo.viewLayout = VK_IMAGE_LAYOUT_GENERAL;
			offscreenRenderTarget_ = imageManager->RequestImage(imageInfo);
		}
		{
			ImageManager::ImageInfo imageInfo = {};
			imageInfo.extent = { surfaceSize.width, surfaceSize.height, 1 };
			imageInfo.format = imageManager->GetDepthImageFormat();
			imageInfo.type = ImageManager::IMAGE_RESIZE;
			imageInfo.sampler = ImageManager::SAMPLER_LINEAR;
			imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			imageInfo.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageInfo.viewLayout = VK_IMAGE_LAYOUT_GENERAL;
			depthImageIndex_ = imageManager->RequestImage(imageInfo);
		}
		{
			ImageManager::ImageInfo imageInfo = {};
			const int resolutionScale = 1;
			imageInfo.extent = { surfaceSize.width / resolutionScale, surfaceSize.height / resolutionScale, 1 };
			imageInfo.format = imageManager->GetOffscreenImageFormat();
			imageInfo.type = ImageManager::IMAGE_RESIZE;
			imageInfo.sampler = ImageManager::SAMPLER_LINEAR;
			imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT;
			imageInfo.layout = VK_IMAGE_LAYOUT_GENERAL;
			imageInfo.viewLayout = VK_IMAGE_LAYOUT_GENERAL;
			raymarchingImage_ = imageManager->RequestImage(imageInfo);
		}
    ImageManager::Ref_ImageInfo noiseImageInfo = {};
    noiseImageInfo.arrayCount = 64;
    noiseImageInfo.format = VK_FORMAT_R8_UNORM;
    noiseImageInfo.imageType = ImageManager::IMAGE_FILE;
    noiseImageInfo.pool = ImageManager::MEMORY_POOL_CONSTANT;
    noiseImageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    noiseImageIndex_ = imageManager->Ref_RequestImage(noiseImageInfo, "..\\data\\noise\\HDR_L__X_.png");

		noiseImageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    noiseMultipleChannels_ = imageManager->Ref_RequestImage(noiseImageInfo, "..\\data\\noise\\HDR_RGBA__X_.png");

    renderScene->GetAdaptiveGrid()->SetImageIndices(raymarchingImage_, depthImageIndex_, 
			renderScene->GetShadowMap()->GetImageIndex(), noiseImageIndex_);
    
		shadowMapImageIndex_ = renderScene->GetShadowMap()->GetImageIndex();

		static_cast<MeshPass*>(passes_[PASS_MESH].get())->SetShadowMap(renderScene->GetShadowMap());
    static_cast<ShadowMapPass*>(passes_[PASS_SHADOW_MAP].get())->SetShadowMap(renderScene->GetShadowMap());
    static_cast<VolumePass*>(passes_[PASS_VOLUME].get())->SetShadowMap(renderScene->GetShadowMap(), imageManager);

    static_cast<MeshPass*>(passes_[PASS_MESH].get())->SetImageIndices(
      offscreenRenderTarget_, depthImageIndex_);

    static_cast<VolumePass*>(passes_[PASS_VOLUME].get())->SetAdaptiveGrid(renderScene->GetAdaptiveGrid());
    static_cast<VolumePass*>(passes_[PASS_VOLUME].get())->SetImageIndices(
      raymarchingImage_, depthImageIndex_, noiseImageIndex_);
    static_cast<PostProcessPass*>(passes_[PASS_POSTPROCESS].get())->SetImageIndices(
      offscreenRenderTarget_, raymarchingImage_, depthImageIndex_, noiseMultipleChannels_);

    for (auto& pass : passes_)
    {
      pass->RequestResources(imageManager, bufferManager, surface->GetImageCount());
    }
  }

  bool PassManager::Create(HWND hwnd, Instance* instance, Surface* surface, ImageManager* imageManager)
  {
    const auto& device = instance->GetDevice();

    if (!CreateManagers(instance, surface, imageManager)) { return false; }
    if (!CreateShaderBindings(instance)) { return false; }
    if (!CreatePasses(hwnd, device, imageManager, surface)) { return false; }

    printf("Pass Manager created\n");
    return true;
  }

  void PassManager::UpdateBindings(Instance* instance, ImageManager* imageManager, BufferManager* bufferManager, Surface* surface)
  {
    const auto& device = instance->GetDevice();

    shaderBindingManager_->UpdateDescriptorSets(device, imageManager, bufferManager);
  }

  void PassManager::OnReloadShaders(VkDevice device, ImageManager* imageManager, Surface* surface)
  {
    if (shaderManager_->ReLoad(device))
    {
      for (auto& pass : passes_)
      {
        pass->OnReloadShaders(device, shaderManager_.get());
      }
    }
  }

  void PassManager::OnResize(VkDevice device, ImageManager* imageManager, BufferManager* bufferManager, uint32_t width, uint32_t height)
  {
    shaderBindingManager_->UpdateDescriptorSets(device, imageManager, bufferManager);
    frameBufferManager_->OnResize(device, width, height);
    static_cast<GuiPass*>(passes_[PASS_GUI].get())->OnResize(width, height);
  }

  void PassManager::OnLoadScene(VkDevice device, Scene* scene, ImageManager* imageManager, BufferManager* bufferManager)
  {
    shaderBindingManager_->UpdateDescriptorSets(device, imageManager, bufferManager);
    shaderBindingManager_->OnLoadScene(device, scene);

		static_cast<VolumePass*>(passes_[PASS_VOLUME].get())->OnLoadScene(scene);
  }

  bool PassManager::CreateManagers(Instance* instance, Surface* surface, ImageManager* imageManager)
  {
    if (!renderPassManager_->CreateRenderPasses(instance, surface, imageManager))
    {
      printf("Render pass Manager creation failed\n");
      return false;
    }

    if (!shaderManager_->ReLoad(instance->GetDevice()))
    {
      printf("Shader manager reloading failed\n");
      return false;
    }

    return true;
  }

  void PassManager::UpdateFrameData(Instance* instance, RenderScene* renderScene, Surface* surface, Scene* scene, ImageManager* imageManager, BufferManager* bufferManager)
  {
    static_cast<ShadowMapPass*>(passes_[PASS_SHADOW_MAP].get())->UpdateBufferData(scene);
    static_cast<MeshPass*>(passes_[PASS_MESH].get())->UpdateBufferData(scene, renderScene);
    static_cast<VolumePass*>(passes_[PASS_VOLUME].get())->UpdateBufferData(surface, scene);
    static_cast<PostProcessPass*>(passes_[PASS_POSTPROCESS].get())->UpdateBufferData(scene, surface, renderScene);
  }

  void PassManager::Update(Instance* instance, BufferManager* bufferManager, float deltaTime)
  {
    static_cast<GuiPass*>(passes_[PASS_GUI].get())->Update(instance, bufferManager, deltaTime);
		static_cast<VolumePass*>(passes_[PASS_VOLUME].get())->Update(deltaTime);
  }

  void PassManager::Render(VkDevice device, QueueManager* queueManager, BufferManager* bufferManager,
    ImageManager* imageManager, Surface* surface, int currFameIndex)
  {
    {
      std::vector<VkImageMemoryBarrier> barrier;
      passes_[PASS_SHADOW_MAP]->Render(surface, frameBufferManager_.get(), queueManager, bufferManager,
        barrier, surface->GetImageAvailableSemaphore(), currFameIndex);

      passes_[PASS_MESH]->Render(surface, frameBufferManager_.get(), queueManager, bufferManager,
        barrier, passes_[PASS_SHADOW_MAP]->GetFinishedSemaphore(), currFameIndex);

			passes_[PASS_VOLUME]->Render(surface, frameBufferManager_.get(), queueManager, bufferManager,
        barrier, passes_[PASS_MESH]->GetFinishedSemaphore(), currFameIndex);

      passes_[PASS_POSTPROCESS]->Render(surface, frameBufferManager_.get(), queueManager, bufferManager,
        barrier, passes_[PASS_VOLUME]->GetFinishedSemaphore(), currFameIndex);
    }

    {
      auto barrier = imageManager->ReadToWriteBarrier({ offscreenRenderTarget_ });
      static_cast<PostProcessPass*>(passes_[PASS_GUI].get())->Render(surface, 
				frameBufferManager_.get(), queueManager, bufferManager,
				barrier, passes_[PASS_POSTPROCESS]->GetFinishedSemaphore(), currFameIndex);
    }
  }

  void PassManager::Release(VkDevice device)
  {
    shaderBindingManager_->Release(device);
    shaderManager_->Release(device);
    renderPassManager_->Release(device);
    frameBufferManager_->Release(device);

    for (auto& pass: passes_)
    {
      pass->Release(device);
    }
  }

  bool PassManager::CreatePasses(HWND hwnd, VkDevice device, ImageManager* imageManager, Surface* surface)
  {
    for (int pass = 0; pass < PASS_MAX; ++pass)
    {
      if (!passes_[pass]->Create(device, shaderManager_.get(), frameBufferManager_.get(), surface, imageManager))
      {
        printf("Creation of pass %d failed\n", pass);
        return false;
      }
    }

    static_cast<GuiPass*>(passes_[PASS_GUI].get())->SetWindow(hwnd);
    frameFinished_ = passes_[PASS_GUI]->GetFinishedSemaphore();

    return true;
  }

  void PassManager::SetResources(RenderScene* renderScene)
  {
    static_cast<ShadowMapPass*>(passes_[PASS_SHADOW_MAP].get())->SetRenderScene(renderScene);
    static_cast<MeshPass*>(passes_[PASS_MESH].get())->SetRenderScene(renderScene);
  }

  bool PassManager::CreateShaderBindings(Instance* instance)
  {
    if (!shaderBindingManager_->Create(instance))
    {
      return false;
    }
    return true;
  }
}