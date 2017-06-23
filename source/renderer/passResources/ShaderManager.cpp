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

#include "ShaderManager.h"

#include <fstream>
#include <assert.h>

namespace Renderer
{

  std::string ShaderManager::shaderSourceFolder_ = "..\\source\\shaders\\";

	std::vector<ShaderManager::ShaderInfo> ShaderManager::shaderInfo_ =
	{
		{ "Mesh.vert", "main", TYPE_VERT, SUBPASS_MESH, 0, 0},
		{ "Mesh.frag", "main", TYPE_FRAG, SUBPASS_MESH, 0, 0 },
		{ "Mesh_DepthOnly.vert", "main", TYPE_VERT, SUBPASS_MESH_DEPTH_ONLY, 0, 0 },
		{ "Mesh_DepthOnly.frag", "main", TYPE_FRAG, SUBPASS_MESH_DEPTH_ONLY, 0, 0 },
		{ "PostProcess.vert", "main", TYPE_VERT, SUBPASS_POSTPROCESS, 0, 0 },
		{ "PostProcess.frag", "main", TYPE_FRAG, SUBPASS_POSTPROCESS, 0, 0 },
		{ "PostProcess_Debug.vert", "main", TYPE_VERT, SUBPASS_POSTPROCESS_DEBUG, 0, 0 },
		{ "PostProcess_Debug.frag", "main", TYPE_FRAG, SUBPASS_POSTPROCESS_DEBUG, 0, 0 },
		{ "Gui.vert", "main", TYPE_VERT, SUBPASS_GUI, 0, 0 },
		{ "Gui.frag", "main", TYPE_FRAG, SUBPASS_GUI, 0, 0 },
		{ "ShadowMap.vert", "main", TYPE_VERT, SUBPASS_SHADOW_MAP, 0, 0 },
		{ "ShadowMap.frag", "main", TYPE_FRAG, SUBPASS_SHADOW_MAP, 0, 0 },
		{ "GridGlobal.comp", "main", TYPE_COMP, SUBPASS_VOLUME_ADAPTIVE_GLOBAL, 0, 0},
		{ "GridGroundFog.comp", "main", TYPE_COMP, SUBPASS_VOLUME_ADAPTIVE_GROUND_FOG, 0, 0},
		{ "GridParticles.comp", "main", TYPE_COMP, SUBPASS_VOLUME_ADAPTIVE_PARTICLES, 0, 0},
		{ "GridDebugFilling.comp", "main", TYPE_COMP, SUBPASS_VOLUME_ADAPTIVE_DEBUG_FILLING, 0,0},
		{ "GridNeighborUpdate.comp", "main", TYPE_COMP, SUBPASS_VOLUME_ADAPTIVE_NEIGHBOR_UPDATE, 0, 0},
		{ "GridMipMapping.comp", "main", TYPE_COMP, SUBPASS_VOLUME_ADAPTIVE_MIPMAPPING, 0,0},
		{ "GridMipMappingMerging.comp", "main", TYPE_COMP, SUBPASS_VOLUME_ADAPTIVE_MIPMAPPING_MERGING, 0,0},
    { "GridRaymarching.comp", "main", TYPE_COMP, SUBPASS_VOLUME_ADAPTIVE_RAYMARCHING, 0, 0}
  };

  std::map<ShaderManager::IncludeFileBits, ShaderManager::IncludeData> ShaderManager::includeFiles_ =
  {
    { INCLUDE_PRECISION, { "include\\Precision.comp" }}
  };

  std::map<ShaderManager::DefineBits, std::string> ShaderManager::defines_ =
  {
    { DEFINE_FP16, "MEDIUM_PRECISION"}
  };

  bool ShaderManager::mediumPrecision_ = true;

  ShaderManager::IncludeData::IncludeData(const std::string fileName) :
    fileName{fileName}
  {}

  bool ShaderManager::ReLoad(VkDevice device)
  {
    if (!ReloadIncludes())
    {
      printf("Failed to load include files\n");
      return false;
    }

    shaderc_compiler_t compiler = shaderc_compiler_initialize();

    //temp containers during compilation
    std::vector<VkShaderModule> shaderModules;
    std::vector<std::vector<VkPipelineShaderStageCreateInfo>> createInfo(Renderer::SUBPASS_MAX);

    //set to true if one of the shaders fails to compile
    bool failed = false;
    for (size_t i = 0; i < shaderInfo_.size(); ++i)
    {
      const auto compileResult = RecompileShader(compiler, shaderInfo_[i]);
      //check if something was compiled
      if (compileResult != nullptr && shaderc_result_get_length(compileResult) != 0)
      {
        VkShaderModuleCreateInfo shaderInfo = {};
        shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderInfo.codeSize = shaderc_result_get_length(compileResult);
        shaderInfo.pCode = reinterpret_cast<const uint32_t*>(shaderc_result_get_bytes(compileResult));

        VkShaderModule shaderModule = VK_NULL_HANDLE;
        const auto result = vkCreateShaderModule(device, &shaderInfo, nullptr, &shaderModule);
        assert(result == VK_SUCCESS);
        if (result != VK_SUCCESS)
        {
          failed = true;
        }
        else
        {
          //add shader modules and create info to temp arrays
          shaderModules.push_back(shaderModule);
          createInfo[shaderInfo_[i].pass].push_back(GetCreateInfo(shaderModule, shaderInfo_[i].type, static_cast<int>(i)));

          shaderc_result_release(compileResult);
        }
      }
      else
      {
        failed = true;
      }
    }

    if (!failed)
    {
      //release old shaders and replace with new ones
      Release(device);
      shaders_ = shaderModules;
      createInfo_ = createInfo;
      printf("Shader reloading successfull\n");
    }
    else
    {
      //release successfully created shaders if one was not compiled
      for (auto& s : shaderModules)
      {
        vkDestroyShaderModule(device, s, nullptr);
      }
    }

    shaderc_compiler_release(compiler);

    return !failed;
  }

  void ShaderManager::SwitchPrecision(VkDevice device)
  {
    mediumPrecision_ = !mediumPrecision_;

    ReLoad(device);
  }

  void ShaderManager::Release(VkDevice device)
  {
    //wait until shaders are not used anymore
    vkDeviceWaitIdle(device);

    for (auto& s : shaders_)
    {
      vkDestroyShaderModule(device, s, nullptr);
    }

    shaders_.resize(shaderInfo_.size());
  }

  shaderc_compilation_result_t ShaderManager::RecompileShader(shaderc_compiler_t& compiler, const ShaderInfo& info)
  {
    auto& fileContent = ReadFile(shaderSourceFolder_ + info.fileName);
    if (fileContent.empty())
    {
      //alternative shader source folder to try
      shaderSourceFolder_ = "..\\" + shaderSourceFolder_;
      fileContent = ReadFile(shaderSourceFolder_ + info.fileName);
      if (fileContent.empty())
      {
        printf("Empty shader file %s", info.fileName.c_str());
        return nullptr;
      }
    }

    if (info.includeFiles != 0)
    {
      fileContent = AddInclude(fileContent, info.includeFiles);
    }

    shaderc_shader_kind shaderKind;
    switch (info.type)
    {
    case ShaderManager::TYPE_VERT:
      shaderKind = shaderc_glsl_vertex_shader;
      break;
    case ShaderManager::TYPE_FRAG:
      shaderKind = shaderc_glsl_fragment_shader;
      break;
    case ShaderManager::TYPE_COMP:
      shaderKind = shaderc_glsl_compute_shader;
      break;
    case ShaderManager::TYPE_GEOM:
      shaderKind = shaderc_glsl_geometry_shader;
      break;
    default:
      printf("[WARNING] Recompile shader: invalid shader type\n");
      break;
    }

    shaderc_compile_options_t compileOptions = shaderc_compile_options_initialize();
    if (info.defines != 0)
    {
      for (auto& define : defines_)
      {
        if (define.first & info.defines)
        {
          if (define.first == DEFINE_FP16 && !mediumPrecision_)
          {
            continue;
          }

          shaderc_compile_options_add_macro_definition(compileOptions, define.second.c_str(),
            define.second.size(), nullptr, 0u);
        }
      }
    }

    auto result = shaderc_compile_into_spv(compiler, fileContent.data(), fileContent.size(),
      shaderKind, info.fileName.c_str(), info.fileName.c_str(), compileOptions);
    shaderc_compile_options_release(compileOptions);

    if (result == nullptr)
    {
      printf("Shader compilation of %s failed\n", info.fileName.c_str());
    }
    else if (shaderc_result_get_length(result) == 0)
    {
      printf("Shader compilation of %s failed: %s\n", info.fileName.c_str(), shaderc_result_get_error_message(result));
    }

    return result;
  }

  std::string ShaderManager::ReadFile(const std::string& fileName)
  {
    std::ifstream file(fileName);
    std::string buffer;

    if (file.is_open())
    {
      file.seekg(0, std::ios::end);
      buffer.reserve(file.tellg());
      file.seekg(0, std::ios::beg);

      buffer.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

      return buffer;
    }
    else
    {
      printf("Shader not found %s\n", fileName.c_str());
      return buffer;
    }
  }

  VkPipelineShaderStageCreateInfo ShaderManager::GetCreateInfo(VkShaderModule module, Type type, int index)
  {
    VkPipelineShaderStageCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    switch (type)
    {
    case ShaderManager::TYPE_VERT:
      createInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
      break;
    case ShaderManager::TYPE_FRAG:
      createInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      break;
    case ShaderManager::TYPE_COMP:
      createInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
      break;
    case ShaderManager::TYPE_GEOM:
      createInfo.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
      break;
    default:
      printf("[WARNING] GetCreateInfo: invalid shader type\n");
      break;
    }
    createInfo.module = module;
    createInfo.pName = shaderInfo_[index].functionName.c_str();
    return createInfo;
  }

  std::string ShaderManager::AddInclude(const std::string& fileContent, int includeFiles)
  {
    auto firstLineEnd = fileContent.find_first_of('\n');

    std::string result = fileContent.substr(0, firstLineEnd + 1);
    for (auto& includeFile : includeFiles_)
    {
      if (includeFile.first & includeFiles)
      {
        result += includeFile.second.content;
      }
    }
    result += fileContent.substr(firstLineEnd + 1, fileContent.size() - firstLineEnd - 1);

    return result;
  }

  const std::vector<VkPipelineShaderStageCreateInfo>& ShaderManager::GetShaderStageInfo(Renderer::SubPassType pass) const
  {
    return createInfo_[pass];
  }

  bool ShaderManager::ReloadIncludes()
  {
    for (auto& includeFile : includeFiles_)
    {
      includeFile.second.content = ReadFile(shaderSourceFolder_ + includeFile.second.fileName);
      if (includeFile.second.content.empty())
      {
        shaderSourceFolder_ = "..\\" + shaderSourceFolder_;
        includeFile.second.content = ReadFile(shaderSourceFolder_ + includeFile.second.fileName);
        if (includeFile.second.content.empty())
        {
          printf("Failed reading shader include file\n");
          return false;
        }
      }
    }

    return true;
  }
}