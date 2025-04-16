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
    
    float4 gFogColor;
    
    float4 gAmbientLight;
    
    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light gLights[MaxLights];
};

cbuffer cbMaterial : register(b2)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
    float4x4 gMatTransform;
};

Texture2D tex : register(t0);

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
    int TexI : TEXINDEX;
};

struct GeoOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
    int TexI : TEXINDEX;
};

GeoOut VS(VertexIn vin)
{
    GeoOut vout = (GeoOut) 0.0f;
	
    // Transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;

    // Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
    vout.NormalW = mul(vin.NormalL, (float3x3) gWorld);

    // Transform to homogeneous clip space.
    vout.PosH = mul(posW, gViewProj);
    
        // Transform to homogeneous clip space.
    vout.TexC = vin.TexC;
    
    vout.TexI = vin.TexI;
    
    return vout;
}

 // We expand each point into a quad (4 vertices), so the maximum number of vertices
 // we output per geometry shader invocation is 4.
[maxvertexcount(4)]
void GS(point GeoOut gin[1],
        uint primID : SV_PrimitiveID,
        inout TriangleStream<GeoOut> triStream)
{
	//
	// Compute the local coordinate system of the sprite relative to the world
	// space such that the billboard is aligned with the y-axis and faces the eye.
	//

    float3 up = float3(0.0f, 1.0f, 0.0f);
    float3 look = gEyePosW - gin[0].PosW;
    look = normalize(look);
    float3 right = cross(up, look);

	//
	// Compute triangle strip vertices (quad) in world space.
	//
   
    float d = abs(dot(look, up));
    
    float halfWidth = 0.5f * 20;
    float halfHeight = 0.5f * 20;
	
    float4 v[4];
    v[0] = float4(gin[0].PosW + halfWidth * right - halfHeight * up, 1.0f);
    v[1] = float4(gin[0].PosW + halfWidth * right + halfHeight * up, 1.0f);
    v[2] = float4(gin[0].PosW - halfWidth * right - halfHeight * up, 1.0f);
    v[3] = float4(gin[0].PosW - halfWidth * right + halfHeight * up, 1.0f);
	
    float2 texC[4] =
    {
        float2(0.0f, 1.0f),
		float2(0.0f, 0.0f),
		float2(1.0f, 1.0f),
		float2(1.0f, 0.0f)
    };
	
    GeoOut gout;
	[unroll]
    for (int i = 0; i < 4; ++i)
    {
        v[i].xy += 0.5f * sin(v[i].x) * sin(9.0f * gTotalTime);
        //v[i].z *= 0.6f + 0.4f * sin(2.0f * gTotalTime);
        gout.PosH = mul(v[i], gViewProj);
        gout.PosW = v[i].xyz;
        gout.NormalW = look;
        gout.TexC = texC[i];
        gout.TexI = 1;
        triStream.Append(gout);
    }
}

float4 PS(GeoOut pin) : SV_Target
{

    if (pin.TexI == 1)
    {
        float x = pin.TexC.x - 0.5f;
        float y = pin.TexC.y - 0.5f;
        
        float theta = gTotalTime * 1.5f;
        
        pin.TexC.x = x * cos(theta) - y * sin(theta);
        pin.TexC.y = y * cos(theta) + x * sin(theta);
    }

       
    float4 diffuseAlbedo = tex.Sample(gsamAnisotropicWrap, pin.TexC * 0.75f + 0.5f);

    // Interpolating normal can unnormalize it, so renormalize it.
    pin.NormalW = normalize(pin.NormalW);

    // Vector from point being lit to eye. 
    float3 toEyeW = gEyePosW - pin.PosW;
    float distToEye = length(toEyeW);
    toEyeW /= distToEye; // normalize

    // Light terms.
    float4 ambient = gAmbientLight * diffuseAlbedo;

    const float shininess = 1.0f - gRoughness;
    Material mat = { diffuseAlbedo, gFresnelR0, shininess };
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
        pin.NormalW, toEyeW, shadowFactor);

    float4 litColor = ambient + directLight;
    
    
    if (pin.TexI == 1)
    {
        float3 to_center = float3(0.0f, 3.0f, 2.0f) - pin.PosW;
        float distFromCenter = length(to_center);
        float maxDist = 5.0f;
        float minDist = 5.0f;
        
        //float alpha = 1 - (distFromCenter / (maxDist - minDist));
        
        float alphaAmt = saturate((distFromCenter - minDist) / maxDist);
        
        diffuseAlbedo.a = lerp(diffuseAlbedo.a, 0.0f, alphaAmt);
        
        float3 rgb = float3(1.0f, 0.843f, 0.0f);// * 5.0f;
        litColor.rgb = rgb;
        //diffuseAlbedo.a = diffuseAlbedo.a * clamp(alpha, 0.0f, 1.0f);
    }
    
    // Common convention to take alpha from diffuse albedo.
    litColor.a = diffuseAlbedo.a;

    //return pin.isOrtho == true ? float4(1.0, 0.0, 0.0, 1.0) : litColor;
    return litColor;
}


