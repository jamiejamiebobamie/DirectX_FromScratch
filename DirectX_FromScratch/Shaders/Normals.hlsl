
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

[maxvertexcount(2)]
//void GS(triangle VertexOut gin[3], inout LineStream<GeoOut> lineStream)

void GS(point VertexOut gin[1], inout LineStream<GeoOut> lineStream)
{
    float lineLength = 1.0f;
    GeoOut p1;
    p1.PosH = mul(float4(gin[0].PosW, 1.0f), gViewProj);
    
    GeoOut p2;
    p2.PosH = mul(float4(gin[0].PosW + gin[0].NormalW * lineLength, 1.0f), gViewProj);
    
    //GeoOut p3;
    //p3.PosH = mul(float4(gin[1].PosW, 1.0f), gViewProj);
    
    //GeoOut p4;
    //p4.PosH = mul(float4(gin[1].PosW + gin[1].NormalW * lineLength, 1.0f), gViewProj);
    
    //GeoOut p5;
    //p5.PosH = mul(float4(gin[2].PosW, 1.0f), gViewProj);
    
    //GeoOut p6;
    //p6.PosH = mul(float4(gin[2].PosW + gin[2].NormalW * lineLength, 1.0f), gViewProj);
   

    lineStream.Append(p1);
    lineStream.Append(p2);
    lineStream.RestartStrip();
    //lineStream.Append(p3);
    //lineStream.Append(p4);
    //lineStream.RestartStrip();
    //lineStream.Append(p5);
    //lineStream.Append(p6);
    //lineStream.RestartStrip();
}

float4 PS(GeoOut pin) : SV_Target
{
    return float4(1.0f, 0.0f, 0.0f, 1.0f);
}


