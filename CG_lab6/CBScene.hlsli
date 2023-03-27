struct Light
{
    float4 pos;
    float4 color;
};

cbuffer ViewBuffer : register (b0)
{
    float4x4 vp;
    float4 cameraPosition;
    int4 lightCount;
    Light lights[10];
    float4 ambientColor;
};