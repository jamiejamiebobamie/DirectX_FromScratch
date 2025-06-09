//***************************************************************************************
// Common.hlsl by Frank Luna (C) 2015 All Rights Reserved.
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

struct MaterialData
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Roughness;
    float4x4 MatTransform;
    uint DiffuseMapIndex;
    uint NormalMapIndex;
    uint MatPad1;
    uint MatPad2;
};

TextureCube gCubeMap : register(t0);
TextureCube gShadowMap : register(t1);

// An array of textures, which is only supported in shader model 5.1+.  Unlike Texture2DArray, the textures
// in this array can be different sizes and formats, making it more flexible than texture arrays.
Texture2D gTextureMaps[10] : register(t2);

// Put in space1, so the texture array does not overlap with these resources.  
// The texture array will occupy registers t0, t1, ..., t3 in space0. 
StructuredBuffer<MaterialData> gMaterialData : register(t0, space1);


SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

// Constant data that varies per frame.
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gTexTransform;
    uint gMaterialIndex;
    uint gObjPad0;
    uint gObjPad1;
    uint gObjPad2;
};

cbuffer cbPass : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float4x4 gShadowTransform;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;
    float3 gPointLightPosW;
    float cbPerObjectPad2;

    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light gLights[MaxLights];
};

//---------------------------------------------------------------------------------------
// Transforms a normal map sample to world space.
//---------------------------------------------------------------------------------------
float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
	// Uncompress each component from [0,1] to [-1,1].
    float3 normalT = 2.0f * normalMapSample - 1.0f;

	// Build orthonormal basis.
    float3 N = unitNormalW;
    float3 T = normalize(tangentW - dot(tangentW, N) * N);
    float3 B = cross(N, T);

    float3x3 TBN = float3x3(T, B, N);

	// Transform from tangent space to world space.
    float3 bumpedNormalW = mul(normalT, TBN);

    return bumpedNormalW;
}

//---------------------------------------------------------------------------------------
// PCF for shadow mapping.
//---------------------------------------------------------------------------------------

float CalcShadowFactor(float4 posW)//shadowPosH)
{
    // Complete projection by doing division by w.
    //shadowPosH.xyz /= shadowPosH.w;

    // Depth in NDC space.
    //float depth = shadowPosH.z;
    
        
    float3 diff = posW.xyz - gPointLightPosW;
    float3 look = normalize(diff);
    float up = float3(0.0f, 1.0f, 0.0f);
    float3 right = cross(look, up);

    uint width, height, numMips;
    gShadowMap.GetDimensions(0, width, height, numMips);

    // Texel size.
    float dx = 1.0f / (float) width;
    float percentLit = 0.0f;
    
//    const float3 offsets[9] =
//    {
//        float3(-dx, -dx), float3(0.0f, -dx), float3(dx, -dx),
//        float3(-dx, 0.0f), float3(0.0f, 0.0f), float3(dx, 0.0f),
//        float3(-dx, +dx), float3(0.0f, +dx), float3(dx, +dx)
//    };
    const float3 offsets[9] =
    {
        look - right * dx + up * dx, look + up * dx, look + right * dx + up * dx,

    look - right * dx, look, look + right * dx,
    
    look - right * dx - up * dx, look - up * dx, look + right * dx - up * dx
    };
    
//    const float2 offsets[9] =
//    {
//        float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx),
//        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
//        float2(-dx, +dx), float2(0.0f, +dx), float2(dx, +dx)
//    };
    
    float dist = distance(posW.xyz, gPointLightPosW);
    float depth = dist / (1000.0f - 0.1f);
    
    float imgDepth = gShadowMap.Sample(gsamPointWrap, look).r;
    return depth > imgDepth ? 0.0f : 1.0f;

//    [unroll]
//    for (int i = 0; i < 9; ++i)
//    {

        // percentLit += gShadowMap.SampleCmpLevelZero(gsamShadow, shadowPosH.xy + offsets[i], depth).r;
//        float imgDepth = gShadowMap.Sample(gsamPointWrap, offsets[i]).r;
//        percentLit += depth > imgDepth ? 0.0f : 1.0f;
//    }
//    return percentLit / 9.0f;
    
        /*
    float percentLit = 0.0f;
    const float2 offsets[25] =
    {
        float2(-2 * dx, -2 * dx), float2(-dx, -2 * dx), float2(0.0f, -2 * dx), float2(dx, -2 * dx), float2(2 * dx, -2 * dx),
        float2(-2*dx, -dx), float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx), float2(2*dx, -dx), 
        float2(-2 * dx, 0.0f), float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f), float2(2 * dx, 0.0f),
        float2(-2 * dx, dx), float2(-dx, +dx), float2(0.0f, +dx), float2(dx, +dx), float2(2 * dx, dx),
        float2(-2 * dx, 2 * dx), float2(-dx, 2 * dx), float2(0.0f, 2 * dx), float2(dx, 2 * dx), float2(2 * dx, 2 * dx)
    };

    [unroll]
    for (int i = 0; i < 25; ++i)
    {
        percentLit += gShadowMap.SampleCmpLevelZero(gsamShadow,
            shadowPosH.xy + offsets[i], depth).r;
    }
    
    return percentLit / 25.0f;
 */

}

