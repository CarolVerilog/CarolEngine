#include "Renderer.h"
#include "Manager/Display/Display.h"
#include "Manager/Ambient Occlusion/Ssao.h"
#include "Manager/Anti-Aliasing/Taa.h"
#include "Manager/Light/Light.h"
#include "Manager/Transparency/Oitppll.h"
#include "Manager/Mesh/Mesh.h"
#include "DirectX/Heap.h"
#include "DirectX/DescriptorAllocator.h"
#include "DirectX/Shader.h"
#include "DirectX/Sampler.h"
#include "Resource/Texture.h"
#include "Resource/GameTimer.h"
#include "Resource/Camera.h"
#include "Resource/SkinnedData.h"
#include "Resource/AssimpModel.h"
#include "Utils/Common.h"
#include <DirectXColors.h>

using std::make_unique;
using namespace DirectX;

unique_ptr<Carol::CircularHeap> Carol::SsaoManager::SsaoCBHeap = nullptr;
unique_ptr<Carol::CircularHeap> Carol::LightManager::ShadowCBHeap = nullptr;
unique_ptr<Carol::CircularHeap> Carol::MeshManager::MeshCBHeap = nullptr;
unique_ptr<Carol::CircularHeap> Carol::AssimpModel::SkinnedCBHeap = nullptr;

Carol::Renderer::Renderer(HWND hWnd, uint32_t width, uint32_t height)
	:BaseRenderer(hWnd, width, height)
{
	ThrowIfFailed(mInitCommandAllocator->Reset());
	ThrowIfFailed(mCommandList->Reset(mInitCommandAllocator.Get(), nullptr));
	
	InitFrameAllocators();
	InitRootSignature();
	InitConstants();
	InitShaders();
	InitPSOs();
	InitSsao();
	InitTaa();
	InitMainLight();
	InitOitppll();
	InitMeshes();
	InitGpuDescriptors();

	mCommandList->Close();
	vector<ID3D12CommandList*> cmdLists{ mCommandList.Get()};
	mCommandQueue->ExecuteCommandLists(1, cmdLists.data());
	FlushCommandQueue();
	ReleaseIntermediateBuffers();
}

void Carol::Renderer::InitFrameAllocators()
{
	mFrameAllocator.resize(mNumFrame);
	for (int i = 0; i < mNumFrame; ++i)
	{
		ThrowIfFailed(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mFrameAllocator[i].GetAddressOf())));
	}
}

void Carol::Renderer::InitRootSignature()
{
	CD3DX12_ROOT_PARAMETER rootPara[9];
	rootPara[0].InitAsConstantBufferView(0);
	rootPara[1].InitAsConstantBufferView(1);
	rootPara[2].InitAsConstantBufferView(2);
	rootPara[3].InitAsConstantBufferView(3);

	CD3DX12_DESCRIPTOR_RANGE range[5];
	range[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);
	range[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
	range[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
	range[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 3, 0);
	range[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);

	rootPara[4].InitAsDescriptorTable(1, &range[0], D3D12_SHADER_VISIBILITY_PIXEL);
	rootPara[5].InitAsDescriptorTable(1, &range[1], D3D12_SHADER_VISIBILITY_PIXEL);
	rootPara[6].InitAsDescriptorTable(1, &range[2], D3D12_SHADER_VISIBILITY_PIXEL);
	rootPara[7].InitAsDescriptorTable(1, &range[3], D3D12_SHADER_VISIBILITY_PIXEL);
	rootPara[8].InitAsDescriptorTable(1, &range[4], D3D12_SHADER_VISIBILITY_PIXEL);
	
	auto staticSamplers = StaticSampler::GetDefaultStaticSamplers();
	CD3DX12_ROOT_SIGNATURE_DESC slotRootSigDesc(9, rootPara, staticSamplers.size(), staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	Blob serializedRootSigBlob;
	Blob errorBlob;

	auto hr = D3D12SerializeRootSignature(&slotRootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSigBlob.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob.Get())
	{
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	
	ThrowIfFailed(hr);
	ThrowIfFailed(mDevice->CreateRootSignature(0, serializedRootSigBlob->GetBufferPointer(), serializedRootSigBlob->GetBufferSize(), IID_PPV_ARGS(mRootSignature.GetAddressOf())));

	mRenderData->RootSignature = mRootSignature.Get();
}

void Carol::Renderer::InitConstants()
{
	mPassCBHeap = make_unique<CircularHeap>(mDevice.Get(), mCommandList.Get(), true, 1, sizeof(PassConstants));
	mPassConstants = make_unique<PassConstants>();
	mPassCBAllocInfo = make_unique<HeapAllocInfo>();
	 
	mRenderData->PassCBHeap = mPassCBHeap.get();
	mRenderData->PassCBAllocInfo = mPassCBAllocInfo.get();
}

void Carol::Renderer::InitShaders()
{
	const D3D_SHADER_MACRO skinnedDefines[] =
	{
		"TAA","1",
		"SSAO","1",
		"SKINNED","1",
		NULL,NULL
	};

	const D3D_SHADER_MACRO staticDefines[] =
	{
		"TAA","1",
		"SSAO","1",
		NULL,NULL
	};

	mShaders[L"OpaqueStaticVS"] = make_unique<Shader>(L"Shaders\\Default.hlsl", staticDefines, "VS", "vs_5_1");
	mShaders[L"OpaqueStaticPS"] = make_unique<Shader>(L"Shaders\\Default.hlsl", staticDefines, "PS", "ps_5_1");
	mShaders[L"OpaqueSkinnedVS"] = make_unique<Shader>(L"Shaders\\Default.hlsl", skinnedDefines, "VS", "vs_5_1");
	mShaders[L"OpauqeSkinnedPS"] = make_unique<Shader>(L"Shaders\\Default.hlsl", skinnedDefines, "PS", "ps_5_1");
	mShaders[L"SkyBoxVS"] = make_unique<Shader>(L"Shaders\\SkyBox.hlsl", staticDefines, "VS", "vs_5_1");
	mShaders[L"SkyBoxPS"] = make_unique<Shader>(L"Shaders\\SkyBox.hlsl", staticDefines, "PS", "ps_5_1");

	mRenderData->Shaders = &mShaders;
}

void Carol::Renderer::InitPSOs()
{
	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "WEIGHTS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{ "BONEINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, 56, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	mNullInputLayout.resize(0);

	mBasePsoDesc.InputLayout = { mInputLayout.data(),(uint32_t)mInputLayout.size() };
	mBasePsoDesc.pRootSignature = mRootSignature.Get();
	mBasePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    mBasePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	mBasePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    mBasePsoDesc.SampleMask = UINT_MAX;
    mBasePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    mBasePsoDesc.NumRenderTargets = 1;
    mBasePsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    mBasePsoDesc.SampleDesc.Count = 1;
    mBasePsoDesc.SampleDesc.Quality = 0;
    mBasePsoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	mRenderData->BasePsoDesc = &mBasePsoDesc;

	auto opaqueStaticPsoDesc = mBasePsoDesc;
	auto opaqueStaticVSBlob = mShaders[L"OpaqueStaticVS"]->GetBlob()->Get();
	auto opaqueStaticPSBlob = mShaders[L"OpaqueStaticPS"]->GetBlob()->Get();
	opaqueStaticPsoDesc.VS = { reinterpret_cast<byte*>(opaqueStaticVSBlob->GetBufferPointer()),opaqueStaticVSBlob->GetBufferSize() };
	opaqueStaticPsoDesc.PS = { reinterpret_cast<byte*>(opaqueStaticPSBlob->GetBufferPointer()),opaqueStaticPSBlob->GetBufferSize() };
	ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&opaqueStaticPsoDesc, IID_PPV_ARGS(mPSOs[L"OpaqueStatic"].GetAddressOf())));

	auto opaqueSkinnedPsoDesc = opaqueStaticPsoDesc;
	auto opaqueSkinnedVSBlob = mShaders[L"OpaqueSkinnedVS"]->GetBlob()->Get();
	auto opaqueSkinnedPSBlob = mShaders[L"OpauqeSkinnedPS"]->GetBlob()->Get();
	opaqueSkinnedPsoDesc.VS = { reinterpret_cast<byte*>(opaqueSkinnedVSBlob->GetBufferPointer()),opaqueSkinnedVSBlob->GetBufferSize() };
	opaqueSkinnedPsoDesc.PS = { reinterpret_cast<byte*>(opaqueSkinnedPSBlob->GetBufferPointer()),opaqueSkinnedPSBlob->GetBufferSize() };	
	ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&opaqueSkinnedPsoDesc, IID_PPV_ARGS(mPSOs[L"OpaqueSkinned"].GetAddressOf())));

	auto skyBoxPsoDesc = mBasePsoDesc;
	auto skyBoxVSBlob = mShaders[L"SkyBoxVS"]->GetBlob()->Get();
	auto skyBoxPSBlob = mShaders[L"SkyBoxPS"]->GetBlob()->Get();
	skyBoxPsoDesc.VS = { reinterpret_cast<byte*>(skyBoxVSBlob->GetBufferPointer()),skyBoxVSBlob->GetBufferSize() };
	skyBoxPsoDesc.PS = { reinterpret_cast<byte*>(skyBoxPSBlob->GetBufferPointer()),skyBoxPSBlob->GetBufferSize() };
	skyBoxPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&skyBoxPsoDesc, IID_PPV_ARGS(mPSOs[L"SkyBox"].GetAddressOf())));

	mRenderData->PSOs = &mPSOs;
}

void Carol::Renderer::InitSsao()
{
	mSsao = make_unique<SsaoManager>(mRenderData.get());
	SsaoManager::InitSsaoCBHeap(mDevice.Get(), mCommandList.Get());
	mRenderData->Ssao = mSsao.get();
}

void Carol::Renderer::InitTaa()
{
	mTaa = make_unique<TaaManager>(mRenderData.get());
	mRenderData->Taa = mTaa.get();
}

void Carol::Renderer::InitMainLight()
{
	LightData lightData = {};
	lightData.Direction = { 0.8f,-1.0f,1.0f };
	lightData.Strength = { 0.8f,0.8f,0.8f };
	mMainLight = make_unique<LightManager>(mRenderData.get(), lightData, 2048, 2048);
	LightManager::InitShadowCBHeap(mDevice.Get(), mCommandList.Get());
	mRenderData->MainLight = mMainLight.get();
}

void Carol::Renderer::InitOitppll()
{
	mOitppll = make_unique<OitppllManager>(mRenderData.get());
	mRenderData->Oitppll = mOitppll.get();
}

void Carol::Renderer::InitMeshes()
{
	MeshManager::InitMeshCBHeap(mDevice.Get(), mCommandList.Get());
	AssimpModel::InitSkinnedCBHeap(mDevice.Get(), mCommandList.Get());

	mRenderData->OpaqueStaticMeshes = &mOpaqueStaticMeshes;
	mRenderData->OpaqueSkinnedMeshes = &mOpaqueSkinnedMeshes;
	mRenderData->TransparentStaticMeshes = &mTransparentStaticMeshes;
	mRenderData->TransparentSkinnedMeshes = &mTransparentSkinnedMeshes;
	mRenderData->Models = &mModels;

	auto ground = make_unique<AssimpModel>(mRenderData.get());
	ground->LoadFlatGround();
	mModels[L"Ground"] = std::move(ground);

	auto skyBox = make_unique<AssimpModel>(mRenderData.get());
	skyBox->LoadSkyBox();
	mSkyBox = std::move(skyBox);
}

void Carol::Renderer::InitGpuDescriptors()
{
	mDisplay->CopyDescriptors();
	mSsao->CopyDescriptors();
	mTaa->CopyDescriptors();
	mMainLight->CopyDescriptors();
	mOitppll->CopyDescriptors();
}

void Carol::Renderer::ReleaseIntermediateBuffers()
{
	mSsao->ReleaseIntermediateBuffers();
	mOitppll->ReleaseIntermediateBuffers();

	auto& mesh = mModels[L"Ground"]->GetMeshes().begin()->second;
	mesh->ReleaseIntermediateBuffers();
}

void Carol::Renderer::Draw()
{	
	ID3D12DescriptorHeap* descriptorHeaps[] = {mCbvSrvUavAllocator->GetGpuDescriptorHeap()};
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mMainLight->Draw();
	mSsao->Draw();
	mTaa->Draw();
	
	mCommandList->Close();
	vector<ID3D12CommandList*> cmdLists{ mCommandList.Get()};
	mCommandQueue->ExecuteCommandLists(1, cmdLists.data());

	mDisplay->Present();
	mGpuFence[mCurrFrame] = ++mCpuFence;
	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCpuFence));

	mCurrFrame = (mCurrFrame + 1) % mNumFrame;
}

void Carol::Renderer::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhWnd);
}

void Carol::Renderer::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void Carol::Renderer::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		mCamera->RotateY(dx);
		mCamera->Pitch(dy);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void Carol::Renderer::OnKeyboardInput()
{
	const float dt = mTimer->DeltaTime();

	if (GetAsyncKeyState('W') & 0x8000)
		mCamera->Walk(10.0f * dt);

	if (GetAsyncKeyState('S') & 0x8000)
		mCamera->Walk(-10.0f * dt);

	if (GetAsyncKeyState('A') & 0x8000)
		mCamera->Strafe(-10.0f * dt);

	if (GetAsyncKeyState('D') & 0x8000)
		mCamera->Strafe(10.0f * dt);
}

void Carol::Renderer::Update()
{
	OnKeyboardInput();

	if (mGpuFence[mCurrFrame] != 0 && mFence->GetCompletedValue() < mGpuFence[mCurrFrame])
	{
		HANDLE eventHandle = CreateEventEx(nullptr, LPCWSTR(nullptr), 0, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mGpuFence[mCurrFrame], eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	ThrowIfFailed(mFrameAllocator[mCurrFrame]->Reset());
	ThrowIfFailed(mCommandList->Reset(mFrameAllocator[mCurrFrame].Get(), nullptr));

	UpdateCamera();
	UpdatePassCB();
	UpdateAnimations();
	UpdateMeshes();
	mSsao->Update();
	mTaa->Update();
	mMainLight->Update();
}

void Carol::Renderer::UpdateCamera()
{
	mCamera->UpdateViewMatrix();
}

void Carol::Renderer::UpdatePassCB()
{
	XMMATRIX view = mCamera->GetView();
	XMMATRIX invView = XMMatrixInverse(GetRvaluePtr(XMMatrixDeterminant(view)), view);
	XMMATRIX proj = mCamera->GetProj();
	XMMATRIX invProj = XMMatrixInverse(GetRvaluePtr(XMMatrixDeterminant(proj)), proj);
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invViewProj = XMMatrixInverse(GetRvaluePtr(XMMatrixDeterminant(viewProj)), viewProj);

	XMStoreFloat4x4(&mPassConstants->View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mPassConstants->InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mPassConstants->Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mPassConstants->InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mPassConstants->ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mPassConstants->InvViewProj, XMMatrixTranspose(invViewProj));

	static XMMATRIX tex(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f
	);
	XMStoreFloat4x4(&mPassConstants->ViewProjTex, XMMatrixTranspose(XMMatrixMultiply(viewProj, tex)));

	XMFLOAT4X4 jitteredProj4x4f = mCamera->GetProj4x4f();
	mTaa->GetHalton(jitteredProj4x4f._31, jitteredProj4x4f._32);
	mTaa->SetHistViewProj(viewProj);

	XMMATRIX histViewProj = mTaa->GetHistViewProj();
	XMMATRIX jitteredProj = XMLoadFloat4x4(&jitteredProj4x4f);
	XMMATRIX jitteredViewProj = XMMatrixMultiply(view, jitteredProj);

	XMStoreFloat4x4(&mPassConstants->HistViewProj, XMMatrixTranspose(histViewProj));
	XMStoreFloat4x4(&mPassConstants->JitteredViewProj, XMMatrixTranspose(jitteredViewProj));

	mPassConstants->EyePosW = mCamera->GetPosition3f();
	mPassConstants->RenderTargetSize = { static_cast<float>(mClientWidth), static_cast<float>(mClientHeight) };
	mPassConstants->InvRenderTargetSize = { 1.0f / mClientWidth, 1.0f / mClientHeight };
	mPassConstants->NearZ = mCamera->GetNearZ();
	mPassConstants->FarZ = mCamera->GetFarZ();

	mPassConstants->Lights[0] = mMainLight->GetLightData();
	mPassCBHeap->DeleteResource(mPassCBAllocInfo.get());
	mPassCBHeap->CreateResource(nullptr, nullptr, mPassCBAllocInfo.get());
	mPassCBHeap->CopyData(mPassCBAllocInfo.get(), mPassConstants.get());
}

void Carol::Renderer::UpdateMeshes()
{
	mOpaqueStaticMeshes.clear();
	mOpaqueSkinnedMeshes.clear();
	mTransparentStaticMeshes.clear();
	mTransparentSkinnedMeshes.clear();

	for (auto& modelMapPair : mModels)
	{
		for (auto& meshMapPair : modelMapPair.second->GetMeshes())
		{
			auto& mesh = meshMapPair.second;
			mesh->Update();

			if (mCamera->Contains(mesh->GetBoundingBox()))
			{
				if (!mesh->IsSkinned() && !mesh->IsTransparent())
				{
					mOpaqueStaticMeshes.push_back(mesh.get());
				}
				else if (mesh->IsSkinned() && !mesh->IsTransparent())
				{
					mOpaqueSkinnedMeshes.push_back(mesh.get());
				}
				else if (!mesh->IsSkinned() && mesh->IsTransparent())
				{
					mTransparentStaticMeshes.push_back(mesh.get());
				}
				else
				{
					mTransparentSkinnedMeshes.push_back(mesh.get());
				}
			}
		}
	}
}

void Carol::Renderer::UpdateAnimations()
{
	for (auto& modelMapPair : mModels)
	{
		modelMapPair.second->UpdateAnimation();
	}
}

void Carol::Renderer::OnResize(uint32_t width, uint32_t height)
{
	BaseRenderer::OnResize(width, height);
	mSsao->OnResize();
	mTaa->OnResize();
	mOitppll->OnResize();

	mDisplay->CopyDescriptors();
	mSsao->CopyDescriptors();
	mTaa->CopyDescriptors();
	mOitppll->CopyDescriptors();
}

void Carol::Renderer::LoadModel(wstring path, wstring textureDir, wstring modelName, DirectX::XMMATRIX world, bool isSkinned, bool isTransparent)
{
	ThrowIfFailed(mInitCommandAllocator->Reset());
	ThrowIfFailed(mCommandList->Reset(mInitCommandAllocator.Get(), nullptr));

	auto model = make_unique<AssimpModel>(mRenderData.get(), path, textureDir, isSkinned, isTransparent);
	if (!model->InitSucceeded())
	{
		mCommandList->Close();
		return;
	}

	model->SetWorld(world);
	mModels[modelName] = std::move(model);

	mCommandList->Close();
	vector<ID3D12CommandList*> cmdLists = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(1, cmdLists.data());
	FlushCommandQueue();
	
	for (auto& modelMapPair : mModels[modelName]->GetMeshes())
	{
		modelMapPair.second->ReleaseIntermediateBuffers();
	}
}

void Carol::Renderer::UnloadModel(wstring modelName)
{
	FlushCommandQueue();
	mModels.erase(modelName);
}

vector<wstring> Carol::Renderer::GetAnimationNames(wstring modelName)
{
	return mModels[modelName]->GetAnimations();
}

void Carol::Renderer::SetAnimation(wstring modelName, wstring animationName)
{
	mModels[modelName]->SetAnimation(animationName);
}

vector<wstring> Carol::Renderer::GetModelNames()
{
	vector<wstring> modelNames;

	for (auto& modelMapPair : mModels)
	{
		modelNames.push_back(modelMapPair.first);
	}

	return modelNames;
}
