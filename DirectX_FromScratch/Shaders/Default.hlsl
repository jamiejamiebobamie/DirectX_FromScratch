//***************************************************************************************
// Default.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Default shader, currently supports lighting.
//***************************************************************************************

// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

Texture2D gDiffuseMap1 : register(t0);
Texture2D gDiffuseMap2 : register(t1);
Texture2D gDiffuseMap3 : register(t2);
Texture2D gFireball1 : register(t3);
Texture2D gFireball2 : register(t4);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerState gsamBorderColor : register(s6);
SamplerState gsamAnisotropicMirror : register(s7);




cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gTexTransform;
    uint gTexIndex;
    uint gObjPad0;
    uint gObjPad1;
    uint gObjPad2;
};

// Constant data that varies per frame.
cbuffer cbPass : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;

    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light gLights[MaxLights];
};

// Constant data that varies per material.
cbuffer cbMaterial : register(b2)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
    float4x4 gMatTransform;
};
 
struct VertexIn
{
	float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;
	
    // Transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;

    // Assumes uniform scaling; otherwise, need to use inverse-transpose of world matrix.
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);

    // Transform to homogeneous clip space.
    vout.PosH = mul(posW, gViewProj);
    
    // Output vertex attributes for interpolation across triangle.
    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
    vout.TexC = mul(texC, gMatTransform).xy;

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float4 texColor = gTexIndex == 0 ? gDiffuseMap1.Sample(gsamAnisotropicWrap, pin.TexC) : gTexIndex == 1 ? gDiffuseMap2.Sample(gsamAnisotropicWrap, pin.TexC) : gDiffuseMap3.Sample(gsamAnisotropicWrap, pin.TexC); //texArr[gTexIndex].Sample(gsamAnisotropicWrap, pin.TexC) * gDiffuseAlbedo;

    float4 diffuseAlbedo;
    if (gTexIndex > 2)
    {
        float4 texColor = gFireball1.Sample(gsamAnisotropicWrap, float2(pin.TexC.x + cos(gTotalTime * -16.0f) / 16.0f, pin.TexC.y + sin(gTotalTime * -16.0f) / 16.0f)); // * float4(1.0f, 0.65f, 0.28f, 1.0f);

        texColor *= float4(0.5f, 0.0f, 0.0f, 1.0f);
        texColor = normalize(texColor);
        float4 texColor2 = gFireball2.Sample(gsamAnisotropicWrap, float2(0.5f + pin.TexC.x * 2.0f + cos(gTotalTime * 2.0f) / 16.0f, 0.5f + pin.TexC.y * 2.0f + sin(gTotalTime * 2.0f) / 16.0f)); // * float4(1.0f, 0.65f, 0.28f, 1.0f);
        texColor2 = normalize(texColor2);
        float4 combined = { texColor.x * texColor2.x, texColor.y * texColor2.y, texColor.z * texColor2.z, texColor.a * texColor2.a };
        diffuseAlbedo = normalize(combined);
        clip(diffuseAlbedo.x + diffuseAlbedo.y + diffuseAlbedo.z < .2f ? -1 : 1);
        diffuseAlbedo += float4(0.4f, 0.9f, 0.3f, 1.0f);
        diffuseAlbedo *= 2.0f;
    }
    else
    {
        diffuseAlbedo = texColor * gDiffuseAlbedo;
    }
 
    // Interpolating normal can unnormalize it, so renormalize it.
    pin.NormalW = normalize(pin.NormalW);

    // Vector from point being lit to eye. 
    float3 toEyeW = normalize(gEyePosW - pin.PosW);

	// Indirect lighting.
    float4 ambient = gTexIndex > 2 ? 0.5f * diffuseAlbedo : gAmbientLight * diffuseAlbedo;

    const float shininess = 1.0f - gRoughness;
    Material mat = { diffuseAlbedo, gFresnelR0, shininess };
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW, 
        pin.NormalW, toEyeW, shadowFactor);

    float4 litColor = gTexIndex > 2 ? ambient : ambient + directLight;

    // Common convention to take alpha from diffuse material.
    litColor.a = diffuseAlbedo.a;
    
    return litColor;
}


