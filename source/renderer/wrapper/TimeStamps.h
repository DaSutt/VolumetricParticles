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

namespace Wrapper
{
	enum TimeStamps
	{
		TIMESTAMP_SHADOW_MAP,
		TIMESTAMP_MESH,
		TIMESTAMP_GRID_GLOBAL,
		TIMESTAMP_GRID_GROUND_FOG,
		TIMESTAMP_GRID_PARTICLES,
		TIMESTAMP_GRID_NEIGHBOR_UPDATE,
		TIMESTAMP_GRID_MIPMAPPING_0,
		TIMESTAMP_GRID_MIPMAPPING_1,
		TIMESTAMP_GRID_MIPMAPPING_MERGIN_0,
		TIMESTAMP_GRID_MIPMAPPING_MERGIN_1,
		TIMESTAMP_GRID_RAYMARCHING,
		TIMESTAMP_POSTPROCESS,
		TIMESTAMP_PASS_GUI,
		TIMESTAMP_MAX
	};
}