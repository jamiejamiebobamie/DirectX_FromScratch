

ConsumeStructuredBuffer<float3> gInputA : register(u0);
AppendStructuredBuffer<float> gOutput : register(u1);


[numthreads(64, 1, 1)]
void CS()
{
    //float3 test = float3(1.0f, 0.0f, 0.0f);
    //gOutput.Append(length(test));
    //gInputA.Consume();
    gOutput.Append(length(gInputA.Consume()));

}
