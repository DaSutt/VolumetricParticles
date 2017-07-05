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
#include <array>
#include <map>
#include <vulkan\vulkan.h>
#include <libshaderc\include\shaderc\shaderc.hpp>

#include "..\passes\Passes.h"

namespace Renderer
{
  class ShaderManager
  {
  public:
		enum Type
		{
			TYPE_VERT,
			TYPE_FRAG,
			TYPE_COMP,
			TYPE_GEOM
		};

		ShaderManager();
    //Reload all shaders, the new shaders are only used if all shaders were compiled correctly
    bool ReLoad(VkDevice device);
		void Release(VkDevice device);
    const std::vector<VkPipelineShaderStageCreateInfo>& GetShaderStageInfo(Renderer::SubPassType pass) const;
  private:
		struct ShaderInfo
    {
      std::string fileName;
      std::string functionName;
      Type type;
      SubPassType pass;
			std::vector<int> defines;
    };

		enum DefineBits
		{
			DEFINE_TEST,
			DEFINE_MAX
		};

		shaderc::SpvCompilationResult CompileShader(int shaderIndex);
		void AddDefines(shaderc::CompileOptions& compileOptions, int shaderIndex);
		
		shaderc::Compiler compiler_;
    std::vector<VkShaderModule> shaders_;
    std::vector<std::vector<VkPipelineShaderStageCreateInfo>> createInfo_;

		//TODO replace this with general option for shader path
		std::string shaderSourceFolder_ = "..\\source\\shaders\\";
    std::vector<ShaderInfo> shaderInfos_;
		std::array<std::string, DEFINE_MAX> defineInfos_;
  };
}