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

#include <Windows.h>
#include <array>
#include <glm\glm.hpp>

class InputHandler;

namespace Renderer
{
  class Instance;

  class GuiPass : public Pass
  {
  public:
    struct MenuState
    {
      bool saveConfiguration;
      bool loadScene;
      bool debugRendering;
      bool reloadShaders;
      bool fixCameraFrustum;
			bool exportFogTexture;
			bool performTimeQueries;
			bool saveTimeQueries;
			int queryFrameCount;

      MenuState();
    };

    struct LightingState
    {
      std::array<float, 3> irradiance;
      glm::vec3 lightVector;
      float lightYRotation;
      float lightZRotation;
      float shadowLogWeight;
      LightingState();
    };

    struct VolumeState
    {
      std::array<float, 3> emission;
      float absorption;
      float scattering;
      float phaseG;
      float absorptionVolumes;
      float scatteringVolumes;
      float phaseVolumes;
      float fogHeight;
			float jitteringScale;
      int stepCount;
			float lightStepDepth;
			bool debugTraversal;
			glm::vec2 cursorPosition;
			float lodScale;
			float noiseScale;
			float minTransmittance;
			bool debugFilling;
			float maxDepth;
			int shadowRayPerLevel;
      VolumeState();
    };

    struct PostProcessState
    {
      PostProcessState();
      bool debugRendering;
    };

		struct ParticleState
		{
			int particleCount;
			float spawnRadius;
			float minParticleRadius;
			float maxParticleRadius;
			ParticleState();
		};

    GuiPass(ShaderBindingManager* bindingManager, RenderPassManager* renderPassManager);

    void RequestResources(ImageManager* imageManager, BufferManager* bufferManager, int frameCount) override;
    bool Create(VkDevice device, ShaderManager* shaderManager, FrameBufferManager* frameBufferManager,
      Surface* surface, ImageManager* imageManager) override;
    void SetWindow(HWND window);

    void Update(Instance* instance, BufferManager* bufferManager, float deltaTime);
    void Render(Surface* surface, FrameBufferManager* frameBufferManager,
      QueueManager* queueManager, BufferManager* bufferManager,
      std::vector<VkImageMemoryBarrier>& memoryBarrier, VkSemaphore* startSemaphore, int frameIndex) override;

    void OnResize(uint32_t width, uint32_t height);
    void OnReloadShaders(VkDevice device, ShaderManager* shaderManager) override;
    void Release(VkDevice) override;

    VkSemaphore* GetFinishedSemaphore() override;

    static void HandleInput(InputHandler* inputHandler);
    static const MenuState& GetMenuState() { return menuState_; }
    static const LightingState& GetLightingState() { return lightingState_; }
    static const VolumeState& GetVolumeState() { return volumeState_; }
    static const PostProcessState& GetPostProcessState() { return postProcessState_; }
		static const ParticleState& GetParticleState() { return particleState_; }
  private:
    enum GraphicSubpasses
    {
      GRAPHIC_GUI,
      GRAPHIC_MAX
    };

    bool CreateFrameBuffers(VkDevice device, FrameBufferManager* frameBufferManager, Surface* surface) override;
    bool CreateGraphicPipeline(VkDevice device, ShaderManager* shaderManager);
    VkPipelineVertexInputStateCreateInfo GetVertexInputState();

    int fontTextureIndex_ = -1;
    int pushConstantIndex_ = -1;

    int vertexBufferIndex_ = -1;
    int indexBufferIndex_ = -1;

    float deltaTime_ = 0.0f;
    static MenuState menuState_;
    static LightingState lightingState_;
    static VolumeState volumeState_;
    static PostProcessState postProcessState_;
		static ParticleState particleState_;
  };
}