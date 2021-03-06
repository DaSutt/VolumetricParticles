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

#include "MeshRenderable.h"

VkPipelineVertexInputStateCreateInfo MeshRenderable::GetVertexInputState()
{
  static std::vector<VkVertexInputBindingDescription> bindings =
  {
    { 0, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_VERTEX },
    { 1, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_VERTEX},
    { 2, sizeof(glm::vec2), VK_VERTEX_INPUT_RATE_VERTEX}
  };

  static std::vector<VkVertexInputAttributeDescription> attributeDescriptions =
  {
    { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
    { 1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0 },
    { 2, 2, VK_FORMAT_R32G32_SFLOAT,0}
  };

  VkPipelineVertexInputStateCreateInfo inputState = {};
  inputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  inputState.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
  inputState.pVertexBindingDescriptions = bindings.data();
  inputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
  inputState.pVertexAttributeDescriptions = attributeDescriptions.data();
  return inputState;
}