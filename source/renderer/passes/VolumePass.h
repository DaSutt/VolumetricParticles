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

#include "Pass.h"

#include "..\..\utility\Math.h"
#include "..\..\scene\Components.h"

class Scene;
namespace Renderer
{
  class ShadowMap;
  class AdaptiveGrid;

  class VolumePass : public Pass
  {
  public:
    VolumePass(ShaderBindingManager* bindingManager, RenderPassManager* renderPassManager);

    void SetShadowMap(ShadowMap* shadowMap, ImageManager* imageManager);
    void SetAdaptiveGrid(AdaptiveGrid* adaptiveGrid);
    void SetImageIndices(int raymarchingImage, int depthImage, int noiseArray);
    void RequestResources(ImageManager* imageManager, BufferManager* bufferManager, int frameCount) override;
    bool Create(VkDevice device, ShaderManager* shaderManager, FrameBufferManager* frameBufferManager,
      Surface* surface, ImageManager* imageManager) override;

    void UpdateBufferData(Surface* surface, Scene* scene);
		void Update(float dt);
    void Render(Surface* surface, FrameBufferManager* frameBufferManager,
      QueueManager* queueManager, BufferManager* bufferManager,
      std::vector<VkImageMemoryBarrier>& memoryBarrier, VkSemaphore* startSemaphore, int frameIndex) override;

		void OnLoadScene(Scene* scene);
    void OnReloadShaders(VkDevice device, ShaderManager* shaderManager) override;
    void Release(VkDevice device) override;

    VkSemaphore* GetFinishedSemaphore() override;
  private:
    enum ComputeSubpasses
    {
			COMPUTE_GLOBAL,
      COMPUTE_GROUND_FOG,
      COMPUTE_PARTICLES,
			COMPUTE_DEBUG_FILLING,
			COMPUTE_NEIGHBOR_UPDATE,
			COMPUTE_MIPMAPPING,
			COMPUTE_MIPMAPPING_MERGING,
      COMPUTE_RAYMARCHING,
      COMPUTE_MAX
    };

    struct MediaPerFrameData
    {
      glm::vec3 outScattering;
      float extinction;
      glm::vec3 emissionPhase;
      float g;
      float fogHeight;
    };

    struct RaymarchingPerFrameData
    {
      glm::mat4 toWorld;
      glm::mat4 texTransform;
      glm::mat4 toLight;
      DirectionalLight dirLight;
      glm::vec3 cameraPos;
      float stepLength;
      glm::vec4 randomness_;
      glm::ivec2 screenSize;
    };

    bool CreateComputePipelines(VkDevice device, ShaderManager* shaderManager);
    bool RecordCommands(QueueManager* queueManager, BufferManager* bufferManager, ImageManager* imageManager, Surface* surface, VkCommandBuffer cmdBuffer,
			ComputeSubpasses subpass, uint32_t currFrameIndex, int gridLevel);
    MediaPerFrameData mediaData_;
    int mediaBufferIndex_ = -1;
    RaymarchingPerFrameData raymarchingData_;
    int raymarchingBufferIndex_ = -1;

    int raymarchingImage_ = -1;
    int depthImage_ = -1;
    int noiseImageIndex_ = -1;
    int perParticleSystemConstant_;
    int perParticleSystemBinding_ = -1;

		int sceneLoaded_ = false;
		
    ShadowMap* shadowMap_;
    AdaptiveGrid* adaptiveGrid_;
    //TODO remove this
    ImageManager* imageManager_;
  };
}