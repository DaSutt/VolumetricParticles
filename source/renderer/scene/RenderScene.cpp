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

#include "RenderScene.h"

#include <glm\gtc\matrix_transform.hpp>

#include "..\..\scene\Scene.h"

#include "..\resources\BufferManager.h"
#include "..\resources\QueueManager.h"
#include "..\resources\ImageManager.h"

#include "..\ShadowMap.h"

#include "..\passes\GuiPass.h"
#include "..\passResources\ShaderBindingManager.h"


namespace Renderer
{
  RenderScene::RenderScene()
  {
    shadowMap_ = std::make_shared<ShadowMap>();
    adaptiveGrid_ = std::make_shared<AdaptiveGrid>(0.125f);
  }

  void RenderScene::RequestResources(BufferManager* bufferManager, ImageManager* imageManager, int bufferingCount)
  {
    //Use same values to create temp buffers, which will be destroyed and resized when loading a scene
    BufferManager::BufferInfo tempBuffer = {};
    tempBuffer.typeBits = BufferManager::BUFFER_CONSTANT_BIT | BufferManager::BUFFER_SCENE_BIT;
    tempBuffer.pool = BufferManager::MEMORY_CONSTANT;
    tempBuffer.size = sizeof(glm::mat4);
    tempBuffer.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    tempBuffer.bufferingCount = bufferingCount;
    for (int i = 0; i < MeshData::BUFFER_MAX; ++i)
    {
      meshData_.buffers_[i] = bufferManager->Ref_RequestBuffer(tempBuffer);
    }

    tempBuffer.pool = BufferManager::MEMORY_SCENE;
    tempBuffer.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    tempBuffer.typeBits = BufferManager::BUFFER_SCENE_BIT | BufferManager::BUFFER_STAGING_BIT;
    for (size_t i = 0; i < MeshData::BUFFER_VERTEX_MAX; ++i)
    {
      meshData_.vertexBufferIndices_.push_back(bufferManager->RequestBuffer(tempBuffer));
    }
    meshData_.indexBufferIndex_ = bufferManager->RequestBuffer(tempBuffer);

    shadowMap_->Resize(imageManager, 1024, 1024, 4);
    adaptiveGrid_->RequestResources(imageManager, bufferManager, bufferingCount);
  }

  bool RenderScene::OnLoadScene(Scene* scene, BufferManager* bufferManager, ImageManager* imageManager, QueueManager* queueManager, int frameCount)
  {
    const auto sceneBoundingBox = scene->GetSceneBoundingBox();
    adaptiveGrid_->SetWorldExtent(sceneBoundingBox.min, sceneBoundingBox.max);
    adaptiveGrid_->ResizeConstantBuffers(bufferManager);

    const auto& geometry = scene->GetGeometry();

    if (geometry.vertexPos_.empty())
      return true;

    materials_.clear();
    const auto& matInfos = scene->GetMaterialInfos();
    const auto& matMappings = scene->GetMaterialMappings();

    for (size_t i = 0; i < matInfos.size(); ++i)
    {
      materials_.push_back(Material(imageManager, matInfos[i]));
    }

    const auto& meshes = scene->GetMeshes();
    const auto& transforms = scene->GetTransforms();

    meshData_.meshCount_ = static_cast<int>(meshes.data().size());
    meshData_.vertexCount_ = static_cast<int>(geometry.vertexPos_.size());
    meshData_.indexCount_ = static_cast<int>(geometry.indices.size());

    ResizeTransformBuffers(bufferManager);
    const auto bufferSizes = ResizeMeshBuffers(bufferManager);
    VkDeviceSize totalSize = 0;
    for (auto size : bufferSizes)
    {
      totalSize += size;
    }

    BufferManager::BufferInfo uploadBufferInfo = {};
    uploadBufferInfo.size = totalSize;
    uploadBufferInfo.typeBits = BufferManager::BUFFER_TEMP_BIT;
    uploadBufferInfo.pool = BufferManager::MEMORY_TEMP;
    uploadBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    const int uploadBufferIndex = bufferManager->RequestBuffer(uploadBufferInfo);

    {
      auto data = bufferManager->Map(uploadBufferIndex, uploadBufferInfo.typeBits);
      memcpy(data, geometry.vertexPos_.data(), bufferSizes[0]);
      void* dataOffset = reinterpret_cast<glm::vec3*>(data) + geometry.vertexPos_.size();
      memcpy(dataOffset, geometry.vertexNormals_.data(), bufferSizes[1]);
      dataOffset = reinterpret_cast<glm::vec3*>(dataOffset) + geometry.vertexNormals_.size();
      memcpy(dataOffset, geometry.vertexUvs_.data(), bufferSizes[2]);
      dataOffset = reinterpret_cast<glm::vec2*>(dataOffset) + geometry.vertexUvs_.size();
      memcpy(dataOffset, geometry.indices.data(), bufferSizes[3]);

      bufferManager->Unmap(uploadBufferIndex, uploadBufferInfo.typeBits);
    }

    {
      VkBufferCopy copy = {};
      for (size_t i = 0; i < MeshData::BUFFER_VERTEX_MAX; ++i)
      {
        copy.size = bufferSizes[i];
        bufferManager->RequestTempBufferCopy(uploadBufferIndex, meshData_.vertexBufferIndices_[i], copy);
        copy.srcOffset += copy.size;
      }

      copy.size = bufferSizes[MeshData::BUFFER_VERTEX_MAX];
      bufferManager->RequestTempBufferCopy(uploadBufferIndex, meshData_.indexBufferIndex_, copy);

      bufferManager->PerformCopies(queueManager);
      vkQueueWaitIdle(queueManager->GetQueue(QueueManager::QUEUE_TRANSFER));
    }

    bufferManager->ReleaseTempBuffer(uploadBufferIndex);

    bufferData_[MeshData::BUFFER_WORLD_INV_TRANSPOSE].clear();
    int offset = static_cast<int>(meshes.data().size());
    int index = 0;
    bufferData_[MeshData::BUFFER_WORLD_INV_TRANSPOSE].resize(offset * 2);
    for (auto it = meshes.begin(); it != meshes.end(); ++it)
    {
      const auto& meshInfo = meshes.data().at(it->second);
      meshData_.indexCounts_.push_back(static_cast<uint32_t>(meshInfo.indexSize));
      meshData_.indexOffsets_.push_back(static_cast<uint32_t>(meshInfo.indexOffset));
      meshData_.vertexOffsets_.push_back(static_cast<uint32_t>(meshInfo.vertexOffset));
      meshData_.materialIndices_.push_back(matMappings.at(meshInfo.materialName));

      world_.push_back(transforms[it->first].CalcTransform());
      bufferData_[MeshData::BUFFER_WORLD_INV_TRANSPOSE][index] = world_.back();
      bufferData_[MeshData::BUFFER_WORLD_INV_TRANSPOSE][index + offset]
        = glm::transpose(glm::inverse(world_.back()));
      ++index;
    }

    transformsChanged_ = true;
    sceneLoaded_ = true;

    return true;
  }

  void RenderScene::UpdateBindings(VkDevice device, ShaderBindingManager* bindingManager, ImageManager* imageManager)
  {
    std::vector<VkDescriptorImageInfo> imageInfos;
    for (size_t i = 0; i < materials_.size(); ++i)
    {
      const auto imageIndex = materials_[i].GetImageIndex();
      if (imageIndex != -1)
      {
        imageInfos.push_back(imageManager->Ref_GetDescriptorInfo(imageIndex));
      }
      else
      {
        VkDescriptorImageInfo info;
        info.imageView = VK_NULL_HANDLE;
        imageInfos.push_back(info);
      }
    }
    bindingManager->UpdateDescriptorSets(device, SUBPASS_MESH, {}, imageInfos);
  }

  void RenderScene::Update(Scene* scene, BufferManager* bufferManager, ImageManager* imageManager, Surface* surface, int frameIndex)
  {
    if (sceneLoaded_)
    {
      const auto& lightingState = GuiPass::GetLightingState();
      shadowMap_->Update(lightingState.lightVector, &scene->GetCamera());

      if (!GuiPass::GetMenuState().fixCameraFrustum)
      {
        cameraFrustum_.ExtractPlanes(scene->GetCamera().GetProj());
      }

      //only copy all mesh indices at the moment
      activeMeshes_[SUBPASS_MESH].clear();
      activeMeshes_[SUBPASS_SHADOW_MAP].clear();
      bufferData_[MeshData::BUFFER_WORLD_VIEW_PROJ].clear();
      bufferData_[MeshData::BUFFER_WORLD_VIEW_PROJ_LIGHT].clear();

      const auto& viewProj = scene->GetCamera().GetViewProj();
      const auto& lightViewProjs = shadowMap_->GetViewProjs();

      bufferData_[MeshData::BUFFER_WORLD_VIEW_PROJ_LIGHT].resize(meshData_.indexCounts_.size() * shadowMap_->GetCascadeCount());
      for (int i = 0; i < static_cast<int>(meshData_.indexCounts_.size()); ++i)
      {
        const auto& world = world_[i];

        activeMeshes_[SUBPASS_MESH].push_back(i);
        bufferData_[MeshData::BUFFER_WORLD_VIEW_PROJ].push_back(viewProj * world);

        activeMeshes_[SUBPASS_SHADOW_MAP].push_back(i);
        for (size_t cascade = 0; cascade < shadowMap_->GetCascadeCount(); ++cascade)
        {
          bufferData_[MeshData::BUFFER_WORLD_VIEW_PROJ_LIGHT][i + cascade * meshData_.indexCounts_.size()]
            = (lightViewProjs[cascade] * world);
        }
      }

      UpdatePerFrameBuffers(bufferManager, frameIndex);

      adaptiveGrid_->Update(bufferManager, imageManager, scene, surface, shadowMap_.get(), frameIndex);
    }
  }

  void RenderScene::BindMeshData(BufferManager* bufferManager, VkCommandBuffer commandBuffer)
  {
    if (sceneLoaded_)
    {
      VkBuffer vertexBuffers[MeshData::BUFFER_VERTEX_MAX];
      VkDeviceSize offsets[MeshData::BUFFER_VERTEX_MAX];
      for (size_t i = 0; i < MeshData::BUFFER_VERTEX_MAX;++i)
      {
        const auto index = meshData_.vertexBufferIndices_[i];
        vertexBuffers[i] = bufferManager->GetBuffer(index, BufferManager::BUFFER_SCENE_BIT);
        offsets[i] = 0;
      };
      vkCmdBindVertexBuffers(commandBuffer, 0, MeshData::BUFFER_VERTEX_MAX, vertexBuffers, offsets);

      vkCmdBindIndexBuffer(commandBuffer,
        bufferManager->GetBuffer(meshData_.indexBufferIndex_, BufferManager::BUFFER_SCENE_BIT),
        0, VK_INDEX_TYPE_UINT32);
    }
  }

  void RenderScene::BindMeshMaterial(VkCommandBuffer commandBuffer, ShaderBindingManager* bindingManager, int index)
  {
    if (sceneLoaded_)
    {
      const auto matIndex = meshData_.materialIndices_[index];

      if (materials_[matIndex].GetImageIndex() != -1)
      {
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bindingManager->GetPipelineLayout(SUBPASS_MESH),
          1, 1, bindingManager->GetDescriptorSet(SUBPASS_MESH, matIndex), 0, nullptr);
      }
    }
  }

  const std::vector<int>& RenderScene::GetMeshIndices(SubPassType subpass) const
  {
    if (sceneLoaded_)
    {
      return activeMeshes_.at(subpass);
    }
    else
    {
      static std::vector<int> empty;
      return empty;
    }
  }

  const MeshData& RenderScene::GetMeshData() const
  {
    return meshData_;
  }

  void RenderScene::UpdatePerFrameBuffers(BufferManager* bufferManager, int frameIndex)
  {
    if (sceneLoaded_)
    {
      std::array<MeshData::Buffers, 3> buffers;
      buffers[0] = MeshData::BUFFER_WORLD_VIEW_PROJ;
      buffers[1] = MeshData::BUFFER_WORLD_VIEW_PROJ_LIGHT;
      buffers[2] = MeshData::BUFFER_WORLD_INV_TRANSPOSE;

      for (auto& bufferType : buffers)
      {
        const auto bufferIndex = meshData_.buffers_[bufferType];
        bufferManager->Ref_UpdateConstantBuffer(bufferIndex, frameIndex,
          bufferData_[bufferType].data(),
          bufferData_[bufferType].size() * sizeof(bufferData_[bufferType][0]));
      }
    }
  }

  std::vector<VkDeviceSize> RenderScene::ResizeMeshBuffers(BufferManager* bufferManager)
  {
    std::vector<VkDeviceSize> sizes;
    for (int i = 0; i < MeshData::BUFFER_VERTEX_MAX; ++i)
    {
      const VkDeviceSize vertexBufferSize = meshData_.vertexCount_ * MeshData::elementSizes[i];
      bufferManager->RequestBufferResize(meshData_.vertexBufferIndices_[i], vertexBufferSize);
      sizes.push_back(vertexBufferSize);
    }

    const VkDeviceSize indexBufferSize = meshData_.indexCount_ * sizeof(uint32_t);
    bufferManager->RequestBufferResize(meshData_.indexBufferIndex_, indexBufferSize);
    sizes.push_back(indexBufferSize);

    bufferManager->ResizeSceneBuffers();

    return sizes;
  }

  void RenderScene::ResizeTransformBuffers(BufferManager* bufferManager)
  {
    const VkDeviceSize matrixBufferSize = meshData_.meshCount_ * sizeof(glm::mat4);
    bufferManager->Ref_RequestBufferResize(meshData_.buffers_[MeshData::BUFFER_WORLD_VIEW_PROJ], matrixBufferSize);
    bufferManager->Ref_RequestBufferResize(meshData_.buffers_[MeshData::BUFFER_WORLD_INV_TRANSPOSE], matrixBufferSize * 2);

    const VkDeviceSize lightMatrixBufferSize = matrixBufferSize * shadowMap_->GetCascadeCount();
    bufferManager->Ref_RequestBufferResize(meshData_.buffers_[MeshData::BUFFER_WORLD_VIEW_PROJ_LIGHT], lightMatrixBufferSize);
  }
}