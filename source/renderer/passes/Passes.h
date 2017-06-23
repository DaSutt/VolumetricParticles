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

namespace Renderer
{
  enum PassType
  {
    PASS_MESH,
    PASS_VOLUME,
    PASS_SHADOW_MAP,
    PASS_POSTPROCESS,
    PASS_GUI,
    PASS_MAX,
    PASS_NONE
  };

	enum SubPassType
	{
		SUBPASS_SHADOW_MAP,
		SUBPASS_VOLUME_ADAPTIVE_GLOBAL,
		SUBPASS_VOLUME_ADAPTIVE_GROUND_FOG,
		SUBPASS_VOLUME_ADAPTIVE_PARTICLES,
		SUBPASS_VOLUME_ADAPTIVE_DEBUG_FILLING,
		SUBPASS_VOLUME_ADAPTIVE_NEIGHBOR_UPDATE,
		SUBPASS_VOLUME_ADAPTIVE_MIPMAPPING,
		SUBPASS_VOLUME_ADAPTIVE_MIPMAPPING_MERGING,
    SUBPASS_VOLUME_ADAPTIVE_RAYMARCHING,
    SUBPASS_MESH,
    SUBPASS_MESH_DEPTH_ONLY,
    SUBPASS_GUI,
    SUBPASS_POSTPROCESS,
    SUBPASS_POSTPROCESS_DEBUG,
    SUBPASS_MAX,
    SUBPASS_NONE
  };

  enum GraphicPassType
  {
    GRAPHIC_PASS_SHADOW_MAP,
    GRAPHIC_PASS_MESH,
    GRAPHIC_PASS_GUI,
    GRAPHIC_PASS_POSTPROCESS,
    GRAPHIC_PASS_POSTPROCESS_DEBUG,
    GRAPHIC_PASS_MAX
  };

  enum VolumeSubPassType
  {
    VOLUME_SUBPASS_MEDIA,
    VOLUME_SUBPASS_LIGHT_SCATTERING,
    VOLUME_SUBPASS_RAYTRACING,
    VOLUME_SUBPASS_BLENDING,
    VOLUME_SUBPASS_MAX
  };

  GraphicPassType GetGraphicPass(SubPassType pass);
  SubPassType GetSubPass(VolumeSubPassType volumeSubPass);
}