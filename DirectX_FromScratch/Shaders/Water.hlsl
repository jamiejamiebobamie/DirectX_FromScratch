//***************************************************************************************
// Default.hlsl by Frank Luna (C) 2015 All Rights Reserved.
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

// An array of textures, which is only supported in shader model 5.1+.  Unlike Texture2DArray, the textures
// in this array can be different sizes and formats, making it more flexible than texture arrays.
Texture2D gTextureMaps[10] : register(t1);

// Put in space1, so the texture array does not overlap with these resources.  
// The texture array will occupy registers t0, t1, ..., t3 in space0. 
StructuredBuffer<MaterialData> gMaterialData : register(t0, space1);


SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

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

// Constant data that varies per material.
cbuffer cbPass : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    uint gIsNoNormalMap;
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

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
    float3 TangentU : TANGENT;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
    float2 TexCa : TEXCOORDA;
    float2 TexCb : TEXCOORDB;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f;

	// Fetch the material data.
    MaterialData matData1 = gMaterialData[gMaterialIndex];
    
    // Fetch the material data.
    MaterialData matData2 = gMaterialData[gMaterialIndex + 1];
    
    uint heightMapIndex1 = matData1.NormalMapIndex;
    uint heightMapIndex2 = matData2.NormalMapIndex;
    
    // Output vertex attributes for interpolation across triangle.
    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
    vout.TexCa = mul(texC, matData1.MatTransform).xy;
    vout.TexCb = mul(texC, matData2.MatTransform).xy;
   
    //float4 normalMapSample1 = gTextureMaps[heightMapIndex1][vout.TexCa];
    //float4(0.0f, 0.0f, 0.0f, 0.0f);
    //gTextureMaps[heightMapIndex1][vout.TexCa];
    //float4 normalMapSample2 = gTextureMaps[heightMapIndex2][vout.TexCb];
    //float4(0.0f, 0.0f, 0.0f, 0.0f);
    //gTextureMaps[heightMapIndex2][vout.TexCb];

    //float height1 = normalMapSample1.w;
    //float height2 = normalMapSample2.w;
    
    // Transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;
    
    //vout.PosW.y = vout.PosW.y + height1;
    //+height2;
    
    // Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
    vout.NormalW = mul(vin.NormalL, (float3x3) gWorld);
	
    vout.TangentW = mul(vin.TangentU, (float3x3) gWorld);

    // Transform to homogeneous clip space.
    vout.PosH = mul(posW, gViewProj);
	
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	// Fetch the material data.
    MaterialData matData1 = gMaterialData[gMaterialIndex];
    float4 diffuseAlbedo = matData1.DiffuseAlbedo;
    float3 fresnelR0 = matData1.FresnelR0;
    float roughness = matData1.Roughness;
    uint diffuseMapIndex1 = matData1.DiffuseMapIndex;
    uint normalMapIndex1 = matData1.NormalMapIndex;
    
    MaterialData matData2 = gMaterialData[gMaterialIndex + 1];
    uint diffuseMapIndex2 = matData2.DiffuseMapIndex;
    uint normalMapIndex2 = matData2.NormalMapIndex;
	
	// Interpolating normal can unnormalize it, so renormalize it.
    pin.NormalW = normalize(pin.NormalW);
	
    float4 normalMapSample1 = gTextureMaps[normalMapIndex1].Sample(gsamAnisotropicWrap, pin.TexCa);
    float4 normalMapSample2 = gTextureMaps[normalMapIndex2].Sample(gsamAnisotropicWrap, pin.TexCb);

    
    //float3 normalSample = normalMapSample1.rgb + normalMapSample2.rgb;
    float3 bumpedNormalW = pin.NormalW; //gIsNoNormalMap == 1 ? pin.NormalW : NormalSampleToWorldSpace(normalMapSample2.rgb, pin.NormalW, pin.TangentW); //normalize((NormalSampleToWorldSpace(normalMapSample1.rgb, pin.NormalW, pin.TangentW) + NormalSampleToWorldSpace(normalMapSample2.rgb, pin.NormalW, pin.TangentW)) / 2.0f);

	// Dynamically look up the texture in the array.
    //diffuseAlbedo *= saturate(gTextureMaps[diffuseMapIndex1].Sample(gsamAnisotropicWrap, pin.TexCa) + gTextureMaps[diffuseMapIndex2].Sample(gsamAnisotropicWrap, pin.TexCb) / 2.0f);
    
    diffuseAlbedo *= gIsNoNormalMap == 1 ? gTextureMaps[diffuseMapIndex1].Sample(gsamAnisotropicWrap, pin.TexCa) : gTextureMaps[diffuseMapIndex2].Sample(gsamAnisotropicWrap, pin.TexCb); //saturate(gTextureMaps[diffuseMapIndex1].Sample(gsamAnisotropicWrap, pin.TexCa) + gTextureMaps[diffuseMapIndex2].Sample(gsamAnisotropicWrap, pin.TexCb));

    
    //diffuseAlbedo *= gTextureMaps[normalMapIndex2].Sample(gsamAnisotropicWrap, pin.TexCb);
    //diffuseAlbedo *= gIsNoNormalMap == 1 ? gTextureMaps[normalMapIndex1].Sample(gsamAnisotropicWrap, pin.TexCa) : gTextureMaps[normalMapIndex2].Sample(gsamAnisotropicWrap, pin.TexCb); //saturate(gTextureMaps[diffuseMapIndex1].Sample(gsamAnisotropicWrap, pin.TexCa) + gTextureMaps[diffuseMapIndex2].Sample(gsamAnisotropicWrap, pin.TexCb));

    
    //bumpedNormalW += normalMapSample1.w + normalMapSample2.w;
    
    //bumpedNormalW = normalize(bumpedNormalW);
    
    
    // Vector from point being lit to eye. 
    float3 toEyeW = normalize(gEyePosW - pin.PosW);

    // Light terms.
    float4 ambient = gAmbientLight * diffuseAlbedo;

    const float shininess = (1.0f - roughness);// * normalMapSample.a;
    Material mat = { diffuseAlbedo, fresnelR0, shininess };
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
        bumpedNormalW, toEyeW, shadowFactor);

    float4 litColor = ambient + directLight;

	// Add in specular reflections.
    float3 r = reflect(-toEyeW, bumpedNormalW);
    float4 reflectionColor = gCubeMap.Sample(gsamLinearWrap, r);
    float3 fresnelFactor = SchlickFresnel(fresnelR0, bumpedNormalW, r);
    litColor.rgb += shininess * fresnelFactor * reflectionColor.rgb;
	
    // Common convention to take alpha from diffuse albedo.
    litColor.a = diffuseAlbedo.a;

    return litColor;
}


