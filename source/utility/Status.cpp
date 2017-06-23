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

#include "Status.h"

#include "..\fileIO\FileDialog.h"

#include "..\Camera.h"
#include "..\renderer\scene\adaptiveGrid\subpasses\ParticleSystems.h"
#include "..\renderer\scene\adaptiveGrid\AdaptiveGridConstants.h"

Status::CameraStatus Status::cameraStatus_;
Status::SurfaceStatus Status::surfaceStatus_;
Status::DirectionalLightStatus Status::dirLightStatus_;
Status::GroundFogStatus Status::groundFogStatus_;
Status::GridStatus Status::gridStatus_;
Status::SceneStatus Status::sceneStatus_;

Renderer::ParticleSystems* Status::particleSystemsPtr_ = nullptr;


namespace
{
	float RadToDeg(float rad)
	{
		return rad * (180.0f / glm::pi<float>());
	}

	void SaveMedium(std::ofstream& file, std::string name, float absorption, float scattering, float phaseG)
	{
		file << "\nMakeNamedMedium \"" << name << "\"" <<
			"\n\t\"string type\"[\"homogeneous\"]" <<
			"\n\t\"rgb sigma_a\"[" << absorption << " " << absorption << " " << absorption << "]" <<
			"\n\t\"rgb sigma_s\"[" << scattering << " " << scattering << " " << scattering << "]" <<
			"\n\t\"float g\"[" << phaseG << "]\n";
	}

	void SaveGround(std::ofstream& file)
	{
		file <<	R"(
AttributeBegin
	Material "matte"
		"rgb Kd"[1 1 1]
		"float sigma"[0.3]
	Translate 0 0 0
	Scale 1 1 1
	Shape "trianglemesh"
		"integer indices"[0 1 2 2 3 0]
		"point P"[-15 0 -15 15 0 -15 15 0 15 -15 0 15]
AttributeEnd
)";
	}

	void SaveParticleSphere(std::ofstream& file, const Renderer::Particle& particle, const glm::vec3& worldOffset, float radius, std::string mediumName)
	{
		const auto pos = particle.position + worldOffset;

		file <<
			"\nAttributeBegin\n" <<
			"\n\tMediumInterface \"" << mediumName << "\" \"\"" <<
			"\n\tMaterial \"\"" <<
			"\n\tTranslate " << pos.x << " " << -pos.y << " " << pos.z <<
			"\n\tShape \"sphere\"" <<
			"\n\t\t\"float radius\"[" << radius << "]" <<
			"\nAttributeEnd";
	}

	void SaveCube(std::ofstream& file, const glm::vec3& translation, const glm::vec3& scale)
	{
		file <<
			"\nTranslate " << translation.x << " " << translation.y << " " << translation.z <<
			"\nScale " << scale.x << " " << scale.y << " " << scale.z <<
			R"(
Shape "trianglemesh"
	"integer indices" [2 1 0  0 3 2  6 5 1  1 2 6  7 4 5  5 6 7  3 0 4  4 7 3  6 2 3  3 7 6  0 1 5  5 4 0]
  "point P" [-0.5 -0.5 -0.5  0.5 -0.5 -0.5  0.5 0.5 -0.5  -0.5 0.5 -0.5  -0.5 -0.5 0.5  0.5 -0.5 0.5  0.5 0.5 0.5  -0.5 0.5 0.5])";
	}
}

void Status::UpdateCameraState(Camera* camera)
{
	cameraStatus_.pos = camera->GetPosition();
	cameraStatus_.lookAt = camera->GetForward();
	cameraStatus_.up = camera->GetUp();
	cameraStatus_.fov = camera->GetFOVHorizontal();
}
void Status::CameraStatus::Save(std::ofstream& file)
{
	file <<
		"\nLookAt " << pos.x << " " << -pos.y << " " << pos.z <<
		"\n\t" << pos.x + lookAt.x << " " << -pos.y + -lookAt.y << " " << pos.z + lookAt.z <<
		"\n\t" << up.x << " " << -up.y << " " << up.z <<
		"\nCamera \"perspective\"" <<
		"\n\t\"float fov\" [" << RadToDeg(fov) << "]\n";
}

void Status::UpdateDirectionalLight(const glm::vec3& direction, const glm::vec3 irradiance)
{
	dirLightStatus_.direction = direction;
	dirLightStatus_.irradiance = irradiance;
}
void Status::DirectionalLightStatus::Save(std::ofstream& file)
{
	file <<
		"\nAttributeBegin" <<
			"\n\tLightSource \"distant\"" <<
				"\n\t\"point from\"[" << direction.x << " " << -direction.y << " " << direction.z << "]" <<
				"\n\t\"point to\"[0 0 0]" <<
				"\n\t\"rgb L\"[" << irradiance.x << " " << irradiance.y << " " << irradiance.z << "]" <<
		"\nAttributeEnd\n";
}

void Status::UpdateSurfaceSize(int width, int height)
{
	surfaceStatus_.width = width;
	surfaceStatus_.height = height;
}
void Status::SurfaceStatus::Save(std::ofstream& file, const std::string& outputName)
{
	file <<
		"\nSampler \"halton\"" <<
		"\n\t\"integer pixelsamples\"[16]\n" <<

		"\nFilm \"image\"" <<
		"\n\t\"integer xresolution\"[" << width << "]" <<
		"\n\t\"integer yresolution\"[" << height << "]" <<
		"\n\t\"string filename\"[\"_" << outputName << ".png\"]\n" <<

		"\nIntegrator \"volpath\"" <<
		"\n\t\"integer maxdepth\"[1]\n" <<

		"\nAccelerator \"bvh\"\n";
}

void Status::UpdateGroundFog(const std::string& fileName, const std::string& mediumName, 
	const glm::vec3& scale, float yOffset)
{
	groundFogStatus_.fileName = fileName;
	groundFogStatus_.mediumName = mediumName;
	groundFogStatus_.scale = scale;
	groundFogStatus_.yOffset = yOffset;
}
void Status::GroundFogStatus::SaveInfo(std::ofstream& file)
{
	if (!fileName.empty())
	{
		file <<
			"\nAttributeBegin" <<
			"\n\tInclude \"" << fileName << "\"" <<
			"\nAttributeEnd\n";
	}
}
void Status::GroundFogStatus::SaveVolume(std::ofstream& file)
{
	if (!mediumName.empty())
	{
		const glm::vec3 worldOffset = gridStatus_.min + gridStatus_.size * 0.5f;
		const float cellSize = gridStatus_.size.x / GridConstants::nodeResolution;
		const glm::vec3 translation = { worldOffset.x, 
			yOffset, worldOffset.z};

		file <<
			"\nAttributeBegin" <<
			"\n\tMediumInterface \"" << mediumName << "\" \"\"" <<
			"\n\tMaterial \"\"";
		SaveCube(file, translation, scale);
		file <<
			"\nAttributeEnd";
	}
}

void Status::UpdateGrid(const glm::vec3& size, const glm::vec3& min)
{
	gridStatus_.size = size;
	gridStatus_.min = min;
}
glm::vec3 Status::GridStatus::TransformGridToPBRT(const glm::vec3& coordinate) const
{
	const glm::vec3 worldPosition = coordinate + min;
	return worldPosition * glm::vec3(1.0f, -1.0f, 1.0f);
}

void Status::UpdateScene(const std::string& fileName)
{
	sceneStatus_.path = FileIO::ReplaceExtension(fileName, "pbrt");
	sceneStatus_.path = FileIO::ChangeToBackSlashes(sceneStatus_.path);
}
void Status::SceneStatus::SaveSceneInclude(std::ofstream& file)
{
	if (path.empty())
	{
		return;
	}

	std::ifstream sceneFile;
	sceneFile.open(path.c_str());
	//Check if file exists
	if (sceneFile)
	{
		file << "\nInclude \"" << path << "\"\n";
	}
	else
	{
		printf("WARNING: scene file was not exported as obj for PBRT import\n");
	}
}

void Status::Save()
{
  const auto filePath = FileIO::SaveFileDialog(L"pbrt");
  const auto fileName = FileIO::GetFileNameWithExtension(filePath);

  std::ofstream file;
  file.open(filePath.c_str(), std::ios::out);
  if (file)
  {
		cameraStatus_.Save(file);
		surfaceStatus_.Save(file, fileName);
		
		const std::string mediumName = "homogeneousVolume";
		SaveMedium(file, mediumName, 0.0f, 10.0f, 0.0f);

		file << "\nWorldBegin\n";
		groundFogStatus_.SaveInfo(file);
		dirLightStatus_.Save(file);
		
		const auto& particles = particleSystemsPtr_->GetParticles();
		const auto& radi = particleSystemsPtr_->GetRadi();

		const auto& worldOffset = particleSystemsPtr_->GetWorldOffset();
		for (size_t i = 0; i < particles.size(); ++i)
		{
			SaveParticleSphere(file, particles[i], worldOffset, radi[i], mediumName);
		}

		groundFogStatus_.SaveVolume(file);
		sceneStatus_.SaveSceneInclude(file);

		file << "\nWorldEnd\n";
  }
}