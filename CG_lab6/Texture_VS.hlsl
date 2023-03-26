#include "CBScene.hlsli"

cbuffer SceneBuffer : register (b1) {
    float4x4 model;
    float4x4 normTransform;
    float4 lightParams; 
    float4 baseColor;
};

struct VSInput
{
    float3 pos : POSITION;
    float3 tang : TANGENT;
    float3 norm : NORMAL;
    float2 uv : TEXCOORD;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float4 worldPos : POSITION;
    float3 tang : TANGENT;
    float3 norm : NORMAL;
    float2 uv : TEXCOORD;
};

VSOutput vs(VSInput vertex)
{
    VSOutput result;
    result.worldPos = mul(model, float4(vertex.pos, 1.0));
    result.pos = mul(vp, result.worldPos);
    result.tang = mul(normTransform, float4(vertex.tang, 0.0)).xyz;
    result.norm = mul(normTransform, float4(vertex.norm, 0.0)).xyz;
    result.uv = vertex.uv;

    return result;
}
