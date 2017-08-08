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

#include <array>
#include <memory>
#include "..\..\..\scene\components\AABoundingBox.h"
#include "GridLevel.h"
#include "..\..\ShadowMap.h"
#include "debug\DebugTraversal.h"
#include "GroundFog.h"
#include "MipMapping.h"
#include "NeighborCells.h"
#include "ImageAtlas.h"


#include "subpasses\ParticleSystems.h"
#include "subpasses\DebugFilling.h"
#include "subpasses\GlobalVolume.h"

#include <vulkan\vulkan.h>

class Scene;
class ParticleSystem;

namespace Renderer
{
  class ImageManager;
  class BufferManager;
  class ShaderBindingManager;
  class Surface;
  class ShadowMap;

  class AdaptiveGrid
  {
  public:
		struct BoundingBoxData
		{
			glm::mat4 world;
			glm::vec4 color;
		};

    enum Pass
    {
			GRID_PASS_GLOBAL,
      GRID_PASS_GROUND_FOG,
      GRID_PASS_PARTICLES,
			GRID_PASS_DEBUG_FILLING,
			GRID_PASS_MIPMAPPING,
			GRID_PASS_MIPMAPPING_MERGING,
			GRID_PASS_NEIGHBOR_UPDATE,
      GRID_PASS_RAYMARCHING
    };

		AdaptiveGrid(float worldCellSize);
    //Calculates the required number of levels for the given world to have the min cell size
    void SetWorldExtent(const glm::vec3& min, const glm::vec3& max);
    //Resize constant buffers on scene load
    void ResizeConstantBuffers(BufferManager* bufferManager);
    //Create constant buffers, empty texture atlas and buffers for nodes and childs(resized later)
    void RequestResources(ImageManager* imageManager, BufferManager* bufferManager, int frameCount);
    void SetImageIndices(int raymarching, int depth, int shadowMap, int noise);
		 
		void OnLoadScene(const Scene* scene);
		void UpdateParticles(float dt);

    //Insert smoke volumes and particle systems into grid levels
    void Update(BufferManager* bufferManager, ImageManager* imageManager, Scene* scene, Surface* surface, ShadowMap* shadowMap, int frameIndex);
    //Needs to be called after node, child buffers were resized
    void UpdateShaderBindings(ShaderBindingManager* bindingManager);

    int GetShaderBinding(ShaderBindingManager* bindingManager, Pass pass);

		void Dispatch(QueueManager* queueManager, ImageManager* imageManager, BufferManager* bufferManager, 
			VkCommandBuffer commandBuffer, Pass pass, int frameIndex, int level);

		void Raymarch(ImageManager* imageManager, VkCommandBuffer commandBuffer, uint32_t width, 
			uint32_t height, int frameIndex);
		void UpdateDebugTraversal(QueueManager* queueManager, BufferManager* bufferManager, ImageManager* imageManager);

    const auto& GetDebugBoundingBoxes() const { return debugBoundingBoxes_; }
    bool Resized() const { return resizing_; }

		//Methods to get data for debugging
		const auto& GetRaymarchingData() const { return raymarchingData_; }
		const auto& GetLevelData() const { return gridLevelData_; }

		int GetMaxParentLevel() const { return mostDetailedParentLevel_; }
  private:
    enum ConstantBuffer
    {
      CB_RAYMARCHING,
      CB_RAYMARCHING_LEVELS,
      CB_MAX
    };

    enum GpuResources
    {
      GPU_BUFFER_NODE_INFOS,
      GPU_BUFFER_ACTIVE_BITS,
			GPU_BUFFER_BIT_COUNTS,
      GPU_BUFFER_CHILDS,
      GPU_MAX
    };

    struct GlobalMediumData
    {
      float scattering;
      float extinction;
      float phaseG;
      int atlasSideLength;
			int imageResolutionOffset;
    };

    

    struct VolumeData
    {
      glm::vec3 min;
      float padding1;
      glm::vec3 max;
      float padding2;
    };

    //Constant buffers are filled with global scene data, camera values, screen size
    void UpdateCBData(Scene* scene, Surface* surface, ShadowMap* shadowMap);
    //For each volume calculate active nodes per level, recreate the grid
    void UpdateGrid(Scene* scene);
    //If more space needed resize
		std::array<VkDeviceSize, GPU_MAX> AdaptiveGrid::GetGpuResourceSize();
		void ResizeGpuResources(BufferManager* bufferManager, ImageManager* imageManager);
		void* GetDebugBufferCopyDst(GridLevel::BufferType type, int size);
		void UpdateGpuResources(BufferManager* bufferManager, int frameIndex);
		//Stores bounding boxes of active nodes for debug rendering
    void UpdateBoundingBoxes();

    //Calculates maximum scale possible based on resolutions and the world cell size
    glm::vec3 CalcMaxScale(std::vector<int> levelResolutions, int textureResolution);


    std::array<int, CB_MAX> cbIndices_;
    GlobalMediumData globalMediumData_;
    GlobalMediumData volumeMediaData_;
    RaymarchingData raymarchingData_;
    std::vector<LevelData> gridLevelData_;

    std::vector<VolumeData> volumeData_;
    std::vector<int> volumeIndices_;
    std::vector<GridLevel::NodePushData> nodePushData_;
    std::map<int, std::vector<int>> volumeIndiceMapping_;

    int raymarchingImageIndex_ = -1;
    int depthImageIndex_ = -1;
    int shadowMapIndex_ = -1;
		int noiseImageIndex_ = -1;

    //Gpu resources shared by all grid levels
    std::array<GpuResource, GPU_MAX> gpuResources_;
    int imageOffset_ = 0;

    //Minimum size of texture resolution in world space
    const float worldCellSize_;

    AxisAlignedBoundingBox worldBoundingBox_;
		float radius_ = 0.0f;
		std::vector<GridLevel> gridLevels_;
		
    std::vector<BoundingBoxData> debugBoundingBoxes_;

    int frameCount_ = 0;
    bool initialized_ = false;
    bool resizing_ = false;

		DebugTraversal debugTraversal_;
		bool previousFrameTraversal_ = false;

		GlobalVolume globalVolume_;
		GroundFog groundFog_;
		ParticleSystems particleSystems_;
		MipMapping mipMapping_;
		DebugFilling debugFilling_;
		NeighborCells neighborCells_;
		ImageAtlas imageAtlas_;

		int mostDetailedParentLevel_ = 0;
		
		bool mipMappingStarted_ = false;
  };
}