
// Normals.hlsl -- Draw Vertex Normals


cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
};

cbuffer cbPass : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
};

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
};

struct VertexOut
{
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
};

struct GeoOut
{
    float4 PosH : SV_POSITION;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    
    vout.PosW = mul(float4(vin.PosL, 1.0f), gWorld).xyz;
    vout.NormalW = mul(vin.NormalL, (float3x3) gWorld);

    return vout;
}

[maxvertexcount(6)]
void GS(triangle VertexOut gin[3], inout LineStream<GeoOut> lineStream)
{
    float3 faceNormal = { 0.0f, 0.0f, 0.0f };
    float3 accPos = { 0.0f, 0.0f, 0.0f };
    for (int i = 0; i < 3; i++)
    {
        faceNormal += gin[i].NormalW;
        accPos += gin[i].PosW;
    }
    
    faceNormal /= 3;
    accPos /= 3;
    
    float lineLength = 1.0f;
    
    GeoOut p1;
    p1.PosH = mul(float4(accPos, 1.0f), gViewProj);
    
    GeoOut p2;
    p2.PosH = mul(float4(accPos + faceNormal * lineLength, 1.0f), gViewProj);

    lineStream.Append(p1);
    lineStream.Append(p2);
    lineStream.RestartStrip();
}

float4 PS(GeoOut pin) : SV_Target
{
    return float4(1.0f, 0.0f, 0.0f, 1.0f);
}


