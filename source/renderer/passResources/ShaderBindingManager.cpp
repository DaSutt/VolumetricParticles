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

#include "ShaderBindingManager.h"

#include <map>
#include <algorithm>

#include "..\wrapper\Instance.h"

#include "..\resources\ImageManager.h"
#include "..\resources\BufferManager.h"

#include "..\..\scene\Scene.h"

namespace Renderer
{
  ShaderBindingManager::BindingInfo::BindingInfo() :
    pass{SUBPASS_NONE},
    setCount{ 1 },
    Ref_image{false}
  {}

  namespace
  {
    std::vector<VkDescriptorPoolSize> GetPoolSizes(const std::vector<ShaderBindingManager::BindingInfo>& bindingInfos)
    {
      std::map<VkDescriptorType, int> typeCounts;
      for (const auto& info : bindingInfos)
      {
        for (const auto& type : info.types)
        {
          typeCounts[type] = typeCounts[type] + 1;
        }
      }

      std::vector<VkDescriptorPoolSize> poolSizes;
      for (int type = 0; type < VK_DESCRIPTOR_TYPE_END_RANGE; ++type)
      {
        auto typeCount = typeCounts.find(static_cast<VkDescriptorType>(type));
        if (typeCount != typeCounts.end())
        {
          poolSizes.push_back({ typeCount->first, static_cast<uint32_t>(typeCount->second) });
        }
      }
      return poolSizes;
    }
  }

  bool ShaderBindingManager::Create(Instance* instance)
  {
    const auto& device = instance->GetDevice();

    const auto poolSizes = GetPoolSizes(bindingInfos_);

    descriptorPool_ = CreateDescriptorPool(device, poolSizes, static_cast<int>(bindingInfos_.size()));
    if(descriptorPool_ == VK_NULL_HANDLE) { return false; }

    if (!CreateDescriptorSetLayouts(device)) { return false; }
    if (!AllocateDescriptorSets(device)) { return false; }
    if (!CreatePipelineLayouts(device)) { return false; }

    printf("ShaderBindingManager created successfully\n");
    return true;
  }

  void ShaderBindingManager::Release(VkDevice device)
  {
    for (int pass = 0; pass < SUBPASS_MAX; ++pass)
    {
      if (pipelineLayouts_[pass] != VK_NULL_HANDLE)
      {
        vkDestroyPipelineLayout(device, pipelineLayouts_[pass], nullptr);
      }
    }
    for (auto& setLayout : descriptorSetLayouts_)
    {
      vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
    }
    vkDestroyDescriptorPool(device, descriptorPool_, nullptr);

    if (sceneDescriptorPool_ != VK_NULL_HANDLE)
    {
      vkDestroyDescriptorPool(device, sceneDescriptorPool_, nullptr);
    }
  }

  void ShaderBindingManager::UpdateDescriptorSets(VkDevice device, ImageManager* imageManager, BufferManager* bufferManager)
  {
    std::vector<VkWriteDescriptorSet> writeDescriptorSets;
    for (size_t i = 0; i < descriptorSets_.size(); ++i)
    {
      for (size_t binding = 0; binding < bindingInfos_[i].types.size(); ++binding)
      {
        VkWriteDescriptorSet writeSet = {};
        writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSet.dstSet = descriptorSets_[i];
        writeSet.dstBinding = static_cast<uint32_t>(binding);
        writeSet.descriptorCount = 1;
        writeSet.descriptorType = bindingInfos_[i].types[binding];
        const auto resourceIndex = bindingInfos_[i].resourceIndex[binding];
        switch (writeSet.descriptorType)
        {
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
          if (bindingInfos_[i].Ref_image ||
            (!bindingInfos_[i].refactoring_.empty() && bindingInfos_[i].refactoring_[binding]))
          {
            writeSet.pImageInfo = &imageManager->Ref_GetDescriptorInfo(resourceIndex);
          }
          else
          {
            writeSet.pImageInfo = &imageManager->GetDescriptorInfo(resourceIndex);
          }
          break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
          if (!bindingInfos_[i].refactoring_.empty() && bindingInfos_[i].refactoring_[binding])
          {
            writeSet.pBufferInfo = bufferManager->Ref_GetDescriptorInfo(resourceIndex, bindingInfos_[i].offset);
          }
          else
          {
            writeSet.pBufferInfo = bufferManager->GetDescriptorInfo(resourceIndex);
          }
          break;
        default:
          printf("Update descriptor set type not yet supported\n");
          break;
        }
        writeDescriptorSets.push_back(writeSet);
      }
    }

    vkUpdateDescriptorSets(device,
      static_cast<uint32_t>(writeDescriptorSets.size()),
      writeDescriptorSets.data(), 0, nullptr);
  }

  void ShaderBindingManager::UpdateDescriptorSets(VkDevice device, SubPassType subpass, const std::vector<VkDescriptorBufferInfo>& buffers, const std::vector<VkDescriptorImageInfo>& images)
  {
    std::vector<VkWriteDescriptorSet> writeDescriptorSets;
    for (size_t i = 0; i < buffers.size(); ++i)
    {
      const auto descriptorIndex = i + sceneDescriptorSetOffsets_[subpass];

      VkWriteDescriptorSet writeSet = {};
      writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writeSet.dstSet = sceneDescriptorSets_[i];
      writeSet.dstBinding = 0;
      writeSet.descriptorCount = 1;
      writeSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      writeSet.pBufferInfo = &buffers[i];
      writeDescriptorSets.push_back(writeSet);
    }
    for (size_t i = 0; i < images.size(); ++i)
    {
      if (images[i].imageView != VK_NULL_HANDLE)
      {
        const auto descriptorIndex = i + sceneDescriptorSetOffsets_[subpass];

        VkWriteDescriptorSet writeSet = {};
        writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSet.dstSet = sceneDescriptorSets_[i];
        writeSet.dstBinding = 0;
        writeSet.descriptorCount = 1;
        writeSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeSet.pImageInfo = &images[i];
        writeDescriptorSets.push_back(writeSet);
      }
    }

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
  }

  void ShaderBindingManager::OnLoadScene(VkDevice device, Scene* scene)
  {
    std::vector<VkDescriptorPoolSize> poolSizes;
    std::vector<VkDescriptorSetLayout> layouts;

    uint32_t totalSets = 0;
    /*{
      uint32_t setCount = 0;
      const auto particleDescriptors = sceneDescriptorPassMapping_.find(SUBPASS_VOLUME_MEDIA_PARTICLES);
      if (particleDescriptors != sceneDescriptorPassMapping_.end())
      {
        sceneDescriptorSetOffsets_[SUBPASS_VOLUME_MEDIA_PARTICLES] = setCount;
        const auto particleSystemCount = scene->GetParticleSystems().size();
        setCount += static_cast<uint32_t>(particleSystemCount);
        poolSizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<uint32_t>(particleSystemCount) });
        const auto layout = descriptorSetLayouts_[sceneDescriptorPassMapping_[SUBPASS_VOLUME_MEDIA_PARTICLES]];
        for (uint32_t i = 0; i < setCount;++i)
        {
          layouts.push_back(layout);
        }
      }
      totalSets += setCount;
    }*/
    {
      uint32_t setCount = 0;
      const auto particleDescriptors = sceneDescriptorPassMapping_.find(SUBPASS_MESH);
      if (particleDescriptors != sceneDescriptorPassMapping_.end())
      {
        sceneDescriptorSetOffsets_[SUBPASS_MESH] = setCount;
        const auto materialCount = scene->GetMaterialInfos().size();
        setCount += static_cast<uint32_t>(materialCount);
        poolSizes.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(materialCount) });
        const auto layout = descriptorSetLayouts_[sceneDescriptorPassMapping_[SUBPASS_MESH]];
        for (uint32_t i = 0; i < setCount;++i)
        {
          layouts.push_back(layout);
        }
      }
      totalSets += setCount;
    }

    if (sceneDescriptorPool_ != VK_NULL_HANDLE)
    {
      vkDestroyDescriptorPool(device, sceneDescriptorPool_, nullptr);
    }
    sceneDescriptorPool_ = CreateDescriptorPool(device, poolSizes, totalSets);
    if (sceneDescriptorPool_ == VK_NULL_HANDLE)
    {
      printf("Failed creating scene descriptor pool\n");
    }

    sceneDescriptorSets_.resize(totalSets);
    VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocInfo.descriptorPool = sceneDescriptorPool_;
    allocInfo.descriptorSetCount = totalSets;
    allocInfo.pSetLayouts = layouts.data();
    const auto result = vkAllocateDescriptorSets(device, &allocInfo, sceneDescriptorSets_.data());
    if (result != VK_SUCCESS)
    {
      printf("Failed allocating scene descriptor sets\n");
    }
  }

  int ShaderBindingManager::RequestShaderBinding(const BindingInfo& bindingInfo)
  {
    const int index = static_cast<int>(bindingInfos_.size());
    assert(bindingInfo.types.size() == bindingInfo.resourceIndex.size());
    descriptorPassMapping_[bindingInfo.pass] = index;
    BindingInfo info = bindingInfo;
    for (int i = 0; i < bindingInfo.setCount; ++i)
    {
      info.offset = i;
      bindingInfos_.push_back(info);
    }
    return index;
  }

  int ShaderBindingManager::RequestSceneShaderBinding(const SceneBindingInfo& bindingInfo)
  {
    const int index = static_cast<int>(sceneBindingInfos_.size());
    sceneDescriptorPassMapping_[bindingInfo.pass] = index;
    sceneBindingInfos_.push_back(bindingInfo);
    return index;
  }

  std::vector<int> ShaderBindingManager::RequestPushConstants(const PushConstantInfo& pushConstantInfo)
  {
    std::vector<int> indices;
    for (const auto& pushConstant : pushConstantInfo.pushConstantRanges)
    {
      const int index = static_cast<int>(pushConstants_.size());
      indices.push_back(index);
      pushConstantPassMapping_[pushConstantInfo.pass].push_back(index);
      pushConstants_.push_back(pushConstant);
    }
    return indices;
  }

  VkDescriptorSet* ShaderBindingManager::GetDescriptorSet(SubPassType pass, int index)
  {
    const auto i = index + sceneDescriptorSetOffsets_[pass];
    return &sceneDescriptorSets_[i];
  }

  VkDescriptorPool ShaderBindingManager::CreateDescriptorPool(VkDevice device, const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets)
  {
    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = maxSets;

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    const auto result = vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);
    if (result != VK_SUCCESS)
    {
      printf("Failed creating descriptor pool, %d\n", result);
      return VK_NULL_HANDLE;
    }

    return descriptorPool;
  }

  bool ShaderBindingManager::CreateDescriptorSetLayouts(VkDevice device)
  {
    for (size_t bindingIndex = 0; bindingIndex < bindingInfos_.size(); ++bindingIndex)
    {
      std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
      for (size_t i = 0; i < bindingInfos_[bindingIndex].types.size(); ++i)
      {
        VkDescriptorSetLayoutBinding layoutBinding = {};
        layoutBinding.binding = static_cast<uint32_t>(i);
        layoutBinding.descriptorType = bindingInfos_[bindingIndex].types[i];
        layoutBinding.descriptorCount = 1;
        layoutBinding.stageFlags = bindingInfos_[bindingIndex].stages[i];

        layoutBindings.push_back(layoutBinding);
      }

      descriptorSetLayouts_.push_back(VK_NULL_HANDLE);
      if (!CreateSetLayout(device, layoutBindings, &descriptorSetLayouts_.back()))
      {
        return false;
      }
    }

    for (size_t bindingIndex = 0; bindingIndex < sceneBindingInfos_.size(); ++bindingIndex)
    {
      std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
      for (size_t i = 0; i < sceneBindingInfos_[bindingIndex].types.size(); ++i)
      {
        VkDescriptorSetLayoutBinding layoutBinding = {};
        layoutBinding.binding = static_cast<uint32_t>(i);
        layoutBinding.descriptorType = sceneBindingInfos_[bindingIndex].types[i];
        layoutBinding.descriptorCount = 1;
        layoutBinding.stageFlags = sceneBindingInfos_[bindingIndex].stages[i];

        layoutBindings.push_back(layoutBinding);
      }

      descriptorSetLayouts_.push_back(VK_NULL_HANDLE);
      if (!CreateSetLayout(device, layoutBindings, &descriptorSetLayouts_.back()))
      {
        return false;
      }
      //Offset for descriptorsets
      sceneDescriptorPassMapping_[sceneBindingInfos_[bindingIndex].pass] += static_cast<int>(bindingInfos_.size());
    }

    return true;
  }

  bool ShaderBindingManager::CreatePipelineLayouts(VkDevice device)
  {
    for (int pass = 0; pass <= SUBPASS_MAX; ++pass)
    {
      std::vector<VkPushConstantRange> pushConstants;
      const auto mapping = pushConstantPassMapping_.find(static_cast<SubPassType>(pass));
      if (mapping != pushConstantPassMapping_.end())
      {
        for (auto index : mapping->second)
        {
          pushConstants.push_back(pushConstants_[index]);
        }
      }

      std::vector<VkDescriptorSetLayout> setLayouts;
      const auto descriptorMapping = descriptorPassMapping_.find(static_cast<SubPassType>(pass));
      if (descriptorMapping != descriptorPassMapping_.end())
      {
        setLayouts = { descriptorSetLayouts_[descriptorMapping->second] };
        const auto sceneDescriptorMapping = sceneDescriptorPassMapping_.find(static_cast<SubPassType>(pass));
        if (sceneDescriptorMapping != sceneDescriptorPassMapping_.end())
        {
          setLayouts.push_back(descriptorSetLayouts_[sceneDescriptorMapping->second]);
        }
        if (!CreatePipelineLayout(device, setLayouts, pushConstants, &pipelineLayouts_[pass]))
        {
          return false;
        }
      }
    }
    return true;
  }

  bool ShaderBindingManager::AllocateDescriptorSets(VkDevice device)
  {
    const auto setCount = static_cast<uint32_t>(bindingInfos_.size());

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool_;
    allocInfo.descriptorSetCount = setCount;
    allocInfo.pSetLayouts = descriptorSetLayouts_.data();

    descriptorSets_.resize(setCount, VK_NULL_HANDLE);
    const auto result = vkAllocateDescriptorSets(device, &allocInfo, descriptorSets_.data());
    if (result != VK_SUCCESS)
    {
      printf("Failed creating descriptor set, %d\n", result);
      return false;
    }
    return true;
  }

  bool CreateSetLayout(VkDevice device, const std::vector<VkDescriptorSetLayoutBinding>& layoutBindings, VkDescriptorSetLayout* setLayout)
  {
    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
    layoutInfo.pBindings = layoutBindings.data();

    auto result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, setLayout);
    if (result != VK_SUCCESS)
    {
      printf("Failed creating descriptor set layout, %d\n", result);
      return false;
    }
    return true;
  }

  bool CreatePipelineLayout(VkDevice device, const std::vector<VkDescriptorSetLayout>& setLayouts,
    const std::vector<VkPushConstantRange> pushConstants,
    VkPipelineLayout* pipelineLayout)
  {
    VkPipelineLayoutCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    createInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    createInfo.pSetLayouts = setLayouts.data();
    createInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstants.size());
    createInfo.pPushConstantRanges = pushConstants.data();

    const auto result = vkCreatePipelineLayout(device, &createInfo, nullptr, pipelineLayout);
    if (result != VK_SUCCESS)
    {
      printf("Failed creating pipeline layout, %d\n", result);
      return false;
    }
    return true;
  }
};