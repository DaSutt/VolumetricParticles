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
#include <iostream>

#include "file_includer.h"

namespace Renderer
{
	ShaderManager::ShaderManager()
	{
		//TODO replace with config file
		shaderInfos_ = {
			{ "Mesh.vert", "main", TYPE_VERT, SUBPASS_MESH, 0, 0 },
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
			{ "GridGlobal.comp", "main", TYPE_COMP, SUBPASS_VOLUME_ADAPTIVE_GLOBAL, 0, 0 },
			{ "GridGroundFog.comp", "main", TYPE_COMP, SUBPASS_VOLUME_ADAPTIVE_GROUND_FOG, 0, 0 },
			{ "GridParticles.comp", "main", TYPE_COMP, SUBPASS_VOLUME_ADAPTIVE_PARTICLES, 0, 0 },
			{ "GridDebugFilling.comp", "main", TYPE_COMP, SUBPASS_VOLUME_ADAPTIVE_DEBUG_FILLING, 0,0 },
			{ "GridNeighborUpdate.comp", "main", TYPE_COMP, SUBPASS_VOLUME_ADAPTIVE_NEIGHBOR_UPDATE, 0, 0 },
			{ "GridMipMapping.comp", "main", TYPE_COMP, SUBPASS_VOLUME_ADAPTIVE_MIPMAPPING, 0,0 },
			{ "GridMipMappingMerging.comp", "main", TYPE_COMP, SUBPASS_VOLUME_ADAPTIVE_MIPMAPPING_MERGING, 0,0 },
			{ "GridRaymarching.comp", "main", TYPE_COMP, SUBPASS_VOLUME_ADAPTIVE_RAYMARCHING, 0, 0 }
		};
	}

	namespace
	{
		//Returns content of the file read into a string
		std::string ReadFile(const std::string& fileName)
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

		//Returns the corresponding shader kind based on the shader type
		shaderc_shader_kind GetShaderKind(ShaderManager::Type type)
		{
			switch (type)
			{
			case ShaderManager::TYPE_VERT:
				return shaderc_glsl_vertex_shader;
			case ShaderManager::TYPE_FRAG:
				return shaderc_glsl_fragment_shader;
			case ShaderManager::TYPE_COMP:
				return shaderc_glsl_compute_shader;
			case ShaderManager::TYPE_GEOM:
				return shaderc_glsl_geometry_shader;
			default:
				printf("[WARNING] Recompile shader: invalid shader type\n");
				assert(false);
				return shaderc_glsl_vertex_shader;
			}
		}

		void PrintWarnings(const shaderc::SpvCompilationResult& compileResult)
		{
			const auto warningCount = compileResult.GetNumWarnings();
			if (warningCount > 0)
			{
				std::cout << "Warnings " << warningCount << std::endl;
			}
		}

		void PrintWarnings(const shaderc::SpvCompilationResult& compileResult, const std::string& shader)
		{
			const auto warningCount = compileResult.GetNumWarnings();
			if (warningCount > 0)
			{
				std::cout << shader << std::endl;
				PrintWarnings(compileResult);
				std::cout << compileResult.GetErrorMessage() << std::endl;
			}
		}

		//Output error count as well as error messages
		void PrintErrors(const shaderc::SpvCompilationResult& compileResult, const std::string& shader)
		{
			const auto errorCount = compileResult.GetNumErrors();
			if (errorCount > 0)
			{
				std::cout << shader << std::endl;
				PrintWarnings(compileResult);
				std::cout << "Errors " << errorCount << std::endl <<
					compileResult.GetErrorMessage() << std::endl;
			}
		}
		bool CreateShaderModule(VkDevice device, const shaderc::SpvCompilationResult& compileResult, VkShaderModule& shaderModule)
		{
			const auto begin = compileResult.begin();
			const auto end = compileResult.end();
			assert(begin != nullptr && end != nullptr);

			VkShaderModuleCreateInfo shaderInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
			shaderInfo.codeSize = (end - begin) * 4;
			shaderInfo.pCode = begin;

			VkShaderModule module = VK_NULL_HANDLE;
			const auto result = vkCreateShaderModule(device, &shaderInfo, nullptr, &module);
			assert(result == VK_SUCCESS);
			if (result != VK_SUCCESS)
			{
				return false;
			}

			shaderModule = module;
			return true;
		}
		VkShaderStageFlagBits GetStageFlags(ShaderManager::Type type)
		{
			switch (type)
			{
			case ShaderManager::TYPE_VERT:
				return VK_SHADER_STAGE_VERTEX_BIT;
			case ShaderManager::TYPE_FRAG:
				return VK_SHADER_STAGE_FRAGMENT_BIT;
			case ShaderManager::TYPE_COMP:
				return VK_SHADER_STAGE_COMPUTE_BIT;
			case ShaderManager::TYPE_GEOM:
				return VK_SHADER_STAGE_GEOMETRY_BIT;
			default:
				printf("[WARNING] GetCreateInfo: invalid shader type\n");
				assert(false);
				return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
			}
		}
		VkPipelineShaderStageCreateInfo FillCreateInfo(ShaderManager::Type type, VkShaderModule module, const char* entryName)
		{
			VkPipelineShaderStageCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
			createInfo.stage = GetStageFlags(type);
			createInfo.module = module;
			createInfo.pName = entryName;
			//TODO add Specialization constants
			//https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#pipelines-specialization-constants
			createInfo.pSpecializationInfo;

			return createInfo;
		}
	}

	bool ShaderManager::ReLoad(VkDevice device)
	{
		//temp containers during compilation
		std::vector<VkShaderModule> shaderModules;
		std::vector<std::vector<VkPipelineShaderStageCreateInfo>> createInfo(Renderer::SUBPASS_MAX);

		//set to true if one of the shaders fails to compile
		bool failed = false;
		for (size_t i = 0; i < shaderInfos_.size(); ++i)
		{
			const auto& info = shaderInfos_[i];

			const auto compileResult = CompileShader(static_cast<int>(i));
			const auto compileStatus = compileResult.GetCompilationStatus();
			if (compileStatus != shaderc_compilation_status_success)
			{
				//Continue compilation of other shaders to output all errors
				failed = true;
				PrintErrors(compileResult, info.fileName);
			}
			else
			{
				PrintWarnings(compileResult, info.fileName);
				VkShaderModule shaderModule = VK_NULL_HANDLE;
				if (CreateShaderModule(device, compileResult, shaderModule))
				{
					createInfo[info.pass].push_back(FillCreateInfo(info.type, shaderModule, info.functionName.data()));
					shaderModules.push_back(shaderModule);
				}
				else
				{
					failed = true;
				}
			}
		}



		//release old shaders and replace with new ones
		if (!failed)
		{
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

		return !failed;
	}

	shaderc::SpvCompilationResult ShaderManager::CompileShader(int shaderIndex)
	{
		const auto& info = shaderInfos_[shaderIndex];

		auto& fileContent = ReadFile(shaderSourceFolder_ + info.fileName);
		shaderc::CompileOptions compileOptions;

		return compiler_.CompileGlslToSpv(fileContent.data(), fileContent.size(),
			GetShaderKind(info.type), info.fileName.data(),
			info.functionName.data(), compileOptions);
	}

	void ShaderManager::Release(VkDevice device)
	{
		//wait until shaders are not used anymore
		vkDeviceWaitIdle(device);

		for (auto& s : shaders_)
		{
			vkDestroyShaderModule(device, s, nullptr);
		}
		shaders_.clear();

		shaders_.resize(shaderInfos_.size());
	}

	const std::vector<VkPipelineShaderStageCreateInfo>& ShaderManager::GetShaderStageInfo(Renderer::SubPassType pass) const
	{
		return createInfo_[pass];
	}
}