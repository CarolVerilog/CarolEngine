#include "include/root_signature.hlsli"
#include "include/mesh.hlsli"
#include "include/shadow.hlsli"

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

[numthreads(128, 1, 1)]
[OutputTopology("triangle")]
void MS(
    uint gtid : SV_GroupThreadID,
    uint gid : SV_GroupID,
    out indices uint3 tris[126],
    out vertices VertexOut verts[64])
{
    StructuredBuffer<Meshlet> meshlets = ResourceDescriptorHeap[gMeshletIdx];
    Meshlet m = meshlets[gid];
    
    SetMeshOutputCounts(m.VertexCount, m.PrimCount);
    
    if (gtid < m.PrimCount)
    {
        tris[gtid] = UnpackPrim(m.Prims[gtid]);
    }
    
    if (gtid < m.VertexCount)
    {
        StructuredBuffer<VertexIn> vertices = ResourceDescriptorHeap[gVertexIdx];
        VertexIn vin = vertices[m.Vertices[gtid]];
            
        #ifdef SKINNED
            vin = SkinnedTransform(vin);
        #endif

            VertexOut vout;

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
        
        verts[gtid] = vout;
    }
}

float4 PS(VertexOut pin) : SV_Target
{
    Texture2D gDiffuseMap = ResourceDescriptorHeap[gDiffuseMapIdx];
    Texture2D gNormalMap = ResourceDescriptorHeap[gNormalMapIdx];

#ifdef SSAO
    Texture2D gSsaoMap = ResourceDescriptorHeap[gAmbientIdx];
#endif
    
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
    ambient *= ambientAccess;
#endif

    float3 shadowFactor = float3(1.0f, 1.0f, 1.0f);
    shadowFactor[0] = CalcShadowFactor(mul(float4(pin.PosW, 1.0f), gLights[0].ViewProjTex));
    float4 litColor = ComputeLighting(gLights, lightMat, pin.PosW, texNormal, normalize(gEyePosW - pin.PosW), shadowFactor);
    
    return float4(ambient + litColor.rgb, 1.0f);
}