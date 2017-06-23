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

#version 420

layout(push_constant) uniform PerBoundingBox
{
	mat4 worldViewProj_;
	vec4 color_;
} boundingBox;

const vec3 boxPos [8] = { vec3 (-.5,-.5,-.5), vec3 (.5,-.5,-.5), vec3 (-.5,.5,-.5), vec3 (.5,.5,-.5), vec3 (-.5,-.5,.5), vec3 (.5,-.5,.5), vec3 (-.5,.5,.5), vec3 (.5,.5,.5) };
const int boxLines [24] = { 0,1,0,2,0,4,1,3,1,5,2,3,2,6,3,7,4,5,4,6,5,7,6,7 };

layout(location = 0) out vec4 outColor;

void main()
{
	vec4 pos = vec4(boxPos[boxLines[gl_VertexIndex]], 1.0);
	gl_Position = boundingBox.worldViewProj_ * pos;
	outColor = boundingBox.color_;
}