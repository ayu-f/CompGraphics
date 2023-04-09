struct VSOutput
{
	float4 pos : SV_Position;
	float2 uv : TEXCOORD;
};
Texture2D colorTexture : register (t0);
SamplerState colorSampler : register (s0);

float4 ps(VSOutput pixel) : SV_Target0
{
	float4 color = colorTexture.Sample(colorSampler, pixel.uv);
	return color;
}