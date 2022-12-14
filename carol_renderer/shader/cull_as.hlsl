#include "include/common.hlsli"
#include "include/mesh.hlsli"
#include "include/cull.hlsli"

groupshared Payload sharedPayload;

#ifdef SHADOW
cbuffer ShadowConstants : register(b3)
{
    uint gLightIdx;
}
#endif

[numthreads(AS_GROUP_SIZE, 1, 1)]
void main(
    uint gid : SV_GroupID,
    uint gtid : SV_GroupThreadID,
    uint dtid : SV_DispatchThreadID)
{ 
    bool visible = false;
    
    if (dtid < gMeshletCount)
    {
#ifdef SHADOW
        float4x4 worldViewProj = mul(gWorld, gLights[gLightIdx].ViewProj);
#else
        float4x4 worldViewProj = mul(gWorld, gViewProj);
#endif
        StructuredBuffer<CullData> cullData = ResourceDescriptorHeap[gCullDataIdx];
        CullData cd = cullData[dtid];
            
        visible = (AabbFrustumTest(cd.Center, cd.Extents, worldViewProj) != OUTSIDE) && NormalConeTest(cd.Center, cd.NormalCone, cd.ApexOffset, gWorld, gEyePosW);       
    }
    
    if (visible)
    {
        uint idx = WavePrefixCountBits(visible);
        sharedPayload.MeshletIndices[idx] = dtid;
    }
    
    uint visibleCount = WaveActiveCountBits(visible);
    DispatchMesh(visibleCount, 1, 1, sharedPayload);
}