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

#include "ShadowMap.h"

#include <glm\gtc\matrix_transform.hpp>

#include "resources\ImageManager.h"
#include "..\Camera.h"

#include "passes\GuiPass.h"

#undef near
#undef far
#undef max
#undef min

namespace Renderer
{
  void ShadowMap::Resize(ImageManager* imageManager, uint32_t width, uint32_t height, int cascadeCount)
  {
    cascadeCount_ = cascadeCount;

    ImageManager::Ref_ImageInfo imageInfo = {};
    imageInfo.extent = { width, height, 1 };
    imageInfo.arrayCount = cascadeCount_;
    imageInfo.imageType = ImageManager::IMAGE_IMMUTABLE;
    imageInfo.format = imageManager->GetDepthImageFormat();
    imageInfo.pool = ImageManager::MEMORY_POOL_CONSTANT;
    imageInfo.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.sampler = ImageManager::SAMPLER_SHADOW;
    imageInfo.viewPerLayer = true;
    imageIndex_ = imageManager->Ref_RequestImage(imageInfo);

    viewport_.x = 0;
    viewport_.y = 0;
    viewport_.width = static_cast<float>(width);
    viewport_.height = static_cast<float>(height);
    viewport_.minDepth = 0.0f;
    viewport_.maxDepth = 1.0f;

    scissor_.offset.x = 0;
    scissor_.offset.y = 0;
    scissor_.extent.width = width;
    scissor_.extent.height = height;

    size_ = { width, height };
    worldFrustumCorners_.resize(cascadeCount_);
    worldFrustumCenter_.resize(cascadeCount_);
    viewProjs_.resize(cascadeCount_);
  }

  void ShadowMap::Update(const glm::vec3& lightVector, const Camera* camera)
  {
    if (GuiPass::GetMenuState().fixCameraFrustum)
    {
      return;
    }

    const auto near = camera->nearZ_;
    const auto far = camera->farZ_;

    view_ = glm::lookAt(glm::vec3(0), -lightVector, glm::vec3(0.0f, -1.0f, 0.0f));

    CalcSplitDistances(near, far);

    UpdateViewSpaceFrustum(camera);
    CalcFrustumCorners(camera, near, far);

    CalcViewProjections(camera, lightVector);

    CalcWorldFrustums(camera);
  }

  void ShadowMap::BindViewportScissor(VkCommandBuffer commandBuffer)
  {
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport_);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor_);
  }

  void ShadowMap::CalcSplitDistances(float near, float far)
  {
    const auto weight = GuiPass::GetLightingState().shadowLogWeight;

    splitDistances_.resize(cascadeCount_ + 1);
    splitDistances_[0] = near;
    for (int i = 1; i < cascadeCount_; ++i)
    {
      const float logDistance = near * powf((far / near), (i / static_cast<float>(cascadeCount_)));
      const float uniDistance = near + (far - near) * (i / static_cast<float>(cascadeCount_));
      splitDistances_[i] = weight * logDistance + (1.0f - weight) * uniDistance;
    }
    splitDistances_[cascadeCount_] = far;
  }

  void ShadowMap::UpdateViewSpaceFrustum(const Camera* camera)
  {
    viewFrustumCorners_.corners =
    {
      glm::vec4(-1.0f,  -1.0f, 0.0f, 1.0f),   //near left bottom
      glm::vec4(1.0f,  -1.0f, 0.0f, 1.0f),    //near right bottom
      glm::vec4(1.0f, 1.0f, 0.0f, 1.0f),      //near right top
      glm::vec4(-1.0f, 1.0f, 0.0f, 1.0f),     //near left top
      glm::vec4(-1.0f,  -1.0f, 1.0f, 1.0f),   //far left bottom
      glm::vec4(1.0f,  -1.0f, 1.0f, 1.0f),    //far right bottom
      glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),      //far right top
      glm::vec4(-1.0f, 1.0f, 1.0f, 1.0f)      //far left top
    };

    const auto invProj = glm::inverse(camera->GetProj());
    for (size_t i = 0; i < viewFrustumCorners_.corners.size(); ++i)
    {
      viewFrustumCorners_.corners[i] = invProj * viewFrustumCorners_.corners[i];
      viewFrustumCorners_.corners[i] /= viewFrustumCorners_.corners[i].w;
    }
  }

  void ShadowMap::CalcFrustumCorners(const Camera* camera, float near, float far)
  {
    std::array<glm::vec4,4> directions;
    for (size_t i = 0; i < 4; ++i)
    {
      directions[i] = glm::normalize(viewFrustumCorners_.corners[i + 4] - viewFrustumCorners_.corners[i]);
    }
		

		
		testCorners.resize(cascadeCount_);
		testCenter.resize(cascadeCount_);
		testRadius.resize(cascadeCount_);
    const auto toWorld = glm::inverse(camera->GetView());

    for (size_t frustumIndex = 0; frustumIndex < worldFrustumCorners_.size(); ++frustumIndex)
    {
      worldFrustumCenter_[frustumIndex] = glm::vec3(0.0f);
			testCenter[frustumIndex] = glm::vec3(0.0f);
      const auto nearDistance = splitDistances_[frustumIndex] / directions[0].z;
      const auto farDistance = splitDistances_[frustumIndex + 1] / directions[0].z;
      for (size_t i = 0; i < 4; ++i)
      {
        worldFrustumCorners_[frustumIndex].corners[i]      = toWorld * (nearDistance * directions[i] + glm::vec4(0,0,0,1));
        worldFrustumCorners_[frustumIndex].corners[i + 4]  = toWorld * (farDistance * directions[i] + glm::vec4(0, 0, 0, 1));

				testCorners[frustumIndex].corners[i] = (nearDistance * directions[i]) * -1.0f;
				testCorners[frustumIndex].corners[i + 4] = (farDistance * directions[i]) * -1.0f;
      }

			for (int i = 0; i < 8; ++i)
      {
				testCorners[frustumIndex].corners[i];
        worldFrustumCenter_[frustumIndex] += static_cast<glm::vec3>(worldFrustumCorners_[frustumIndex].corners[i]);
				testCenter[frustumIndex] += static_cast<glm::vec3>(testCorners[frustumIndex].corners[i]);
      }
      worldFrustumCenter_[frustumIndex] /= 8.0f;
			testCenter[frustumIndex] /= 8.0f;
			testRadius[frustumIndex] = glm::distance(static_cast<glm::vec3>(testCorners[frustumIndex].corners[4]), testCenter[frustumIndex]);
			testCenter[frustumIndex] = static_cast<glm::vec3>(toWorld * glm::vec4(testCenter[frustumIndex], 1.0f));
			//glm::vec3 offset = glm::vec3(0.0f, 0.0f, (farDistance - nearDistance) * 0.5f + nearDistance);
			//testCenter[frustumIndex] += offset;
    }
  }

  void ShadowMap::CalcViewProjections(const Camera* camera, const glm::vec3& lightVector)
  {
    debugBoundingBoxes_.clear();
		for (int i = 0; i < cascadeCount_; ++i)
		{
			const glm::mat4x4 clip(1.0f, 0.0f, 0.0f, 0.0f,
				+0.0f, -1.0f, 0.0f, 0.0f,
				+0.0f, 0.0f, 0.5f, 0.0f,
				+0.0f, 0.0f, 0.5f, 1.0f);

			const float min = -testRadius[i];
			const float max = testRadius[i];

			cascadeCenterRadius_[i] = glm::vec4(testCenter[i], testRadius[i]);

			glm::vec3 up = glm::normalize(glm::cross(-lightVector, glm::vec3(1.0f, 0.0f, 0.0f)));
			glm::mat4 view = glm::translate(glm::mat4(1.0f), -testCenter[i]) * glm::lookAt(glm::vec3(0.0f), -lightVector, up);

			if (i == 0)
			{
				//printf("%f %f %f\n", testCenter[i].x, testCenter[i].y, testCenter[i].z);
			}

			//std::array<glm::vec3, 4> directions;
			//for (size_t i = 0; i < 4; ++i)
			//{
			//	const auto temp = glm::normalize(viewFrustumCorners_.corners[i + 4] - viewFrustumCorners_.corners[i]);
			//	directions[i] = glm::vec3(temp.x, temp.y, temp.z);
			//}
			//const auto offset = ((splitDistances_[i] + splitDistances_[i + 1]) * 0.5f + splitDistances_[i]) * directions[i] / 5.0f;

			glm::mat4 proj = clip * glm::ortho(min, max, min, max, min, max);
			view = glm::lookAt(lightVector, glm::vec3(0, 0, 0), glm::vec3(0, -1, 0));
      viewProjs_[i] = proj * view;

			worldToLightSpace_[i] = glm::mat4x4(
				0.5f, 0.0f, 0.0f, 0.0f,
				0.0f, 0.5f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.5f, 0.5f, 0.0f, 1.0f
			) * viewProjs_[i];

			if (!GuiPass::GetMenuState().fixCameraFrustum)
			{
				auto rotation = glm::lookAt(glm::vec3(0.0f), lightVector, up);
				//rotation = glm::rotate(glm::mat4(1.0f), 1.0f, glm::vec3(0.0f, -1.0f, 0.0f));
				glm::mat4 testWorld = glm::scale(glm::translate(glm::mat4(1.0f), testCenter[i]), glm::vec3(testRadius[i])) * rotation;
				debugBoundingBoxes_.push_back(testWorld);

				
			}
    }

  }

  AxisAlignedBoundingBox ShadowMap::CalcBoundingBox(const FrustumCorners& frustum, const glm::mat4x4& toLight)
  {
    AxisAlignedBoundingBox result;
    result.min = glm::vec3(std::numeric_limits<float>::max());
    result.max = glm::vec3(std::numeric_limits<float>::lowest());

    for (const auto& corner : frustum.corners)
    {
      const glm::vec4 lightCorner = toLight * corner;
      result.min = glm::min(result.min, static_cast<glm::vec3>(lightCorner));
      result.max = glm::max(result.max, static_cast<glm::vec3>(lightCorner));
    }
    return result;
  }

  void ShadowMap::CalcWorldFrustums(const Camera* camera)
  {
    const auto fovHHalf = camera->GetFOVHorizontal() * 0.5f;
    const auto fovVHalf = camera->GetFOVHorizontal() * camera->GetAspectRatio() * 0.5f;
    std::array<glm::vec3, 4> directions;

    const auto horizontalRotPos = glm::rotate(glm::mat4x4(1.0f), fovHHalf, camera->GetUp());
    const auto horizontalRotNeg = glm::rotate(glm::mat4x4(1.0f), -fovHHalf, camera->GetUp());
    const auto verticalRotPos = glm::rotate(glm::mat4x4(1.0f), fovVHalf, camera->GetRight());
    const auto verticalRotNeg = glm::rotate(glm::mat4x4(1.0f), -fovVHalf, camera->GetRight());

    const glm::vec4 forward = glm::vec4(camera->GetForward(), 1.0f);
    directions[0] = verticalRotPos * horizontalRotPos * forward;
    directions[1] = verticalRotNeg * horizontalRotNeg * forward;
    directions[2] = verticalRotNeg * horizontalRotPos * forward;
    directions[3] = verticalRotPos * horizontalRotNeg * forward;
  }
}