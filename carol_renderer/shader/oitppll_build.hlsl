#include "include/oitppll.hlsli"
#include "include/root_signature.hlsli"
#include "include/mesh.hlsli"
#include "include/shadow.hlsli"

#ifdef SKINNED
#include "include/skinned.hlsli"
#endif





struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION0;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
    float2 TexC : TEXCOORD;
#ifdef SSAO
    float4 SsaoPosH : POSITION1;
#endif
};

VertexOut VS(VertexIn vin)
{
   VertexOut vout;

#ifdef SKINNED
    vin = SkinnedTransform(vin);
#endif

    vout.PosW = mul(float4(vin.PosL, 1.0f), gWorld).xyz;
    vout.NormalW = normalize(mul(vin.NormalL, (float3x3) gWorld));
    vout.TangentW = normalize(mul(vin.TangentL, (float3x3) gWorld));
    vout.TexC = vin.TexC;

#ifdef TAA
    vout.PosH = mul(float4(vout.PosW, 1.0f), gJitteredViewProj);
#else
    vout.PosH = mul(float4(vout.PosW, 1.0f), gViewProj);
#endif

#ifdef SSAO
    vout.SsaoPosH = mul(float4(vout.PosW, 1.0f), gViewProjTex);
#endif

    return vout;
}

void PS(VertexOut pin)
{
    RWStructuredBuffer<OitNode> gOitNodeBuffer = ResourceDescriptorHeap[gResourceStartOffset + gOitW];
    RWByteAddressBuffer gStartOffsetBuffer = ResourceDescriptorHeap[gResourceStartOffset + gOitOffsetW];
    RWByteAddressBuffer gCounter = ResourceDescriptorHeap[gResourceStartOffset + gOitCounterW];
    
    Texture2D gDiffuseMap = ResourceDescriptorHeap[gResourceStartOffset + gDiffuseMapIdx];
    Texture2D gNormalMap = ResourceDescriptorHeap[gResourceStartOffset + gNormalMapIdx];
    Texture2D gDepthMap = ResourceDescriptorHeap[gResourceStartOffset + gDepthStencilIdx];
#ifdef SSAO
    Texture2D gSsaoMap = ResourceDescriptorHeap[gResourceStartOffset + gAmbientIdx];
#endif
    
    float2 ndcPos = pin.PosH.xy * gInvRenderTargetSize;
    if (pin.PosH.z > gDepthMap.Sample(gsamAnisotropicWrap, ndcPos).r)
    {
        return;
    }

    float4 texDiffuse = gDiffuseMap.SampleLevel(gsamAnisotropicWrap, pin.TexC, pow(pin.PosH.z, 15.0f) * 8.0f);

    LightMaterialData lightMat;
    lightMat.fresnelR0 = gFresnelR0;
    lightMat.diffuseAlbedo = texDiffuse.rgb;
    lightMat.roughness = gRoughness;

    float3 texNormal = gNormalMap.SampleLevel(gsamAnisotropicWrap, pin.TexC, pow(pin.PosH.z, 15.0f) * 8.0f).rgb;
    texNormal = TexNormalToWorldSpace(texNormal, pin.NormalW, pin.TangentW);

    float3 ambient = gLights[0].Ambient * texDiffuse.rgb;

#ifdef SSAO
    pin.SsaoPosH /= pin.SsaoPosH.w;
    float ambientAccess = gSsaoMap.SampleLevel(gsamLinearClamp, pin.SsaoPosH.xy, 0.0f).r;
    ambient*=ambientAccess;
#endif

    float3 shadowFactor = float3(1.0f, 1.0f, 1.0f);
    shadowFactor[0] = CalcShadowFactor(mul(float4(pin.PosW, 1.0f), gLights[0].ViewProjTex));
    float4 litColor = ComputeLighting(gLights, lightMat, pin.PosW, texNormal, normalize(gEyePosW - pin.PosW), shadowFactor);
    float3 color = ambient + litColor.rgb;

    OitNode link;
    link.ColorU = float4(color.rgb, 0.6f);
    link.DepthU = pin.PosH.z * 0xffffffff;

    uint pixelCount;
    gCounter.InterlockedAdd(0, 1, pixelCount);

    uint2 pixelPos = uint2(pin.PosH.x - 0.5f, pin.PosH.y - 0.5f);

    uint pixelCountAddr = 4 * (pixelPos.y * gRenderTargetSize.x + pixelPos.x);
    uint oldStartOffset;
    gStartOffsetBuffer.InterlockedExchange(pixelCountAddr, pixelCount, oldStartOffset);

    link.NextU = oldStartOffset;
    gOitNodeBuffer[pixelCount] = link;
}