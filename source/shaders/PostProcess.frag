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

const int DEBUG_SCREEN_DIVISION = 2;

layout(location = 0) out vec4 outColor;

layout(binding = 0, rgba16f) uniform image2D meshRenderingResults;
layout(binding = 1, r16f) uniform image2D raymarchingResults_;
layout(binding = 2) uniform sampler2DArray noiseTextureArray_;

layout(binding = 3) uniform perFrameData
{
	vec4 randomness_;
	vec2 screenSize_;
};

vec4 SampleNoise(ivec2 imageCoord)
{
	vec2 texCoord = (imageCoord / screenSize_) + randomness_.xy;
	vec4 noise = texture(noiseTextureArray_, vec3(texCoord, 0));
	noise = noise * 2.0 - 1.0;
	noise = sign(noise) * (1.0 -sqrt(1.0 - abs(noise)));
	return noise;
}

void main()
{
	ivec2 imageCoord = ivec2(gl_FragCoord.xy);

	vec4 noise = SampleNoise(imageCoord);
	vec4 opaqueColor = imageLoad(meshRenderingResults, imageCoord);
	vec4 raymarchingResults = imageLoad(raymarchingResults_, imageCoord);
	vec3 finalColor = opaqueColor.xyz * raymarchingResults.a + raymarchingResults.xyz;
	finalColor = pow(finalColor, vec3(1.0 / 2.2));
	finalColor += noise.rgb / 255.0;
	//outColor = vec4(raymarchingResults.xyz,  1.0f);
	outColor = vec4(finalColor, 1.0);
}