#include "include/root_signature.hlsli"

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

static const float2 gTexCoords[6] =
{
    float2(0.0f, 1.0f),
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(1.0f, 1.0f)
};

static int sampleCount = 9;
static int dx[9] = { -1, -1, -1, 0, 0, 0, 1, 1, 1 };
static int dy[9] = { -1, 0, 1, -1, 0, 1, -1, 0, 1 };

[numthreads(6, 1, 1)]
[OutputTopology("triangle")]
void MS(
    uint gtid : SV_GroupThreadID,
    uint gid : SV_GroupID,
    out indices uint3 tris[2],
    out vertices VertexOut verts[6])
{   
    
    SetMeshOutputCounts(6, 2);
    
    if (gtid == 0)
    {
        tris[gtid] = uint3(0, 1, 2);
    }
    else if (gtid == 1)
    {
        tris[gtid] = uint3(3, 4, 5);
    }
    
    if (gtid < 6)
    {
        VertexOut vout;

        vout.TexC = gTexCoords[gtid];
        vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f);
        verts[gtid] = vout;
    }
}

float3 Rgb2Ycocg(float3 rgbColor)
{
    float3 ycocg;
    
    ycocg.x = 0.25f * rgbColor.x + 0.5f * rgbColor.y + 0.25 * rgbColor.z;
    ycocg.y = 0.5f * rgbColor.x + (-0.5f) * rgbColor.z;
    ycocg.z = -0.25f * rgbColor.x + 0.5f * rgbColor.y + (-0.25) * rgbColor.z;
    
    return ycocg;
}

float3 Ycocg2Rgb(float3 ycocgColor)
{
    float3 rgb;
    
    float temp = ycocgColor.x - ycocgColor.z;
    rgb.r = temp + ycocgColor.y;
    rgb.g = ycocgColor.x + ycocgColor.z;
    rgb.b = temp - ycocgColor.y;
    
    return rgb;
}

float3 Clip(float3 histColor, float3 minPixelColor, float3 maxPixelColor)
{
    float3 centerColor = (minPixelColor + maxPixelColor) / 2;
    float3 ray = centerColor - histColor;
    float3 invRay = rcp(ray);
    
    float3 minRay = (minPixelColor - histColor) * invRay;
    float3 maxRay = (maxPixelColor - histColor) * invRay;
    float3 factorVec = min(minRay, maxRay);
    
    float blendFactor = saturate(max(max(factorVec.x, factorVec.y), factorVec.z));
    return lerp(histColor, centerColor, blendFactor);
}


void CalcPixelColorAabb(float2 texC, inout float3 minPixelColor, inout float3 maxPixelColor)
{
    Texture2D gCurrMap = ResourceDescriptorHeap[gFrameIdx];
    
    minPixelColor = float3(1.0f, 0.5f, 0.5f);
    maxPixelColor = float3(0.0f, -0.5f, -0.5f);

    float3 meanColor = float3(0.0f, 0.0f, 0.0f);
    float3 varColor = float3(0.0f, 0.0f, 0.0f);
    static float3 gamma = 1.0f;
    
    [unroll]
    for (int i = 0; i < sampleCount; i++)
    {
        float3 pixelColor = Rgb2Ycocg(gCurrMap.Sample(gsamPointClamp, texC).rgb);
        meanColor += pixelColor;
        varColor += pixelColor * pixelColor;
    }

    meanColor /= sampleCount;
    varColor = sqrt(abs(varColor / sampleCount - meanColor * meanColor));
    
    minPixelColor = meanColor - gamma * varColor;
    maxPixelColor = meanColor + gamma * varColor;
}

float4 PS(VertexOut pin) : SV_Target
{
    Texture2D gDepthMap = ResourceDescriptorHeap[gDepthStencilIdx];
    Texture2D gCurrMap = ResourceDescriptorHeap[gFrameIdx];
    Texture2D gHistMap = ResourceDescriptorHeap[gHistIdx];
    Texture2D gVelocityMap = ResourceDescriptorHeap[gVelocityIdx];
        
    float minZ = 1.0f;
    float2 minZPos = pin.TexC;
    
    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        float2 adjacentPos = pin.TexC + float2(dx[i], dy[i]) * gInvRenderTargetSize;
        float adjacentZ = gDepthMap.Sample(gsamPointClamp, adjacentPos).r;
        
        if (adjacentZ < minZ)
        {
            minZ = adjacentZ;
            minZPos = adjacentPos;
        }
    }
    
    float2 currPos = pin.TexC;
    float2 histPos = currPos + gVelocityMap.Sample(gsamPointClamp, minZPos).xy;
        
    float4 currPixelColor = gCurrMap.Sample(gsamPointClamp, currPos);
    float4 histPixelColor = gHistMap.Sample(gsamPointClamp, histPos);
    
    float3 minPixelColor;
    float3 maxPixelColor;
    CalcPixelColorAabb(currPos, minPixelColor, maxPixelColor);
    
    histPixelColor.rgb = Ycocg2Rgb(Clip(Rgb2Ycocg(histPixelColor.rgb), minPixelColor, maxPixelColor));
    
    float4 taaPixel = 0.05f * currPixelColor + 0.95f * histPixelColor;
    return taaPixel;
}
