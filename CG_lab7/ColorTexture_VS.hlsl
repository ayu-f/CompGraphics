#include "CBScene.hlsli"

cbuffer ModelBuffer : register (b1)
{
    float4x4 model;
    float4x4 normTransform;
    float4 lightParams;
    float4 baseColor;
};

struct VSInput
{
    float3 pos : POSITION;
    float3 norm : NORMAL;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float4 worldPos : POSITION;
    float3 norm : NORMAL;
};

VSOutput vs(VSInput vertex)
{
    VSOutput result;
    result.worldPos = mul(model, float4(vertex.pos, 1.0));
    result.pos = mul(vp, result.worldPos);
    result.norm = mul(normTransform, float4(vertex.norm, 0.0)).xyz;

    return result;
}
