#include "CBScene.hlsli"

struct ModelBuffer
{
    float4x4 model;
    float4x4 normTransform;
    float4 lightParams; //x - ambientCoef, y - diffuseCoef, z - specularCoef, w - shinines
    float4 baseColor; //xyz - color, w - opacity
    int4 colorTextureId_normalMapId_useLight;
};

cbuffer ModelBufferInst : register (b1)
{
    ModelBuffer modelBuffer[100];
};

#ifdef USE_VISIBLE_ID
cbuffer VisibleInstIdBuffer : register (b2)
{
    uint4 ids[100];
};
#endif //USE_VISIBLE_ID

struct VSInput
{
    float3 pos : POSITION;
    float3 tang : TANGENT;
    float3 norm : NORMAL;
    float2 uv : TEXCOORD;
    uint instanceId : SV_InstanceID;
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

VSOutput vs(VSInput vertex)
{
    VSOutput result;
#ifdef USE_VISIBLE_ID
    uint idx = ids[vertex.instanceId].x;
#else
    uint idx = vertex.instanceId;
#endif //USE_VISIBLE_ID
    result.worldPos = mul(modelBuffer[idx].model, float4(vertex.pos, 1.0));
    result.pos = mul(vp, result.worldPos);
    result.tang = mul(modelBuffer[idx].normTransform, float4(vertex.tang, 0.0)).xyz;
    result.norm = mul(modelBuffer[idx].normTransform, float4(vertex.norm, 0.0)).xyz;
    result.uv = vertex.uv;
    result.instanceId = idx;

    return result;
}
