#pragma once
#include <render_pass/render_pass.h>
#include <scene/light.h>
#include <DirectXMath.h>
#include <memory>

#define MAX_LIGHTS 16

namespace Carol
{
	class GlobalResources;
	class DefaultResource;
	class CircularHeap;
	class HeapAllocInfo;

	class FramePass :public RenderPass
	{
	public:
		FramePass(GlobalResources* globalResources, DXGI_FORMAT frameFormat = DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT depthStencilResourceFormat = DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT depthStencilDsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT depthStencilSrvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
		FramePass(const FramePass&) = delete;
		FramePass(FramePass&&) = delete;
		FramePass& operator=(const FramePass&) = delete;
		
		virtual void Draw()override;
		virtual void Update()override;
		virtual void OnResize()override;
		virtual void ReleaseIntermediateBuffers()override;

		CD3DX12_CPU_DESCRIPTOR_HANDLE GetFrameRtv();
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetDepthStencilDsv();
		uint32_t GetFrameSrvIdx();
		uint32_t GetDepthStencilSrvIdx();

	protected:
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
		virtual void InitResources()override;
		virtual void InitDescriptors()override;

		enum
		{
			FRAME_SRV, DEPTH_STENCIL_SRV, FRAME_CBV_SRV_UAV_COUNT
		};

		enum
		{
			FRAME_RTV, FRAME_RTV_COUNT
		};

		enum
		{
			DEPTH_STENCIL_DSV, FRAME_DSV_COUNT
		};

		std::unique_ptr<DefaultResource> mFrameMap;
		std::unique_ptr<DefaultResource> mDepthStencilMap;

		DXGI_FORMAT mFrameFormat;
		DXGI_FORMAT mDepthStencilResourceFormat;
		DXGI_FORMAT mDepthStencilDsvFormat;
		DXGI_FORMAT mDepthStencilSrvFormat;
	};
}