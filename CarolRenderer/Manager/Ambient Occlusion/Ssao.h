#pragma once
#include "../Manager.h"
#include "../../Utils/d3dx12.h"
#include <DirectXMath.h>
#include <memory>
#include <vector>

using std::unique_ptr;
using std::vector;

namespace Carol
{
	class HeapAllocInfo;
	class CircularHeap;
	class DefaultResource;
	class Shader;

	class SsaoConstants {
	public:
		DirectX::XMFLOAT4X4 Proj;
		DirectX::XMFLOAT4X4 InvProj;
		DirectX::XMFLOAT4X4 ProjTex;
		DirectX::XMFLOAT4 OffsetVectors[14];

		DirectX::XMFLOAT4 BlurWeights[3];
		DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f,0.0f };

		float OcclusionRadius = 0.5f;
		float OcclusionFadeStart = 0.2f;
		float OcclusionFadeEnd = 1.0f;
		float SurfaceEplison = 0.05f;
	};

	class SsaoManager : public Manager
	{
	public:
		SsaoManager(
			RenderData* renderData,
			uint32_t blurCount = 6,
			DXGI_FORMAT normalMapFormat = DXGI_FORMAT_R16G16B16A16_SNORM,
			DXGI_FORMAT ambientMapFormat = DXGI_FORMAT_R16_UNORM);
		SsaoManager(const SsaoManager&) = delete;
		SsaoManager(SsaoManager&&) = delete;
		SsaoManager& operator=(const SsaoManager&) = delete;

		virtual void Draw()override;
		virtual void Update()override;
		virtual void OnResize()override;
		virtual void ReleaseIntermediateBuffers()override;

		static void InitSsaoCBHeap(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);

		void SetBlurCount(uint32_t blurCout);
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetSsaoSrv();
	protected:
		virtual void InitRootSignature()override;
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
		virtual void InitResources()override;
		virtual void InitDescriptors()override;

		void InitRandomVectors();
		void InitRandomVectorMap();

		void InitConstants();

		void DrawNormalsAndDepth();
		void DrawSsao();
		void DrawAmbientMap();
		void DrawAmbientMap(bool horzBlur);

		vector<float> CalcGaussWeights(float sigma);
		void GetOffsetVectors(DirectX::XMFLOAT4 offsets[14]);

		unique_ptr<DefaultResource> mNormalMap;
		unique_ptr<DefaultResource> mRandomVecMap;
		unique_ptr<DefaultResource> mAmbientMap0;
		unique_ptr<DefaultResource> mAmbientMap1;

		uint32_t mBlurCount = 3;

		enum
		{
			NORMAL_SRV, RAND_VEC_SRV, AMBIENT0_SRV, AMBIENT1_SRV, SSAO_SRV_COUNT
		};

		enum
		{
			NORMAL_RTV, AMBIENT0_RTV, AMBIENT1_RTV, SSAO_RTV_COUNT
		};

		DXGI_FORMAT mNormalMapFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
		DXGI_FORMAT mAmbientMapFormat = DXGI_FORMAT_R16_UNORM;

		DirectX::XMFLOAT4 mOffsets[14];

		unique_ptr<SsaoConstants> mSsaoConstants;
		unique_ptr<HeapAllocInfo> mSsaoCBAllocInfo;
		static unique_ptr<CircularHeap> SsaoCBHeap;
	};
}

