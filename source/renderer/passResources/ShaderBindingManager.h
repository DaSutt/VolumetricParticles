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
#include <array>
#include <map>

#include "..\passes\Passes.h"

class Scene;

namespace Renderer
{
  class Instance;
  class ImageManager;
  class BufferManager;

  class ShaderBindingManager
  {
  public:
    struct BindingInfo
    {
      std::vector<VkDescriptorType> types;
      std::vector<VkShaderStageFlags> stages;
      std::vector<int> resourceIndex;
      int offset;
      bool Ref_image;
      std::vector<bool> refactoring_;
      int setCount;
      SubPassType pass;
      BindingInfo();
    };

    struct PushConstantInfo
    {
      std::vector<VkPushConstantRange> pushConstantRanges;
      SubPassType pass;
    };

    struct SceneBindingInfo
    {
      std::vector<VkDescriptorType> types;
      std::vector<VkShaderStageFlags> stages;
      SubPassType pass;
    };

    bool Create(Instance* instance);
    void Release(VkDevice device);
    void UpdateDescriptorSets(VkDevice device, ImageManager* imageManager, BufferManager* bufferManager);
    void UpdateDescriptorSets(VkDevice device, SubPassType subpass, const std::vector<VkDescriptorBufferInfo>& buffers, const std::vector<VkDescriptorImageInfo>& images);
    void OnLoadScene(VkDevice device, Scene* scene);

    int RequestShaderBinding(const BindingInfo& bindingInfo);
    int RequestSceneShaderBinding(const SceneBindingInfo& bindingInfo);
    std::vector<int> RequestPushConstants(const PushConstantInfo& pushConstantInfo);
    const VkPipelineLayout& GetPipelineLayout(SubPassType pass) { return pipelineLayouts_[pass]; }
    const VkDescriptorSet& GetDescriptorSet(int index, int frameOffset) { return descriptorSets_[index + frameOffset]; }
    VkDescriptorSet* GetDescriptorSet(SubPassType pass, int index);
  private:
    VkDescriptorPool CreateDescriptorPool(VkDevice device, const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets);
    bool CreateDescriptorSetLayouts(VkDevice device);
    bool AllocateDescriptorSets(VkDevice device);
    bool CreatePipelineLayouts(VkDevice device);

    VkDescriptorPool                        descriptorPool_       = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayout>      descriptorSetLayouts_;
    std::vector<VkDescriptorSet>            descriptorSets_;
    std::map<SubPassType, int>                   descriptorPassMapping_;
    std::array<VkPipelineLayout, SUBPASS_MAX>  pipelineLayouts_;

    std::map<SubPassType, std::vector<int>> pushConstantPassMapping_;
    std::vector<VkPushConstantRange> pushConstants_;

    VkDescriptorPool sceneDescriptorPool_ = VK_NULL_HANDLE;
    std::map<SubPassType, int> sceneDescriptorPassMapping_;
    std::map<SubPassType, int> sceneDescriptorSetOffsets_;
    std::vector<VkDescriptorSet> sceneDescriptorSets_;

    std::vector<BindingInfo> bindingInfos_;
    std::vector<SceneBindingInfo> sceneBindingInfos_;
  };

  bool CreateSetLayout(VkDevice device, const std::vector<VkDescriptorSetLayoutBinding>& layoutBindings, VkDescriptorSetLayout* setLayout);
  bool CreatePipelineLayout(VkDevice device, const std::vector<VkDescriptorSetLayout>& setLayouts, const std::vector<VkPushConstantRange> pushConstants, VkPipelineLayout* pipelineLayout);
}