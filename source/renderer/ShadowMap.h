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

#include <glm\glm.hpp>
#include <vulkan\vulkan.h>
#include <array>

#include "scene\renderables\Frustum.h"
#include "..\scene\components\AABoundingBox.h"

class Camera;

namespace Renderer
{
  class ImageManager;
	constexpr int g_cascadeCount = 4;

  class ShadowMap
  {
  public:
    void Resize(ImageManager* imageManager, uint32_t width, uint32_t height, int cascadeCount);
    void Update(const glm::vec3& lightVector, const Camera* camera);

    void BindViewportScissor(VkCommandBuffer commandBuffer);

    const auto& GetViewProjs() const { return viewProjs_; }
		const auto& GetShadowMatrices() const { return worldToLightSpace_; }
    const glm::mat4& GetToTextureSpace() const { return textureMatrix_; }
    const VkExtent2D& GetSize()const { return size_; }
    const int GetImageIndex() const { return imageIndex_; }
    const int GetCascadeCount() const { return cascadeCount_; }
		const auto& GetSplitDistances() const { return splitDistances_; }

    std::vector<glm::mat4x4> debugBoundingBoxes_;
  private:
    struct FrustumCorners
    {
      std::array<glm::vec4, 8> corners;
    };

    void CalcSplitDistances(float near, float far);
    void UpdateViewSpaceFrustum(const Camera* camera);
    void CalcFrustumCorners(const Camera* camera, float near, float far);
    void CalcViewProjections(const Camera* camera, const glm::vec3& lightVector);
    AxisAlignedBoundingBox CalcBoundingBox(const FrustumCorners& frustum, const glm::mat4x4& toLight);

    void CalcWorldFrustums(const Camera* camera);

    VkViewport viewport_;
    VkRect2D scissor_;
    glm::mat4 viewProj_;
    glm::mat4 textureMatrix_;

    VkExtent2D size_;
    int imageIndex_ = -1;
    int cascadeCount_;

    std::vector<float> splitDistances_;
    std::vector<FrustumCorners> worldFrustumCorners_;
    std::vector<glm::vec3> worldFrustumCenter_;
    FrustumCorners viewFrustumCorners_;

    glm::mat4 view_;
    std::vector<glm::mat4> viewProjs_;
		std::array<glm::mat4, g_cascadeCount> worldToLightSpace_;

		std::vector<FrustumCorners> testCorners;
		std::vector<glm::vec3> testCenter;
		std::vector<float> testRadius;

		std::array<glm::vec4, g_cascadeCount> cascadeCenterRadius_;
  };
}