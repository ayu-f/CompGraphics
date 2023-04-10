#include "LightCalc.hlsli"


#ifdef USE_TEXTURE
Texture2DArray colorTexture : register (t0);
#endif //USE_TEXTURE

#ifdef USE_NORMAL_MAP
Texture2DArray normalMapTexture : register (t1);
#endif //USE_NORMAL_MAP

SamplerState colorSampler : register(s0);

struct ModelBuffer
{
    float4x4 model;
    float4x4 normTransform;
    float4 lightParams;
    float4 baseColor;
    int4 modelInfo;
};

cbuffer ModelBufferInst : register (b1)
{
    ModelBuffer modelBuffer[100];
};

cbuffer VisibleInstIdBuffer : register (b2)
{
    uint4 ids[100];
};

struct VSOutput
{
    float4 pos : SV_Position;
    float4 worldPos : POSITION;
    float3 tang : TANGENT;
    float3 norm : NORMAL;
    float2 uv : TEXCOORD;
    nointerpolation uint instanceId : INST_ID;
};

float4 ps(VSOutput pixel) : SV_Target0
{
    uint idx = pixel.instanceId;
    float3 color = modelBuffer[idx].baseColor.xyz;

#ifdef USE_TEXTURE
    int colorTextureId = modelBuffer[idx].modelInfo.x;
    if (colorTextureId >= 0)
    {
        color = color * colorTexture.Sample(colorSampler, float3(pixel.uv, colorTextureId)).xyz;
    }
#endif //USE_TEXTURE


    float3 normal = normalize(pixel.norm);
#ifdef USE_NORMAL_MAP
    int normalMapId = modelBuffer[idx].modelInfo.y;
    if (normalMapId >= 0)
    {
        float3 localNorm = normalMapTexture.Sample(colorSampler, float3(pixel.uv, normalMapId)).xyz * 2.0 - float3(1.0, 1.0, 1.0);
        float3 tangent = normalize(pixel.tang);
        float3 binorm = cross(tangent, normal);
        normal = normalize(localNorm.x * tangent + localNorm.y * binorm + localNorm.z * normal);
    }
#endif //USE_NORMAL_MAP

#ifdef USE_TRANSPARENCY
    #define _IS_TRANSPARENT true
#else
    #define _IS_TRANSPARENT false
#endif //USE_TRANSPARENCY

#ifdef USE_LIGHT
    if (modelBuffer[idx].modelInfo.z != 0)
    {
        color = CalculateColor(color, normal, pixel.worldPos.xyz, modelBuffer[idx].lightParams, _IS_TRANSPARENT);
    }
#endif //USE_LIGHT

#undef _IS_TRANSPARENT

#ifdef USE_TRANSPARENCY
    return float4(color, modelBuffer[idx].baseColor.w);
#else
    return float4(color, 1.0);
#endif //USE_TRANSPARENCY
}

