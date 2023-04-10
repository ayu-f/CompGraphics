#include "CBScene.hlsli"

float3 CalculateColor(in float3 objColor, in float3 objNormal, in float3 pos, in float4 lightParams, in bool trans)
{
    float3 finalColor = objColor * ambientColor.xyz * lightParams.x;

    for (int i = 0; i < lightCount.x; i++)
    {
        float3 normal = objNormal;

        float3 lightDir = lights[i].pos.xyz - pos;
        float lightDist = length(lightDir);
        lightDir /= lightDist;

        float atten = clamp(1.0 / (lightDist * lightDist), 0, 1);

        if (trans && dot(lightDir, objNormal) < 0.0)
        {
            normal = -normal;
        }

        finalColor += objColor * max(dot(lightDir, normal), 0) * atten * lights[i].color.xyz * lightParams.y;

        float3 viewDir = normalize(cameraPosition.xyz - pos);
        float3 reflectDir = reflect(-lightDir, normal);

        float spec = lightParams.w > 0.0 ? pow(max(dot(viewDir, reflectDir), 0.0), lightParams.w) : 0.0;

        finalColor += objColor * spec * atten * lights[i].color.xyz * lightParams.z;
    }

    return finalColor;
}