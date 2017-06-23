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

#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(binding = 0) buffer WorldViewProjBuffer
{
  mat4 worldViewProj_[];
};

layout(binding = 1) buffer WorldInvTransposeBuffer
{
  mat4 world_[];
  mat4 worldInvTranspose_[];
};

layout(push_constant) uniform PerObjectPushConstant
{
  int index;
} perObject;

layout(location = 0) out vec3 pos;
layout(location = 1) out vec3 normal;
layout(location = 2) out vec2 uv;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
  gl_Position = worldViewProj_[perObject.index] * vec4(inPos, 1.0);
	pos = vec3(world_[perObject.index] * vec4(inPos, 1.0));
	normal = (worldInvTranspose_[perObject.index] * vec4(inNormal, 0.0f)).xyz;
	uv = inUV;
}
