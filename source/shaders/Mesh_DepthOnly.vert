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

layout(set = 0, binding = 0) uniform perFrameBuffer
{
	mat4 gridTransform_;
};

out gl_PerVertex
{
	vec4 gl_Position;
};

const vec3 boxPos[8] = { vec3(0,0,0), vec3(1,0,0), vec3(0,1,0), vec3(1,1,0), vec3(0,0,1), vec3(1,0,1), vec3(0,1,1), vec3(1,1,1) };
const int boxTriangles[36] = { 0,2,1,1,2,3, 1,3,5,5,3,7, 5,7,4,4,7,6, 4,6,0,0,6,2, 2,7,3,2,6,7, 0,1,5,0,5,4 };

void main()
{
	vec4 pos = vec4(boxPos[boxTriangles[gl_VertexIndex]], 1.0);
	gl_Position = gridTransform_ * pos;
}