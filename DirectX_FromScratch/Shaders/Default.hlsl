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

// Constant data that varies per frame.
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
	float4x4 gTexTransform;
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
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;

	float4 gFogColor;
	float gFogStart;
	float gFogRange;
	//float2 cbPerObjectPad2;
    
    float gRadius;
    float gIncr;

    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light gLights[MaxLights];
};

cbuffer cbMaterial : register(b2)
{
	float4   gDiffuseAlbedo;
    float3   gFresnelR0;
    float    gRoughness;
	float4x4 gMatTransform;
};

struct VertexIn
{
	float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
};

struct GeoOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float4 Color : COLOR;
};


void Subdivide(inout GeoOut inVerts[3], inout GeoOut outVerts[6])
{

	//       v1
	//       *
	//      / \
	//     /   \
	//  m0*-----*m1
	//   / \   / \
	//  /   \ /   \
	// *-----*-----*
	// v0    m2     v2
    
    GeoOut m[3];
    
    m[0].PosW = 0.5f * (inVerts[0].PosW + inVerts[1].PosW);
    m[1].PosW = 0.5f * (inVerts[1].PosW + inVerts[2].PosW);
    m[2].PosW = 0.5f * (inVerts[2].PosW + inVerts[0].PosW);
    
    m[0].PosW = normalize(m[0].PosW);
    m[1].PosW = normalize(m[1].PosW);
    m[2].PosW = normalize(m[2].PosW);
    
    m[0].NormalW = m[0].PosW;
    m[1].NormalW = m[1].PosW;
    m[2].NormalW = m[2].PosW;
    
    outVerts[0] = inVerts[0];
    outVerts[1] = m[0];
    outVerts[2] = m[2];
    outVerts[3] = m[1];
    outVerts[4] = inVerts[2];
    outVerts[5] = inVerts[1];
    
    
    inVerts[1] = m[0];
    inVerts[2] = m[2];

}

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;
	
    // Transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;

    // Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);

    // Transform to homogeneous clip space.
    vout.PosH = mul(posW, gViewProj);
    
    return vout;
}

[maxvertexcount(36)]
void GS(triangle VertexOut gin[3],
        uint primID : SV_PrimitiveID,
        inout TriangleStream<GeoOut> triStream)
{
    float3 meanVec = float3(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 3; i++)
    {
        meanVec += gin[i].PosW;
    }
    meanVec /= 3.0f;
       
    float d = distance(gEyePosW, meanVec);
    
    int iters = d < 7 ? 2 : d >= 14 ? 0 : 1;
    
    GeoOut vIn[3];
    vIn[0].PosW = gin[0].PosW;
    vIn[0].NormalW = gin[0].NormalW;
    vIn[1].PosW = gin[1].PosW;
    vIn[1].NormalW = gin[1].NormalW;
    vIn[2].PosW = gin[2].PosW;
    vIn[2].NormalW = gin[2].NormalW;

    [unroll]
    for (int j = 0; j < iters; j++)
    {
        GeoOut vOut[6];
        Subdivide(vIn, vOut);
    	[unroll]
        for (int i = 0; i < 6; i++)
        {
            vOut[i].PosH = mul(float4(vOut[i].PosW, 1.0f), gViewProj);
            vOut[i].Color = float4(vOut[i].PosW, 1.0f);
            triStream.Append(vOut[i]);
            if (i % 3 == 0) triStream.RestartStrip();
        }
    }
    
    if (iters == 0)
    {
        for (int i = 0; i < 3; i++)
        {
            vIn[i].PosH = mul(float4(vIn[i].PosW, 1.0f), gViewProj);
            vIn[i].Color = float4(vIn[i].PosW, 1.0f);
            triStream.Append(vIn[i]);
   //         if (i % 3 == 0)
     //           triStream.RestartStrip();
        }
    }
}



float4 PS(VertexOut pin) : SV_Target
{
    float4 diffuseAlbedo = gDiffuseAlbedo;;
    //pin.Color;
    //gDiffuseAlbedo;

    // Interpolating normal can unnormalize it, so renormalize it.
    pin.NormalW = normalize(pin.NormalW);

    // Vector from point being lit to eye. 
	float3 toEyeW = gEyePosW - pin.PosW;
	float distToEye = length(toEyeW);
	toEyeW /= distToEye; // normalize

    // Light terms.
    float4 ambient = gAmbientLight*diffuseAlbedo;

    const float shininess = 1.0f - gRoughness;
    Material mat = { diffuseAlbedo, gFresnelR0, shininess };
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
        pin.NormalW, toEyeW, shadowFactor);

    float4 litColor = ambient + directLight;

    // Common convention to take alpha from diffuse albedo.
    litColor.a = diffuseAlbedo.a;

    return litColor;
}


