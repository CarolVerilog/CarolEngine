#pragma once
#include <utils/d3dx12.h>
#include <utils/common.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <memory>

namespace Carol
{
	class Heap;
	class HeapAllocInfo;
	class DescriptorManager;
	class DescriptorAllocInfo;

	DXGI_FORMAT GetBaseFormat(DXGI_FORMAT format);
	DXGI_FORMAT GetUavFormat(DXGI_FORMAT format);
	DXGI_FORMAT GetDsvFormat(DXGI_FORMAT format);
	uint32_t GetPlaneSize(DXGI_FORMAT format);

	class Resource
	{
	public:
		Resource();
		Resource(
			D3D12_RESOURCE_DESC* desc,
			Heap* heap,
			D3D12_RESOURCE_STATES initState = D3D12_RESOURCE_STATE_COMMON,
			D3D12_CLEAR_VALUE* optimizedClearValue = nullptr);
		virtual ~Resource();

		ID3D12Resource* Get();
		ID3D12Resource** GetAddressOf();
		D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress();

		void CopySubresources(
			ID3D12GraphicsCommandList* cmdList,
			Heap* intermediateHeap,
			const void* data,
			uint32_t byteSize
		);

		void CopySubresources(
			ID3D12GraphicsCommandList* cmdList,
			Heap* intermediateHeap,
			D3D12_SUBRESOURCE_DATA* subresources,
			uint32_t firstSubresource,
			uint32_t numSubresources
		);

		void CopyData(const void* data, uint32_t byteSize);
		void ReleaseIntermediateBuffer();

	protected:
		Microsoft::WRL::ComPtr<ID3D12Resource> mResource;
		std::unique_ptr<HeapAllocInfo> mAllocInfo;

		Microsoft::WRL::ComPtr<ID3D12Resource> mIntermediateBuffer;
		std::unique_ptr<HeapAllocInfo> mIntermediateBufferAllocInfo;

		byte* mMappedData;
	};

	class Buffer
	{
	public:
		Buffer(
			DescriptorManager* descriptorManager);
		~Buffer();

		ID3D12Resource* Get();
		ID3D12Resource** GetAddressOf();
		D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress();

		void CopySubresources(
			ID3D12GraphicsCommandList* cmdList,
			Heap* intermediateHeap,
			const void* data,
			uint32_t byteSize
		);

		void CopySubresources(
			ID3D12GraphicsCommandList* cmdList,
			Heap* intermediateHeap,
			D3D12_SUBRESOURCE_DATA* subresources,
			uint32_t firstSubresource,
			uint32_t numSubresources
		);

		void CopyData(const void* data, uint32_t byteSize);
		void ReleaseIntermediateBuffer();

		D3D12_CPU_DESCRIPTOR_HANDLE GetCpuSrv(uint32_t planeSlice = 0);
		D3D12_CPU_DESCRIPTOR_HANDLE GetShaderCpuSrv(uint32_t planeSlice = 0);
		D3D12_GPU_DESCRIPTOR_HANDLE GetGpuSrv(uint32_t planeSlice = 0);
		uint32_t GetGpuSrvIdx(uint32_t planeSlice = 0);

		D3D12_CPU_DESCRIPTOR_HANDLE GetCpuUav(uint32_t mipSlice = 0, uint32_t planeSlice = 0);
		D3D12_CPU_DESCRIPTOR_HANDLE GetShaderCpuUav(uint32_t mipSlice = 0, uint32_t planeSlice = 0);
		D3D12_GPU_DESCRIPTOR_HANDLE GetGpuUav(uint32_t mipSlice = 0, uint32_t planeSlice = 0);
		uint32_t GetGpuUavIdx(uint32_t mipSlice = 0, uint32_t planeSlice = 0);

		D3D12_CPU_DESCRIPTOR_HANDLE GetRtv(uint32_t mipSlice = 0, uint32_t planeSlice = 0);
		D3D12_CPU_DESCRIPTOR_HANDLE GetDsv(uint32_t mipSlice = 0);

	protected:
		void BindDescriptors();
		void CopyCbvSrvUav();

		virtual void BindSrv() = 0;
		virtual void BindUav() = 0;
		virtual void BindRtv() = 0;
		virtual void BindDsv() = 0;

		std::unique_ptr<Resource> mResource;
		D3D12_RESOURCE_DESC mResourceDesc;

		DescriptorManager* mDescriptorManager;

		std::unique_ptr<DescriptorAllocInfo> mCpuSrvAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mGpuSrvAllocInfo;
		
		std::unique_ptr<DescriptorAllocInfo> mCpuUavAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mGpuUavAllocInfo;

		std::unique_ptr<DescriptorAllocInfo> mRtvAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mDsvAllocInfo;
	};

	enum ColorBufferViewDimension
	{
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE1D,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE1DARRAY,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2DARRAY,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2DMS,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2DMSARRAY,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE3D,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURECUBE,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURECUBEARRAY,
		COLOR_BUFFER_VIEW_DIMENSION_UNKNOWN
	};

	class ColorBuffer : public Buffer
	{
	public:
		ColorBuffer(
			uint32_t width,
			uint32_t height,
			uint32_t depthOrArraySize,
			ColorBufferViewDimension viewDimension,
			DXGI_FORMAT format,
			Heap* heap,
			DescriptorManager* descriptorManager,
			D3D12_RESOURCE_STATES initState,
			D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
			D3D12_CLEAR_VALUE* optClearValue = nullptr,
			uint32_t mipLevels = 1,
			uint32_t viewMipLevles = 0,
			uint32_t mostDetailedMip = 0,
			float resourceMinLODClamp = 0.f,
			uint32_t viewArraySize = 0,
			uint32_t firstArraySlice = 0,
			uint32_t sampleCount = 1,
			uint32_t sampleQuality = 0);

	protected:
		virtual void BindSrv()override;
		virtual void BindUav()override;
		virtual void BindRtv()override;
		virtual void BindDsv()override;

		D3D12_RESOURCE_DIMENSION GetResourceDimension(ColorBufferViewDimension viewDimension);

		ColorBufferViewDimension mViewDimension;
		DXGI_FORMAT mFormat;

		uint32_t mViewMipLevels;
		uint32_t mMostDetailedMip;
		float mResourceMinLODClamp;

		uint32_t mViewArraySize;
		uint32_t mFirstArraySlice;
		uint32_t mPlaneSize;
	};

	class StructuredBuffer : public Buffer
	{
	public:
		StructuredBuffer(
			uint32_t numElements,
			uint32_t elementSize,
			Heap* heap,
			DescriptorManager* descriptorManager,
			D3D12_RESOURCE_STATES initState,
			D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
			uint32_t viewNumElements = 0,
			uint32_t firstElement = 0
		);
	protected:
		virtual void BindSrv()override;
		virtual void BindUav()override;
		virtual void BindRtv()override;
		virtual void BindDsv()override;

		uint32_t mNumElements;
		uint32_t mElementSize;
		uint32_t mViewNumElements;
		uint32_t mFirstElement;
	};

	class RawBuffer : public Buffer
	{
	public:
		RawBuffer(
			uint32_t byteSize,
			Heap* heap,
			DescriptorManager* descriptorManager,
			D3D12_RESOURCE_STATES initState,
			D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE
		);

	protected:
		virtual void BindSrv()override;
		virtual void BindUav()override;
		virtual void BindRtv()override;
		virtual void BindDsv()override;
	};
}
