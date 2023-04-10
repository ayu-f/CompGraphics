#include "LightCalc.hlsli"


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
    float3 norm : NORMAL;
};

float4 ps(VSOutput pixel) : SV_Target0
{
    float3 color = baseColor.xyz;
    float3 normal = normalize(pixel.norm);

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

