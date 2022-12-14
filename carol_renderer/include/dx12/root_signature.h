#pragma once
#include <utils/d3dx12.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <comdef.h>
#include <vector>
#include <memory>
#include <string>

namespace Carol
{
	class DescriptorAllocator;
	class DescriptorAllocInfo;

	class RootSignature
	{
	public:
		RootSignature(ID3D12Device* device);
		ID3D12RootSignature* Get();

		enum {
			MESH_CB,
			MESH_CONSTANTS,
			SKINNED_CB,
			PASS_CONSTANTS,
			FRAME_CB,
			FRAME_CONSTANTS,
			ROOT_SIGNATURE_COUNT
		};

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
	};
}
