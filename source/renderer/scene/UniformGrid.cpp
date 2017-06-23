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

#include "UniformGrid.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm\gtx\transform.hpp>

#include "..\resources\ImageManager.h"
#include "..\resources\BufferManager.h"

#include "..\passResources\ShaderBindingManager.h"
#include "..\..\scene\grid\Grid.h"
#include "..\..\scene\Scene.h"

namespace Renderer
{
  UniformGrid::UniformGrid(Grid* sceneGrid) :
    sceneGrid_ {sceneGrid}
  {}

  //Create the grid image3d
  bool UniformGrid::Init(ImageManager* imageManager)
  {
    const auto& resolution = sceneGrid_->GetResolution();

    ImageManager::ImageInfo imageInfo = {};
    imageInfo.extent = { resolution.x, resolution.y, resolution.z };
    imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    imageInfo.layout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo.type = ImageManager::IMAGE_NO_POOL;
    imageInfo.sampler = ImageManager::SAMPLER_GRID;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    inScatteringExtinctionGrid_ = imageManager->RequestImage(imageInfo);
    if (inScatteringExtinctionGrid_ == -1)
    {
      printf("Failed creating in scattering and extinction grid\n");
      return false;
    }
    emissionPhaseFactorGrid_ = imageManager->RequestImage(imageInfo);
    if (emissionPhaseFactorGrid_ == -1)
    {
      printf("Failed creating emission phase factor grid\n");
      return false;
    }

    return true;
  }

  //Copy data from particle systems into buffers
  void UniformGrid::UpdateData(BufferManager* bufferManager)
  {
    for (auto& particleSystem : particleSystems_)
    {
      particleSystem.Update(bufferManager);
    }
    for (auto& particleSystem : particleSystems_)
    {
      particleSystem.FinishUpdate(bufferManager);
    }
  }

  void UniformGrid::OnLoadScene(VkDevice device, Grid* sceneGrid, BufferManager* bufferManager, ShaderBindingManager* bindingManager, Scene* scene)
  {
    sceneGrid_ = sceneGrid;

    const auto& min = sceneGrid_->GetMin();
    const auto& max = sceneGrid_->GetMax();

    const auto scale = max - min;
    texTransform_ = glm::translate(glm::scale(glm::mat4(), 1.0f / scale), -min);;

    particleSystems_.clear();
    /*const auto& sceneParticleSystems = scene->GetParticleSystems();
    std::vector<VkDescriptorBufferInfo> bufferInfos;
    for (auto& system : sceneParticleSystems.data())
    {
      particleSystems_.push_back({});
      particleSystems_.back().Init(system.get(), bufferManager);

      const auto bufferIndex = particleSystems_.back().GetBufferIndex();
      bufferInfos.push_back(*bufferManager->GetDescriptorInfo(bufferIndex));
    }*/

    //bindingManager->UpdateDescriptorSets(device, SUBPASS_VOLUME_MEDIA_PARTICLES, bufferInfos, {});

    sceneLoaded_ = true;
  }

  //Update grid with particle data
  void UniformGrid::UpdateGrid(VkCommandBuffer& commandBuffer)
  {
    const auto dispatchSize = sceneGrid_->GetResolution();
    vkCmdDispatch(commandBuffer, dispatchSize.x / 8, dispatchSize.y / 8, dispatchSize.z / 8);
  }

  void UniformGrid::UpdateGridParticles(VkDevice device, VkCommandBuffer& commandBuffer, ShaderBindingManager* bindingManager,
    BufferManager* bufferManager, int shaderBinding)
  {
    //const auto subpass = SUBPASS_VOLUME_MEDIA_PARTICLES;
    const auto& particleSystems = sceneGrid_->GetParticleSystemInfo();
    for (size_t i = 0; i < particleSystems.size(); ++i)
    {
      auto dispatchSize = particleSystems[i].max - particleSystems[i].min;
      if (dispatchSize.x == 0)
      {
        dispatchSize.x = 1;
      }
      if (dispatchSize.y == 0)
      {
        dispatchSize.y = 1;
      }
      if (dispatchSize.z == 0)
      {
        dispatchSize.z = 1;
      }

      //PerParticleSystemData data;
      ////TODO use correct cellsize
      //data.cellSize = sceneGrid_->GetCellSize().x;
      //data.toWorldOffset = glm::ivec3(sceneGrid_->GetResolution()) / 2;
      //data.offset = particleSystems[i].min;
      //data.activeParticles = particleSystems_[i].GetActiveParticles();

      //const auto bufferIndex = particleSystems_[i].GetBufferIndex();
      //vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, bindingManager->GetPipelineLayout(subpass),
      //  1, 1, bindingManager->GetDescriptorSet(subpass, static_cast<int>(i)), 0, nullptr);

      ///*vkCmdPushConstants(commandBuffer, bindingManager->GetPipelineLayout(SUBPASS_VOLUME_MEDIA_PARTICLES),
      //  VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PerParticleSystemData), &data);*/

      //vkCmdDispatch(commandBuffer, dispatchSize.x, dispatchSize.y, dispatchSize.z);
    }
  }
}