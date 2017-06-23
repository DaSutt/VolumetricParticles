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

#include <vector>
#include <map>
#include <vulkan\vulkan.h>
#include <libshaderc\include\shaderc\shaderc.h>

#include "..\passes\Passes.h"

namespace Renderer
{
  class ShaderManager
  {
  public:
    bool ReLoad(VkDevice device);
    void SwitchPrecision(VkDevice device);
    void Release(VkDevice device);

    const std::vector<VkPipelineShaderStageCreateInfo>& GetShaderStageInfo(Renderer::SubPassType pass) const;
  private:
    enum Type
    {
      TYPE_VERT,
      TYPE_FRAG,
      TYPE_COMP,
      TYPE_GEOM
    };

    enum IncludeFileBits
    {
      INCLUDE_PRECISION = 1 << 0,
      INCLUDE_MAX
    };

    enum DefineBits
    {
      DEFINE_FP16 = 1 << 0,
      DEFINE_MAX
    };

    struct ShaderInfo
    {
      std::string fileName;
      std::string functionName;
      Type type;
      SubPassType pass;
      int includeFiles;
      int defines;
    };

    struct IncludeData
    {
      std::string fileName;
      std::string content;
      IncludeData(const std::string fileName);
    };

    static shaderc_compilation_result_t RecompileShader(shaderc_compiler_t& compiler, const ShaderInfo& info);
    static std::string ReadFile(const std::string& fileName);
    static VkPipelineShaderStageCreateInfo GetCreateInfo(VkShaderModule module, Type type, int index);
    static std::string AddInclude(const std::string& fileContent, int includeFiles);

    static std::vector<ShaderInfo> shaderInfo_;
    static std::map<IncludeFileBits, IncludeData> includeFiles_;
    static std::map<DefineBits, std::string> defines_;
    static std::string shaderSourceFolder_;
    static bool mediumPrecision_;

    bool ReloadIncludes();

    std::vector<VkShaderModule> shaders_;
    std::vector<std::vector<VkPipelineShaderStageCreateInfo>> createInfo_;
  };
}