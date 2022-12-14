#pragma once
#include <utils/d3dx12.h>
#include <DirectXCollision.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

namespace Carol
{
	class HeapManager;
    class HeapAllocInfo;
	class DescriptorManager;
    class Shader;
	class RootSignature;
	class Display;
	class Timer;
	class Camera;
    class FrameConstants;
	class GlobalResources;

	class BaseRenderer
	{
	public:
		BaseRenderer(HWND hWnd, uint32_t width, uint32_t height);
		BaseRenderer(const BaseRenderer&) = delete;
		BaseRenderer(BaseRenderer&&) = delete;
		BaseRenderer& operator=(const BaseRenderer&) = delete;

		virtual void CalcFrameState();
		virtual void Update() = 0;
		virtual void Draw() = 0;
		virtual void Tick();
		virtual void Stop();
		virtual void Start();

		virtual void OnMouseDown(WPARAM btnState, int x, int y) = 0;
		virtual void OnMouseUp(WPARAM btnState, int x, int y) = 0;
		virtual void OnMouseMove(WPARAM btnState, int x, int y) = 0;
		virtual void OnKeyboardInput() = 0;
		virtual void OnResize(uint32_t width, uint32_t height, bool init = false);
		
		virtual void SetPaused(bool state);
		virtual bool Paused();
		virtual void SetMaximized(bool state);
		virtual bool Maximized();
		virtual void SetMinimized(bool state);
		virtual bool Minimized();
		virtual void SetResizing(bool state);
		virtual bool Resizing();

	protected:
		float AspectRatio();

		virtual void InitTimer();
		virtual void InitCamera();

		virtual void InitDebug();
		virtual void InitDxgiFactory();
		virtual void InitDevice();
		virtual void InitFence();

		virtual void InitCommandQueue();
		virtual void InitCommandAllocator();
		virtual void InitCommandList();
		virtual void InitRootSignature();

		virtual void InitHeapManager();
		virtual void InitDescriptorManager();
		virtual void InitDisplay();

		virtual void InitShaders();
		virtual void InitPSOs();

		virtual void FlushCommandQueue();

	protected:
		Microsoft::WRL::ComPtr<ID3D12Debug> mDebugLayer;
		Microsoft::WRL::ComPtr<IDXGIFactory> mDxgiFactory;
		Microsoft::WRL::ComPtr<ID3D12Device> mDevice;
		Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
		uint32_t mCpuFence;

		Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mInitCommandAllocator;

		std::unique_ptr<RootSignature> mRootSignature;
		std::unique_ptr<HeapManager> mHeapManager;
		std::unique_ptr<DescriptorManager> mDescriptorManager;

        std::unordered_map<std::wstring, std::unique_ptr<Shader>> mShaders;
        std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;
		D3DX12_MESH_SHADER_PIPELINE_STATE_DESC mBaseGraphicsPsoDesc;
		D3D12_COMPUTE_PIPELINE_STATE_DESC mBaseComputePsoDesc;

        uint32_t mNumFrame = 3;
        uint32_t mCurrFrame = 0;
        std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> mFrameAllocator;
        std::vector<uint32_t> mGpuFence = { 0,0,0 };

		std::unique_ptr<Display> mDisplay;

		std::unique_ptr<Timer> mTimer;
		std::unique_ptr<Camera> mCamera;

		D3D12_VIEWPORT mScreenViewport;
		D3D12_RECT mScissorRect;

		HWND mhWnd;
		std::wstring mMainWndCaption = L"Carol";
		D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;

		uint32_t mClientWidth = 800;
		uint32_t mClientHeight = 600;		
		DirectX::XMINT2 mLastMousePos;

		std::unique_ptr<GlobalResources> mGlobalResources;

		bool mPaused = false;
		bool mMaximized = false;
		bool mMinimized = false;
		bool mResizing = false;
	};
}