#pragma once
#include "../Utils/Buddy.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <memory>

using Microsoft::WRL::ComPtr;
using std::vector;
using std::unique_ptr;

namespace Carol
{
	class Heap;
	class Bitset;

	class HeapAllocInfo
	{
	public:
		Heap* Heap;
		uint64_t Bytes = 0;
		uint64_t Addr = 0;
	};

	class Heap
	{
	public:
		Heap(ID3D12Device* device, D3D12_HEAP_TYPE type, D3D12_HEAP_FLAGS flag);
		virtual void CreateResource(ComPtr<ID3D12Resource>* resource, D3D12_RESOURCE_DESC* desc, HeapAllocInfo* info, D3D12_RESOURCE_STATES initState, D3D12_CLEAR_VALUE* optimizedClearValue = nullptr) = 0;
		virtual void DeleteResource(HeapAllocInfo* info) = 0;

	protected:
		virtual bool Allocate(uint32_t size, HeapAllocInfo* info) = 0;
		virtual bool Deallocate(HeapAllocInfo* info) = 0;

		ID3D12Device* mDevice;
		D3D12_HEAP_TYPE mType;
		D3D12_HEAP_FLAGS mFlag;
	};

	class BuddyHeap : public Heap
	{
	public:
		BuddyHeap(
			ID3D12Device* device,
			D3D12_HEAP_TYPE type,
			D3D12_HEAP_FLAGS flag,
			uint32_t heapSize = 1 << 26,
			uint32_t pageSize = 1 << 16);
		virtual void CreateResource(
			ComPtr<ID3D12Resource>* resource,
			D3D12_RESOURCE_DESC* desc,
			HeapAllocInfo* info,
			D3D12_RESOURCE_STATES initState,
			D3D12_CLEAR_VALUE* optimizedClearValue = nullptr)override;
		virtual void DeleteResource(HeapAllocInfo* info);

	protected:
		virtual bool Allocate(uint32_t size, HeapAllocInfo* info)override;
		virtual bool Deallocate(HeapAllocInfo* info)override;

		void Align();
		void AddHeap();

		vector<ComPtr<ID3D12Heap>> mHeaps;
		vector<unique_ptr<Buddy>> mBuddies;

		uint32_t mHeapSize;
		uint32_t mPageSize;
		uint32_t mNumPages;
	};

	class CircularHeap : public Heap
	{
	public:
		CircularHeap(
			ID3D12Device* device,
			ID3D12GraphicsCommandList* cmdList,
			bool isConstant,
			uint32_t elementCount = 32,
			uint32_t elementSize = 256);
		~CircularHeap();

		virtual void CreateResource(
			ComPtr<ID3D12Resource>* resource,
			D3D12_RESOURCE_DESC* desc,
			HeapAllocInfo* info,
			D3D12_RESOURCE_STATES initState = D3D12_RESOURCE_STATE_COMMON,
			D3D12_CLEAR_VALUE* optimizedClearValue = nullptr);
		virtual void DeleteResource(HeapAllocInfo* info);

		void CopyData(HeapAllocInfo* info, const void* data);
		D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress(HeapAllocInfo* info);

	protected:
		virtual bool Allocate(uint32_t size, HeapAllocInfo* info)override;
		virtual bool Deallocate(HeapAllocInfo* info)override;

		void Align();
		void ExpandHeap();

		vector<ComPtr<ID3D12Resource>> mHeaps;
		vector<byte*> mMappedData;

		ID3D12GraphicsCommandList* mCommandList;

		uint32_t mElementCount = 0;
		uint32_t mElementSize = 0;
		uint32_t mBufferSize = 0;
		
		uint32_t mHeapSize = 0;

		uint32_t mBegin;
		uint32_t mEnd;
		uint32_t mQueueSize = 0;
	};

	class SegListHeap : public Heap
	{
	public:
		SegListHeap(
			ID3D12Device* device,
			D3D12_HEAP_TYPE type,
			D3D12_HEAP_FLAGS flag,
			uint32_t maxPageSize = 1 << 26);
		virtual void CreateResource(
			ComPtr<ID3D12Resource>* resource, 
			D3D12_RESOURCE_DESC* desc, HeapAllocInfo* info, 
			D3D12_RESOURCE_STATES initState, 
			D3D12_CLEAR_VALUE* optimizedClearValue = nullptr);
		virtual void DeleteResource(HeapAllocInfo* info);

	protected:
		virtual bool Allocate(uint32_t size, HeapAllocInfo* info);
		virtual bool Deallocate(HeapAllocInfo* info);

		uint32_t GetOrder(uint32_t size);
		void AddHeap(uint32_t order);

		vector<vector<ComPtr<ID3D12Heap>>> mSegLists;
		vector<vector<unique_ptr<Bitset>>> mBitsets;

		uint32_t mMaxNumPages;
		uint32_t mMinPageSize = 65536;
		uint32_t mOrder;
	};
}
