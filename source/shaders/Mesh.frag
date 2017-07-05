#version 450
#extension GL_ARB_separate_shader_objects : enable

#define M_PI 3.1415926535897932384626433832795

//Code from http://www.frostbite.com/2014/11/moving-frostbite-to-pbr/

struct DirectionalLight
{
  vec4 lightVector;
  vec4 irradiance;
};

struct Material
{
	vec3 baseColor;
};

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(binding = 2) uniform PerFrameBuffer
{
	mat4x4 shadowCascades_[4];
  DirectionalLight dirLight;
  vec3 cameraPosW;
  float padding;
} perFrame;

layout(binding = 3) uniform sampler2DArray shadowMap;
layout(set = 1, binding = 0) uniform sampler2D baseColorTexture;

vec3 F_Schlick(vec3 f0, float f90, float LdotH)
{
  return f0 + (f90 - f0) * pow(1.0 - LdotH, 5.0);
}

vec3 f0 = vec3(0.05, 0.05, 0.05);
float fd90 = 1.0;
float roughness = 0.75f;

float DiffuseBRDF(float NdotV, float NdotL, float LdotH, float linearRoughness)
{
	float energyBias = mix (0, 0.5 , linearRoughness );
	float energyFactor = mix(1.0 , 1.0 / 1.51 , linearRoughness );
	float fd90 = energyBias + 2.0 * LdotH * LdotH * linearRoughness ;
	vec3 f0 = vec3 (1.0, 1.0, 1.0);
	float lightScatter = F_Schlick (f0 , fd90 , NdotL ).r;
	float viewScatter = F_Schlick (f0 , fd90 , NdotV ).r;

	return lightScatter * energyFactor * viewScatter;
}

float V_SmithGGXCorrelated ( float NdotL , float NdotV , float alphaG )
{
	float alphaG2 = alphaG * alphaG ;
	float Lambda_GGXV = NdotL * sqrt ((- NdotV * alphaG2 + NdotV ) * NdotV + alphaG2 );
	float Lambda_GGXL = NdotV * sqrt ((- NdotL * alphaG2 + NdotL ) * NdotL + alphaG2 );

	return 0.5 / ( Lambda_GGXV + Lambda_GGXL );
}

float D_GGX ( float NdotH , float m)
{
	float m2 = m * m;
	float f = ( NdotH * m2 - NdotH ) * NdotH + 1;
	return m2 / (f * f);
}

vec2 Visibility(vec3 inPos)
{
  vec2 visibility = vec2(1.0f,-1.0f);
  
  //TODO: select correct cascade
  vec4 lightCoord = perFrame.shadowCascades_[0] * vec4(inPos, 1.0);
  if(min(lightCoord.x, lightCoord.y) >= 0.0 && max(lightCoord.x, lightCoord.y) <= 1.0)
  {
    if(texture(shadowMap, vec3(lightCoord.xy, 0)).r + 0.05f < lightCoord.z)
    {
      visibility.x = 0.0f;
    }
  }
  //vec4 lightCoord = vec4(0);
  //for(int i = 0; i < 1; ++i)
  //{
  //  lightCoord = perFrame.shadowCascades_[i] * vec4(inPos, 1.0);
  //  if(min(lightCoord.x, lightCoord.y) >= 0.0 && max(lightCoord.x, lightCoord.y) <= 1.0)
  //  {
  //    visibility.y = i;
  //    if(texture(shadowMap, vec3(lightCoord.xy, i)).r + 0.05f < lightCoord.z)
  //    {
  //      visibility.x = 0.2f;
  //    }
  //    return visibility;
  //  }
  //}
  return visibility;
}

void main()
{
	vec3 lightVector = perFrame.dirLight.lightVector.xyz;

  vec3 normal = normalize(inNormal);
  vec3 view = normalize(perFrame.cameraPosW - inPos);
  vec3 halfVector = normalize(view + perFrame.dirLight.lightVector.xyz);

  float LdotH = clamp(dot(lightVector, halfVector), 0.0, 1.0);
	float NdotV = abs( dot (normal, view));
	float NdotL = clamp( dot (normal, lightVector), 0.0, 1.0);
	float NdotH = clamp( dot(normal, halfVector), 0.0, 1.0);

  vec3 fresnel = F_Schlick(f0, fd90, LdotH);
	float distribution = D_GGX(NdotH, roughness);
	float masking = V_SmithGGXCorrelated(NdotL, NdotV, roughness);
  vec3 specular = distribution * fresnel * masking / M_PI;

	vec3 baseColor = vec3(1.0);
  //baseColor = vec3(1.0, 1.0, 1.0);
  vec3 diffuse = baseColor * DiffuseBRDF(NdotV, NdotL, LdotH, roughness) / M_PI;

	vec3 irradiance = perFrame.dirLight.irradiance.xyz * NdotL;
  vec2 visibility = Visibility(inPos);
  vec3 radiance = (diffuse + specular) * irradiance * visibility.x;
  //if(visibility.y == 0.0)
  //{
  //  radiance = radiance * 0.5 + vec3(0.5,0.0,0.0);
  //}
  //else if(visibility.y == 1.0)
  //{
  //  radiance = radiance * 0.5 + vec3(0.0, 0.5 ,0.0);
  //}
  //else if(visibility.y == 2.0)
  //{
  //  radiance = radiance * 0.5 + vec3(0,0,0.5);
  //}
  //else
  //{
  //  radiance = radiance * 0.5 + vec3(0.5,0.5,0.0);
  //}
  outColor = vec4(radiance, 1.0);
}
