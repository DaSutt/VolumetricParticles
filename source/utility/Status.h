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
#include <iostream>
#include <fstream>

class Camera;

namespace Renderer
{
	class ParticleSystems;
}

class Status
{
public:
  //todo add open file dialog
  //save as pbrt file for import
  static void Save();

	static void UpdateCameraState(Camera* camera);
	static void UpdateDirectionalLight(const glm::vec3& direction, const glm::vec3 irradiance);
	static void UpdateSurfaceSize(int width, int height);
	//Scale and offset in world space
	//Y offset is in grid space [0,gridSize] with pos y down
	static void UpdateGroundFog(const std::string& fileName, const std::string& mediumName, 
		const glm::vec3& scale, float yOffset);
	static void UpdateScene(const std::string& fileName);
	static void UpdateGrid(const glm::vec3& size, const glm::vec3& min);

	static void SetParticleSystems(Renderer::ParticleSystems* particleSystems) { 
		particleSystemsPtr_ = particleSystems; }

	static const auto& GetGridStatus() { return gridStatus_; }
private:
	struct CameraStatus
	{
		glm::vec3 pos;
		glm::vec3 lookAt;
		glm::vec3 up;
		float fov;
		void Save(std::ofstream& file);
	};
	struct SurfaceStatus
	{
		int width;
		int height;
		void Save(std::ofstream& file, const std::string& outputName);
	};
	struct DirectionalLightStatus
	{
		glm::vec3 direction;
		glm::vec3 irradiance;
		void Save(std::ofstream& file);
	};
	struct GroundFogStatus
	{
		std::string fileName;
		std::string mediumName;
		glm::vec3 scale;
		float yOffset;
		void SaveInfo(std::ofstream& file);
		void SaveVolume(std::ofstream& file);
	};
	struct GridStatus
	{
		glm::vec3 size;
		glm::vec3 min;

		//Transform from grid space [0,gridSize] y pos down to [-offset, offset] y pos up
		glm::vec3 TransformGridToPBRT(const glm::vec3& coordinate) const;
	};
	struct SceneStatus
	{
		std::string path;
		void SaveSceneInclude(std::ofstream& file);
	};

	static CameraStatus cameraStatus_;
	static SurfaceStatus surfaceStatus_;
	static DirectionalLightStatus dirLightStatus_;
	static GroundFogStatus groundFogStatus_;
	static GridStatus gridStatus_;
	static SceneStatus sceneStatus_;

	static Renderer::ParticleSystems* particleSystemsPtr_;
};