#include "LightCalc.hlsli"


Texture2D colorTexture : register (t0);

#ifdef USE_NORMAL_MAP
Texture2D normalMapTexture : register (t1);
#endif //USE_NORMAL_MAP

SamplerState colorSampler : register(s0);


cbuffer ModelBuffer : register (b1)
{
    float4x4 model;
    float4x4 normTransform;
    float4 lightParams; 
    float4 baseColor; 
};

struct VSOutput
{
    float4 pos : SV_Position;
    float4 worldPos : POSITION;
    float3 tang : TANGENT;
    float3 norm : NORMAL;
    float2 uv : TEXCOORD;
};

float4 ps(VSOutput pixel) : SV_Target0
{
    float3 color = colorTexture.Sample(colorSampler, pixel.uv).xyz;

#ifdef USE_NORMAL_MAP
    float3 pixNorm = normalize(pixel.norm);
    float3 pixeTang = normalize(pixel.tang);
    float3 binorm = cross(pixeTang, pixNorm);
    float3 localNorm = normalMapTexture.Sample(colorSampler, pixel.uv).xyz * 2.0 - float3(1.0, 1.0, 1.0);
    float3 normal = normalize(localNorm.x * pixeTang + localNorm.y * binorm + localNorm.z * pixNorm);
#else
    float3 normal = normalize(pixel.norm);
#endif //USE_NORMAL_MAP

#ifdef USE_LIGHT

    #ifdef USE_TRANSPARENCY

    color = CalculateColor(color, normal, pixel.worldPos.xyz, lightParams, true);

    #else

    color = CalculateColor(color, normal, pixel.worldPos.xyz, lightParams, false);

    #endif //USE_TRANSPARENCY

#endif //USE_LIGHT

#ifdef USE_TRANSPARENCY
    return float4(color, baseColor.w);
#else
    return float4(color, 1.0);
#endif //USE_TRANSPARENCY
}

