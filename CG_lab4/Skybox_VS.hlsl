cbuffer ViewTransformsBuffer : register (b0)
{
    float4x4 vp;
    float4 cameraPos;
};

cbuffer SceneTransformsBuffer : register (b1)
{
    float4x4 model;
};

struct VSInput
{
    float3 pos : POSITION;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float3 localPos : POSITION1;
};

VSOutput vs(VSInput vertex) {
    VSOutput result;
    float4 pos = mul(model, float4(vertex.pos, 1.0)) + float4(cameraPos.xyz, 0.0);
    result.pos = mul(vp, pos);
    result.localPos = vertex.pos;
    return result;
}
