RaytracingAccelerationStructure Scene : register(u0, space0);
RWTexture2D<float4> gOutput : register(u0);

float3 linearToSrgb(float3 c)
{
    // Based on http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
    float3 sq1 = sqrt(c);
    float3 sq2 = sqrt(sq1);
    float3 sq3 = sqrt(sq2);
    float3 srgb = 0.662002687 * sq1 + 0.684122060 * sq2 - 0.323583601 * sq3 - 0.0225411470 * c;
    return srgb;
}

[shader("raygeneration")]
void rayGen()
{  
    uint2 launchIndex = DispatchRaysIndex();
    float3 col = linearToSrgb(float3(launchIndex.x * (1.0/1280.0), launchIndex.y * (1.0f/720.0), 0.0));
    gOutput[launchIndex] = float4(col, 1);
}

struct Payload
{
    bool hit;
};

[shader("miss")]
void miss(inout Payload payload)
{
    payload.hit = false;
}

struct IntersectionAttribs
{
    float2 baryCrd;
};

[shader("closesthit")]
void chs(inout Payload payload, in IntersectionAttribs attribs)
{
    payload.hit = true;
}