#include "d3dApp.h"
#include "framework.h"
#include "resource.h"
#include <WindowsX.h>

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;


INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

const int gNumFrameResources = 3;

LRESULT CALLBACK
MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// main window procedure that picks up messages during the event loop
	return d3dApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

d3dApp* d3dApp::mApp = nullptr;
d3dApp* d3dApp::GetApp()
{
	return mApp;
}

d3dApp::d3dApp(HINSTANCE hInstance) : mhAppInst(hInstance)
{
	assert(mApp == nullptr);
	mApp = this;
}

d3dApp::~d3dApp()
{
	if (md3dDevice != nullptr) {
		FlushCommandQueue();
	}
}

HINSTANCE d3dApp::AppInst()const
{
	return mhAppInst;
}

HWND d3dApp::MainWnd()const
{
	return mhMainWnd;
}

float d3dApp::AspectRatio()const
{
	return static_cast<float>(mClientWidth) / mClientHeight;
}

bool d3dApp::Get4xMsaaState()const
{
	return m4xMsaaState;
}

void d3dApp::Set4xMsaaState(bool value)
{
	if(m4xMsaaState != value){
		m4xMsaaState = value;

		CreateSwapChain();
		OnResize();
	}
}

bool d3dApp::Initialize()
{
	if (!InitMainWindow()) {
		return false;
	}
	if (!InitDirect3D()) {
		return false;
	}

	OnResize();

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// Get the increment size of a descriptor in this heap type.  This is hardware specific, 
    // so we have to query this information.
	mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	LoadTextures();
	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();
	BuildRoomGeometry();
	BuildTreeGeometry();
	BuildSkullGeometry();
	BuildCarGeometry();
	BuildMaterials();
	BuildRenderItems();
	BuildFrameResources();
	BuildPSOs();

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	return true;
}

bool d3dApp::InitMainWindow()
{
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = mhAppInst;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"MainWnd";

	if (!RegisterClass(&wc))
	{
		MessageBox(0, L"RegisterClass Failed.", 0, 0);
		return false;
	}

	RECT R = { 0, 0, mClientWidth, mClientHeight };
	DWORD dwStyle = WS_OVERLAPPEDWINDOW | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_CAPTION;
	AdjustWindowRect(&R, dwStyle, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	mhMainWnd = CreateWindow(L"MainWnd", mMainWndCaption.c_str(),
		dwStyle, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, mhAppInst, 0);
	if (!mhMainWnd)
	{
		MessageBox(0, L"Create window failed.", 0, 0);
		return false;
	}

	ShowWindow(mhMainWnd, SW_SHOWMAXIMIZED); // SW_SHOW
	UpdateWindow(mhMainWnd);

	return true;
}

bool d3dApp::InitDirect3D() {

	#if defined(DEBUG) || defined(_DEBUG)
		{
			ComPtr<ID3D12Debug> debugController;
			ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
			debugController->EnableDebugLayer();
		}
	#endif

	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory)));

	HRESULT hardwareResult = D3D12CreateDevice(
		nullptr,
		D3D_FEATURE_LEVEL_12_1, //D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&md3dDevice));

	if (FAILED(hardwareResult))
	{
		ComPtr<IDXGIAdapter> pWarpAdapter;
		ThrowIfFailed(mdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));

		ThrowIfFailed(D3D12CreateDevice(
			pWarpAdapter.Get(),
			D3D_FEATURE_LEVEL_12_1, //D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&md3dDevice)));
	}

	ThrowIfFailed(md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));

	mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	msQualityLevels.Format = mBackBufferFormat;
	msQualityLevels.SampleCount = 4;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msQualityLevels.NumQualityLevels = 0;
	ThrowIfFailed(md3dDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&msQualityLevels,
		sizeof(msQualityLevels)));

	m4xMsaaQuality = msQualityLevels.NumQualityLevels;
	assert(m4xMsaaQuality > 0 && "Unexpected MSAA quality level.");

	#ifdef _DEBUG
	LogAdapters();
	#endif

	CreateCommandObjects();
	CreateSwapChain();
	CreateRtvAndDsvDescriptorHeaps();

	return true;
}

void d3dApp::CreateCommandObjects()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));

	ThrowIfFailed(md3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(mDirectCmdListAlloc.GetAddressOf())));

	ThrowIfFailed(md3dDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		mDirectCmdListAlloc.Get(),
		nullptr,
		IID_PPV_ARGS(mCommandList.GetAddressOf())));

	mCommandList->Close();
}

void d3dApp::CreateSwapChain()
{

	mSwapChain.Reset();

	DXGI_SWAP_CHAIN_DESC sd;
	sd.BufferDesc.Width = mClientWidth;
	sd.BufferDesc.Height = mClientHeight;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = mBackBufferFormat;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	sd.SampleDesc.Quality = m4xMsaaState ? m4xMsaaQuality - 1 : 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = SwapChainBufferCount;
	sd.OutputWindow = mhMainWnd;
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	ThrowIfFailed(mdxgiFactory->CreateSwapChain(
		mCommandQueue.Get(),
		&sd,
		mSwapChain.GetAddressOf()));
}

void d3dApp::CreateRtvAndDsvDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));
}

void d3dApp::LoadTextures()
{
	auto bricksTex = std::make_unique<Texture>();
	bricksTex->Name = "bricksTex";
	bricksTex->Filename = L"Textures/bricks3.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), bricksTex->Filename.c_str(),
		bricksTex->Resource, bricksTex->UploadHeap));

	auto checkboardTex = std::make_unique<Texture>();
	checkboardTex->Name = "checkboardTex";
	checkboardTex->Filename = L"Textures/checkboard.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), checkboardTex->Filename.c_str(),
		checkboardTex->Resource, checkboardTex->UploadHeap));

	auto iceTex = std::make_unique<Texture>();
	iceTex->Name = "iceTex";
	iceTex->Filename = L"Textures/ice.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), iceTex->Filename.c_str(),
		iceTex->Resource, iceTex->UploadHeap));

	auto white1x1Tex = std::make_unique<Texture>();
	white1x1Tex->Name = "white1x1Tex";
	white1x1Tex->Filename = L"Textures/white1x1.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), white1x1Tex->Filename.c_str(),
		white1x1Tex->Resource, white1x1Tex->UploadHeap));

	auto waterTex = std::make_unique<Texture>();
	waterTex->Name = "waterTex";
	waterTex->Filename = L"Textures/water1.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), waterTex->Filename.c_str(),
		waterTex->Resource, waterTex->UploadHeap));

	auto treeTex = std::make_unique<Texture>();
	treeTex->Name = "treeTex";
	treeTex->Filename = L"Textures/tree01S.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), treeTex->Filename.c_str(),
		treeTex->Resource, treeTex->UploadHeap));

	auto treeTex2 = std::make_unique<Texture>();
	treeTex2->Name = "treeTex2";
	treeTex2->Filename = L"Textures/tree02S.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), treeTex2->Filename.c_str(),
		treeTex2->Resource, treeTex2->UploadHeap));

	auto grassTex = std::make_unique<Texture>();
	grassTex->Name = "grassTex";
	grassTex->Filename = L"Textures/grass.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), grassTex->Filename.c_str(),
		grassTex->Resource, grassTex->UploadHeap));

	auto puddlesTex = std::make_unique<Texture>();
	puddlesTex->Name = "puddlesTex";
	puddlesTex->Filename = L"Textures/puddles.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), puddlesTex->Filename.c_str(),
		puddlesTex->Resource, puddlesTex->UploadHeap));

	mTextures[bricksTex->Name] = std::move(bricksTex);
	mTextures[checkboardTex->Name] = std::move(checkboardTex);
	mTextures[iceTex->Name] = std::move(iceTex);
	mTextures[white1x1Tex->Name] = std::move(white1x1Tex);
	mTextures[waterTex->Name] = std::move(waterTex);
	mTextures[treeTex->Name] = std::move(treeTex);
	mTextures[treeTex2->Name] = std::move(treeTex2);
	mTextures[grassTex->Name] = std::move(grassTex);
	mTextures[puddlesTex->Name] = std::move(puddlesTex);
}

void d3dApp::BuildDescriptorHeaps()
{
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 9;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	auto bricksTex = mTextures["bricksTex"]->Resource;
	auto checkboardTex = mTextures["checkboardTex"]->Resource;
	auto iceTex = mTextures["iceTex"]->Resource;
	auto white1x1Tex = mTextures["white1x1Tex"]->Resource;
	auto waterTex = mTextures["waterTex"]->Resource;
	auto treeTex = mTextures["treeTex"]->Resource;
	auto treeTex2 = mTextures["treeTex2"]->Resource;
	auto grassTex = mTextures["grassTex"]->Resource;
	auto puddlesTex = mTextures["puddlesTex"]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = bricksTex->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1;
	md3dDevice->CreateShaderResourceView(bricksTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = checkboardTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(checkboardTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = iceTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(iceTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = white1x1Tex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(white1x1Tex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = waterTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(waterTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = treeTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(treeTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = treeTex2->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(treeTex2.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = grassTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(grassTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = puddlesTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(puddlesTex.Get(), &srvDesc, hDescriptor);

}

void d3dApp::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[1].InitAsConstantBufferView(0);
	slotRootParameter[2].InitAsConstantBufferView(1);
	slotRootParameter[3].InitAsConstantBufferView(2);

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter, 
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void d3dApp::BuildShadersAndInputLayout()
{
	const D3D_SHADER_MACRO defines[] =
	{
		"FOG", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"FOG", "1",
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO wavyDefines[] =
	{
		"WAVY", "1",
		"FOG", "1",
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", defines, "PS", "ps_5_0");
	mShaders["alphaTestedPS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", alphaTestDefines, "PS", "ps_5_0");
	mShaders["wavyVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", wavyDefines, "VS", "vs_5_0");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void d3dApp::BuildRoomGeometry()
{
	GeometryGenerator geomGen;
	GeometryGenerator::MeshData plane = geomGen.CreateGrid(20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData plane2 = geomGen.CreateGrid(10.0f, 7.0f, 60, 40);
	GeometryGenerator::MeshData plane3 = geomGen.CreateGrid(10.0f, 7.0f, 20, 30);

	SubmeshGeometry wallSubmesh;
	wallSubmesh.IndexCount = (UINT)plane.GetIndices16().size();
	wallSubmesh.StartIndexLocation = (UINT)0;
	wallSubmesh.BaseVertexLocation = (UINT)0;

	SubmeshGeometry floorSubmesh;
	floorSubmesh.IndexCount = (UINT)plane.GetIndices16().size();
	floorSubmesh.StartIndexLocation = (UINT)plane.GetIndices16().size();
	floorSubmesh.BaseVertexLocation = (UINT)plane.Vertices.size();

	SubmeshGeometry doorSubmesh;
	doorSubmesh.IndexCount = (UINT)plane2.GetIndices16().size();
	doorSubmesh.StartIndexLocation = (UINT)(wallSubmesh.IndexCount + floorSubmesh.IndexCount);
	doorSubmesh.BaseVertexLocation = (UINT)(plane.Vertices.size() * 2);

	SubmeshGeometry mirrorSubmesh;
	mirrorSubmesh.IndexCount = (UINT)plane3.GetIndices16().size();
	mirrorSubmesh.StartIndexLocation = (UINT)(wallSubmesh.IndexCount + floorSubmesh.IndexCount + doorSubmesh.IndexCount);
	mirrorSubmesh.BaseVertexLocation = (UINT)(plane.Vertices.size() * 2 + plane2.Vertices.size());

	size_t k = 0;
	std::vector<Vertex> vertices(plane.Vertices.size() * 2 + plane2.Vertices.size() + plane3.Vertices.size());
	for (size_t i = 0; i < plane.Vertices.size(); ++i, ++k)
	{
		vertices[i].Pos = plane.Vertices[i].Position;
		vertices[i].Normal = plane.Vertices[i].Normal;
		vertices[i].TexC = plane.Vertices[i].TexC;
	}

	for (size_t i = 0; i < plane.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = plane.Vertices[i].Position;
		vertices[k].Normal = plane.Vertices[i].Normal;
		vertices[k].TexC = plane.Vertices[i].TexC;
	}

	for (size_t i = 0; i < plane2.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = plane2.Vertices[i].Position;
		vertices[k].Normal = plane2.Vertices[i].Normal;
		vertices[k].TexC = plane2.Vertices[i].TexC;
	}

	for (size_t i = 0; i < plane3.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = plane3.Vertices[i].Position;
		vertices[k].Normal = plane3.Vertices[i].Normal;
		vertices[k].TexC = plane3.Vertices[i].TexC;
	}

	std::vector<uint16_t> indices;
	indices.insert(indices.end(), std::begin(plane.GetIndices16()), std::end(plane.GetIndices16()));
	indices.insert(indices.end(), std::begin(plane.GetIndices16()), std::end(plane.GetIndices16()));
	indices.insert(indices.end(), std::begin(plane2.GetIndices16()), std::end(plane2.GetIndices16()));
	indices.insert(indices.end(), std::begin(plane3.GetIndices16()), std::end(plane3.GetIndices16()));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "roomGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["floor"] = floorSubmesh;
	geo->DrawArgs["wall"] = wallSubmesh;
	geo->DrawArgs["door"] = doorSubmesh;
	geo->DrawArgs["mirror"] = mirrorSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void d3dApp::BuildTreeGeometry()
{
	GeometryGenerator geomGen;
	GeometryGenerator::MeshData plane = geomGen.CreateGrid(15.0f, 20.0f, 20, 30);
	GeometryGenerator::MeshData plane2 = geomGen.CreateGrid(20.0f, 7.0f, 20, 30);

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)plane.GetIndices16().size();
	submesh.StartIndexLocation = (UINT)0;
	submesh.BaseVertexLocation = (UINT)0;

	SubmeshGeometry submesh2;
	submesh2.IndexCount = (UINT)plane2.GetIndices16().size();
	submesh2.StartIndexLocation = submesh.IndexCount;
	submesh2.BaseVertexLocation = (UINT)plane.Vertices.size();

	std::vector<Vertex> vertices(plane.Vertices.size() + plane2.Vertices.size());
	size_t k = 0;
	for (size_t i = 0; i < plane.Vertices.size(); ++i, ++k)
	{
		vertices[i].Pos = plane.Vertices[i].Position;
		vertices[i].Normal = plane.Vertices[i].Normal;
		vertices[i].TexC = plane.Vertices[i].TexC;
	}

	for (size_t i = 0; i < plane2.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = plane2.Vertices[i].Position;
		vertices[k].Normal = plane2.Vertices[i].Normal;
		vertices[k].TexC = plane2.Vertices[i].TexC;
	}

	std::vector<uint16_t> indices;
	indices.insert(indices.end(), std::begin(plane.GetIndices16()), std::end(plane.GetIndices16()));
	indices.insert(indices.end(), std::begin(plane2.GetIndices16()), std::end(plane2.GetIndices16()));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "treeGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["tree"] = submesh;
	geo->DrawArgs["bush"] = submesh2;

	mGeometries[geo->Name] = std::move(geo);
}

void d3dApp::BuildSkullGeometry()
{
	std::ifstream fin("Models/skull.txt");

	if (!fin)
	{
		MessageBox(0, L"Models/skull.txt not found.", 0, 0);
		return;
	}

	UINT vcount = 0;
	UINT tcount = 0;
	std::string ignore;

	fin >> ignore >> vcount;
	fin >> ignore >> tcount;
	fin >> ignore >> ignore >> ignore >> ignore;

	std::vector<Vertex> vertices(vcount);
	for (UINT i = 0; i < vcount; ++i)
	{
		fin >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
		fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;

		// Model does not have texture coordinates, so just zero them out.
		vertices[i].TexC = { 0.0f, 0.0f };
	}

	fin >> ignore;
	fin >> ignore;
	fin >> ignore;

	std::vector<std::int32_t> indices(3 * tcount);
	for (UINT i = 0; i < tcount; ++i)
	{
		fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
	}

	fin.close();

	//
	// Pack the indices of all the meshes into one index buffer.
	//

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::int32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "skullGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["skull"] = submesh;

	mGeometries[geo->Name] = std::move(geo);
}

void d3dApp::BuildCarGeometry()
{
	std::ifstream fin("Models/car.txt");

	if (!fin)
	{
		MessageBox(0, L"Models/car.txt not found.", 0, 0);
		return;
	}

	UINT vcount = 0;
	UINT tcount = 0;
	std::string ignore;

	fin >> ignore >> vcount;
	fin >> ignore >> tcount;
	fin >> ignore >> ignore >> ignore >> ignore;

	std::vector<Vertex> vertices(vcount);
	for (UINT i = 0; i < vcount; ++i)
	{
		fin >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
		fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;

		float u = i % 3 == 0 ? 0.0f : i % 2 == 0 ? 0.333f : 0.666f;
		float v = i % 3 == 0 ? 0.0f : 1.0f;

		// Model does not have texture coordinates, so just zero them out.
		vertices[i].TexC = { u, v };
	}

	fin >> ignore;
	fin >> ignore;
	fin >> ignore;

	std::vector<std::int32_t> indices(3 * tcount);
	for (UINT i = 0; i < tcount; ++i)
	{
		fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
	}

	fin.close();

	//
	// Pack the indices of all the meshes into one index buffer.
	//

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::int32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "carGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["car"] = submesh;

	mGeometries[geo->Name] = std::move(geo);
}

void d3dApp::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

	//
	// PSO for transparent objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;
	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.LogicOpEnable = false;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	transparentPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;

	D3D12_DEPTH_STENCIL_DESC doorDDS;
	doorDDS.DepthEnable = true;
	doorDDS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	doorDDS.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	doorDDS.StencilEnable = true;
	doorDDS.StencilReadMask = 0xff;
	doorDDS.StencilWriteMask = 0xff;

	doorDDS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	doorDDS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	doorDDS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	doorDDS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	doorDDS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	doorDDS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	doorDDS.BackFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	doorDDS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	transparentPsoDesc.DepthStencilState = doorDDS;

	//transparentPsoDesc.VS =
	//{
	//	reinterpret_cast<BYTE*>(mShaders["wavyVS"]->GetBufferPointer()),
	//	mShaders["wavyVS"]->GetBufferSize()
	//};

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&mPSOs["transparent"])));



	CD3DX12_BLEND_DESC carBlend(D3D12_DEFAULT);
	//carBlend.RenderTarget[0].RenderTargetWriteMask = 0;

	D3D12_DEPTH_STENCIL_DESC carDDS;
	carDDS.DepthEnable = false;
	carDDS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	carDDS.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
	carDDS.StencilEnable = true;
	carDDS.StencilReadMask = 0xff;
	carDDS.StencilWriteMask = 0xff;

	carDDS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	carDDS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	carDDS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	carDDS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	carDDS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	carDDS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	carDDS.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	carDDS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC carPsoDesc = opaquePsoDesc;
	carPsoDesc.DepthStencilState = carDDS;
	carPsoDesc.BlendState = carBlend;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&carPsoDesc, IID_PPV_ARGS(&mPSOs["car"])));



	//
	// PSO for marking stencil wall and floor.
	//

	CD3DX12_BLEND_DESC wallFloorBlendState(D3D12_DEFAULT);
	wallFloorBlendState.RenderTarget[0].RenderTargetWriteMask = 0;

	D3D12_DEPTH_STENCIL_DESC wallFloorDSS;
	wallFloorDSS.DepthEnable = false;
	wallFloorDSS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	wallFloorDSS.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
	wallFloorDSS.StencilEnable = true;
	wallFloorDSS.StencilReadMask = 0xff;
	wallFloorDSS.StencilWriteMask = 0xff;

	wallFloorDSS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	wallFloorDSS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	wallFloorDSS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	wallFloorDSS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	wallFloorDSS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	wallFloorDSS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	wallFloorDSS.BackFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	wallFloorDSS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC markFloorWallPsoDesc = opaquePsoDesc;
	markFloorWallPsoDesc.DepthStencilState = wallFloorDSS;
	markFloorWallPsoDesc.BlendState = wallFloorBlendState;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&markFloorWallPsoDesc, IID_PPV_ARGS(&mPSOs["markStencilWallAndFloor"])));


	//
	// PSO for shadow objects
	//

	// We are going to draw shadows with transparency, so base it off the transparency description.
	D3D12_DEPTH_STENCIL_DESC shadowDSS;
	shadowDSS.DepthEnable = true;
	shadowDSS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	shadowDSS.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

	shadowDSS.StencilEnable = true;
	shadowDSS.StencilReadMask = 0xff;
	shadowDSS.StencilWriteMask = 0xff;

	shadowDSS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_DECR;
	shadowDSS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	shadowDSS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.BackFace.StencilPassOp = D3D12_STENCIL_OP_DECR;
	shadowDSS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPsoDesc = transparentPsoDesc;
	shadowPsoDesc.DepthStencilState = shadowDSS;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&shadowPsoDesc, IID_PPV_ARGS(&mPSOs["shadow"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC treePsoDesc = opaquePsoDesc;
	D3D12_RENDER_TARGET_BLEND_DESC treeBlendDesc;
	treeBlendDesc.BlendEnable = true;
	treeBlendDesc.LogicOpEnable = false;
	treeBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	treeBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	treeBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	treeBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	treeBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	treeBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	treeBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	treeBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	treePsoDesc.BlendState.RenderTarget[0] = treeBlendDesc;
	treePsoDesc.BlendState.AlphaToCoverageEnable = true;

	D3D12_DEPTH_STENCIL_DESC treeDDS;
	treeDDS.DepthEnable = true;
	treeDDS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	treeDDS.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	treeDDS.StencilEnable = true;
	treeDDS.StencilReadMask = 0xff;
	treeDDS.StencilWriteMask = 0xff;
	treeDDS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	treeDDS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	treeDDS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	treeDDS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	treeDDS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	treeDDS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	treeDDS.BackFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	treeDDS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	treePsoDesc.DepthStencilState = treeDDS;
	treePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["wavyVS"]->GetBufferPointer()),
		mShaders["wavyVS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&treePsoDesc, IID_PPV_ARGS(&mPSOs["trees"])));

	D3D12_DEPTH_STENCIL_DESC greenSkullDSS;
	greenSkullDSS.DepthEnable = true;
	greenSkullDSS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	greenSkullDSS.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

	greenSkullDSS.StencilEnable = true;
	greenSkullDSS.StencilReadMask = 0xff;
	greenSkullDSS.StencilWriteMask = 0xff;

	greenSkullDSS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	greenSkullDSS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	greenSkullDSS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
	greenSkullDSS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	greenSkullDSS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	greenSkullDSS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	greenSkullDSS.BackFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
	greenSkullDSS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC treeskullPsoDesc = opaquePsoDesc;
	treeskullPsoDesc.DepthStencilState = greenSkullDSS;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&treeskullPsoDesc, IID_PPV_ARGS(&mPSOs["treeskull"])));


	D3D12_DEPTH_STENCIL_DESC puddleDSS;
	puddleDSS.DepthEnable = true;
	puddleDSS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	puddleDSS.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

	puddleDSS.StencilEnable = true;
	puddleDSS.StencilReadMask = 0xff;
	puddleDSS.StencilWriteMask = 0xff;

	puddleDSS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	puddleDSS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	puddleDSS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	puddleDSS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	puddleDSS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	puddleDSS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	puddleDSS.BackFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	puddleDSS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC puddlePsoDesc = transparentPsoDesc;
	puddlePsoDesc.DepthStencilState = puddleDSS;
	puddlePsoDesc.BlendState.AlphaToCoverageEnable = true;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&puddlePsoDesc, IID_PPV_ARGS(&mPSOs["puddle"])));


	D3D12_GRAPHICS_PIPELINE_STATE_DESC reflectedPsoDesc = carPsoDesc;
	D3D12_DEPTH_STENCIL_DESC reflectedDDS;
	reflectedDDS.DepthEnable = false;
	reflectedDDS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	reflectedDDS.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;

	reflectedDDS.StencilEnable = true;
	reflectedDDS.StencilReadMask = 0xff;
	reflectedDDS.StencilWriteMask = 0xff;

	reflectedDDS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	reflectedDDS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	reflectedDDS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
	reflectedDDS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	reflectedDDS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	reflectedDDS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	reflectedDDS.BackFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
	reflectedDDS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	reflectedPsoDesc.DepthStencilState = reflectedDDS;

	reflectedPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	reflectedPsoDesc.RasterizerState.FrontCounterClockwise = true;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&reflectedPsoDesc, IID_PPV_ARGS(&mPSOs["reflected"])));



	//
	// PSO for opaque wireframe objects.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
	opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));
}

void d3dApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			2, (UINT)mAllRitems.size(), (UINT)mMaterials.size()));
	}
}

void d3dApp::BuildMaterials()
{
	auto bricks = std::make_unique<Material>();
	bricks->Name = "bricks";
	bricks->MatCBIndex = 0;
	bricks->DiffuseSrvHeapIndex = 0;
	bricks->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	bricks->Roughness = 0.25f;

	auto checkertile = std::make_unique<Material>();
	checkertile->Name = "checkertile";
	checkertile->MatCBIndex = 1;
	checkertile->DiffuseSrvHeapIndex = 1;
	checkertile->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	checkertile->FresnelR0 = XMFLOAT3(0.07f, 0.07f, 0.07f);
	checkertile->Roughness = 0.3f;

	auto icemirror = std::make_unique<Material>();
	icemirror->Name = "icemirror";
	icemirror->MatCBIndex = 2;
	icemirror->DiffuseSrvHeapIndex = 2;
	icemirror->DiffuseAlbedo = XMFLOAT4(0.9f, 0.5f, 0.3f, 0.3f);
	icemirror->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	icemirror->Roughness = 0.5f;

	auto skullMat = std::make_unique<Material>();
	skullMat->Name = "skullMat";
	skullMat->MatCBIndex = 3;
	skullMat->DiffuseSrvHeapIndex = 3;
	skullMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	skullMat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	skullMat->Roughness = 0.3f;

	auto shadowMat = std::make_unique<Material>();
	shadowMat->Name = "shadowMat";
	shadowMat->MatCBIndex = 4;
	shadowMat->DiffuseSrvHeapIndex = 3;
	shadowMat->DiffuseAlbedo = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.5f);
	shadowMat->FresnelR0 = XMFLOAT3(0.001f, 0.001f, 0.001f);
	shadowMat->Roughness = 0.0f;

	auto waterMat = std::make_unique<Material>();
	waterMat->Name = "waterMat";
	waterMat->MatCBIndex = 5;
	waterMat->DiffuseSrvHeapIndex = 4;
	waterMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	waterMat->FresnelR0 = XMFLOAT3(1.333f, 1.333f, 1.333f); 
	waterMat->Roughness = 0.0f;

	auto icemirror2 = std::make_unique<Material>();
	icemirror2->Name = "icemirror2";
	icemirror2->MatCBIndex = 6;
	icemirror2->DiffuseSrvHeapIndex = 2;
	icemirror2->DiffuseAlbedo = XMFLOAT4(0.5f, 0.9f, 0.3f, 0.3f);
	icemirror2->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	icemirror2->Roughness = 0.5f;

	auto icemirror3 = std::make_unique<Material>();
	icemirror3->Name = "icemirror3";
	icemirror3->MatCBIndex = 7;
	icemirror3->DiffuseSrvHeapIndex = 2;
	icemirror3->DiffuseAlbedo = XMFLOAT4(0.5f, 0.3f, 0.9f, 0.3f);
	icemirror3->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	icemirror3->Roughness = 0.5f;

	auto treeMat1 = std::make_unique<Material>();
	treeMat1->Name = "treeMat1";
	treeMat1->MatCBIndex = 8;
	treeMat1->DiffuseSrvHeapIndex = 5;
	treeMat1->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	treeMat1->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	treeMat1->Roughness = 0.5f;

	auto treeMat2 = std::make_unique<Material>();
	treeMat2->Name = "treeMat2";
	treeMat2->MatCBIndex = 9;
	treeMat2->DiffuseSrvHeapIndex = 6;
	treeMat2->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	treeMat2->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	treeMat2->Roughness = 0.5f;

	auto grassMat = std::make_unique<Material>();
	grassMat->Name = "grassMat";
	grassMat->MatCBIndex = 10;
	grassMat->DiffuseSrvHeapIndex = 7;
	grassMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	grassMat->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	grassMat->Roughness = 0.5f;

	auto greenSkullMat = std::make_unique<Material>();
	greenSkullMat->Name = "greenSkullMat";
	greenSkullMat->MatCBIndex = 11;
	greenSkullMat->DiffuseSrvHeapIndex = 3;
	greenSkullMat->DiffuseAlbedo = XMFLOAT4(0.4f, 0.6f, 0.4f, 1.0f);
	greenSkullMat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	greenSkullMat->Roughness = 0.3f;

	auto puddleMat = std::make_unique<Material>();
	puddleMat->Name = "puddleMat";
	puddleMat->MatCBIndex = 12;
	puddleMat->DiffuseSrvHeapIndex = 8;
	puddleMat->DiffuseAlbedo = XMFLOAT4(0.2f, 0.5f, 0.7f, 0.4f);
	puddleMat->FresnelR0 = XMFLOAT3(2.333f, 2.333f, 2.333f);
	puddleMat->Roughness = 0.1f;

	mMaterials["bricks"] = std::move(bricks);
	mMaterials["checkertile"] = std::move(checkertile);
	mMaterials["icemirror"] = std::move(icemirror);
	mMaterials["icemirror2"] = std::move(icemirror2);
	mMaterials["icemirror3"] = std::move(icemirror3);
	mMaterials["skullMat"] = std::move(skullMat);
	mMaterials["shadowMat"] = std::move(shadowMat);
	mMaterials["waterMat"] = std::move(waterMat);
	mMaterials["treeMat1"] = std::move(treeMat1);
	mMaterials["treeMat2"] = std::move(treeMat2);
	mMaterials["grassMat"] = std::move(grassMat);
	mMaterials["greenSkullMat"] = std::move(greenSkullMat);
	mMaterials["puddleMat"] = std::move(puddleMat);
}

void d3dApp::BuildRenderItems()
{
	auto floorRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&floorRitem->World, XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixRotationY(-MathHelper::Pi / 2.0f) * XMMatrixTranslation(0.0f, 0.0f, -10.0f));
	floorRitem->TexTransform = MathHelper::Identity4x4();
	floorRitem->ObjCBIndex = 0;
	floorRitem->Mat = mMaterials["checkertile"].get();
	floorRitem->Geo = mGeometries["roomGeo"].get();
	floorRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	floorRitem->IndexCount = floorRitem->Geo->DrawArgs["floor"].IndexCount;
	floorRitem->StartIndexLocation = floorRitem->Geo->DrawArgs["floor"].StartIndexLocation;
	floorRitem->BaseVertexLocation = floorRitem->Geo->DrawArgs["floor"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(floorRitem.get());

	auto wallsRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&wallsRitem->World, XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixRotationX(-MathHelper::Pi / 2.0f) * XMMatrixRotationZ(-MathHelper::Pi / 2.0f) * XMMatrixTranslation(0.0f, 10.0f, 0.0f));
	wallsRitem->TexTransform = MathHelper::Identity4x4();
	wallsRitem->ObjCBIndex = 1;
	wallsRitem->Mat = mMaterials["bricks"].get();
	wallsRitem->Geo = mGeometries["roomGeo"].get();
	wallsRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wallsRitem->IndexCount = wallsRitem->Geo->DrawArgs["wall"].IndexCount;
	wallsRitem->StartIndexLocation = wallsRitem->Geo->DrawArgs["wall"].StartIndexLocation;
	wallsRitem->BaseVertexLocation = wallsRitem->Geo->DrawArgs["wall"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(wallsRitem.get());

	auto skullRitem = std::make_unique<RenderItem>();
	skullRitem->World = MathHelper::Identity4x4();
	skullRitem->TexTransform = MathHelper::Identity4x4();
	skullRitem->ObjCBIndex = 2;
	skullRitem->Mat = mMaterials["skullMat"].get();
	skullRitem->Geo = mGeometries["skullGeo"].get();
	skullRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skullRitem->IndexCount = skullRitem->Geo->DrawArgs["skull"].IndexCount;
	skullRitem->StartIndexLocation = skullRitem->Geo->DrawArgs["skull"].StartIndexLocation;
	skullRitem->BaseVertexLocation = skullRitem->Geo->DrawArgs["skull"].BaseVertexLocation;
	mSkullRitem = skullRitem.get();
	mRitemLayer[(int)RenderLayer::Skull].push_back(skullRitem.get());

	// Shadowed skull will have different world matrix, so it needs to be its own render item.
	auto shadowedSkullRitemWall = std::make_unique<RenderItem>();
	*shadowedSkullRitemWall = *skullRitem;
	shadowedSkullRitemWall->ObjCBIndex = 3;
	shadowedSkullRitemWall->Mat = mMaterials["shadowMat"].get();
	mShadowedSkullRitemWall = shadowedSkullRitemWall.get();
	mRitemLayer[(int)RenderLayer::Shadow].push_back(shadowedSkullRitemWall.get());

	// Shadowed skull will have different world matrix, so it needs to be its own render item.
	auto shadowedSkullRitemFloor = std::make_unique<RenderItem>();
	*shadowedSkullRitemFloor = *skullRitem;
	shadowedSkullRitemFloor->ObjCBIndex = 4;
	shadowedSkullRitemFloor->Mat = mMaterials["shadowMat"].get();
	mShadowedSkullRitemFloor = shadowedSkullRitemFloor.get();
	mRitemLayer[(int)RenderLayer::Shadow].push_back(shadowedSkullRitemFloor.get());

	auto doorRitemCenter = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&doorRitemCenter->World, XMMatrixScaling(1.2f, 1.2f, 1.2f) * XMMatrixRotationX(MathHelper::Pi / 2) * XMMatrixRotationZ(MathHelper::Pi / 2) * XMMatrixTranslation(0.0f, 7.0f, -22.0f));
	doorRitemCenter->TexTransform = MathHelper::Identity4x4();
	doorRitemCenter->ObjCBIndex = 5;
	doorRitemCenter->Mat = mMaterials["icemirror"].get();
	doorRitemCenter->Geo = mGeometries["roomGeo"].get();
	doorRitemCenter->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	doorRitemCenter->IndexCount = doorRitemCenter->Geo->DrawArgs["door"].IndexCount;
	doorRitemCenter->StartIndexLocation = doorRitemCenter->Geo->DrawArgs["door"].StartIndexLocation;
	doorRitemCenter->BaseVertexLocation = doorRitemCenter->Geo->DrawArgs["door"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Transparent].push_back(doorRitemCenter.get());

	auto doorRitemLeft = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&doorRitemLeft->World, XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixRotationX(MathHelper::Pi / 2) * XMMatrixRotationZ(MathHelper::Pi / 2) * XMMatrixTranslation(9.0f, 6.0f, -23.0f));
	doorRitemLeft->TexTransform = MathHelper::Identity4x4();
	doorRitemLeft->ObjCBIndex = 6;
	doorRitemLeft->Mat = mMaterials["icemirror2"].get();
	doorRitemLeft->Geo = mGeometries["roomGeo"].get();
	doorRitemLeft->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	doorRitemLeft->IndexCount = doorRitemLeft->Geo->DrawArgs["door"].IndexCount;
	doorRitemLeft->StartIndexLocation = doorRitemLeft->Geo->DrawArgs["door"].StartIndexLocation;
	doorRitemLeft->BaseVertexLocation = doorRitemLeft->Geo->DrawArgs["door"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Transparent].push_back(doorRitemLeft.get());

	auto doorRitemRight = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&doorRitemRight->World, XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixRotationX(MathHelper::Pi / 2) * XMMatrixRotationZ(MathHelper::Pi / 2) * XMMatrixTranslation(-9.0f, 6.5f, -23.0f));
	doorRitemRight->TexTransform = MathHelper::Identity4x4();
	doorRitemRight->ObjCBIndex = 7;
	doorRitemRight->Mat = mMaterials["icemirror3"].get();
	doorRitemRight->Geo = mGeometries["roomGeo"].get();
	doorRitemRight->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	doorRitemRight->IndexCount = doorRitemRight->Geo->DrawArgs["door"].IndexCount;
	doorRitemRight->StartIndexLocation = doorRitemRight->Geo->DrawArgs["door"].StartIndexLocation;
	doorRitemRight->BaseVertexLocation = doorRitemRight->Geo->DrawArgs["door"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Transparent].push_back(doorRitemRight.get());

	auto carRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&carRitem->World, XMMatrixScaling(1.5f, 1.5f, 1.5f) * XMMatrixRotationY(MathHelper::Pi / 2) * XMMatrixTranslation(0.0f, 7.0f, -27.0f));
	carRitem->TexTransform = MathHelper::Identity4x4();
	carRitem->ObjCBIndex = 8;
	carRitem->Mat = mMaterials["waterMat"].get();
	carRitem->Geo = mGeometries["carGeo"].get();
	carRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	carRitem->IndexCount = carRitem->Geo->DrawArgs["car"].IndexCount;
	carRitem->StartIndexLocation = carRitem->Geo->DrawArgs["car"].StartIndexLocation;
	carRitem->BaseVertexLocation = carRitem->Geo->DrawArgs["car"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Car].push_back(carRitem.get());

	auto treeRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&treeRitem->World, XMMatrixRotationX(-MathHelper::Pi / 2) * XMMatrixRotationY(-MathHelper::Pi / 2) * XMMatrixTranslation(-22.0f, 10.0f, -10.0f));
	treeRitem->TexTransform = MathHelper::Identity4x4();
	treeRitem->ObjCBIndex = 9;
	treeRitem->Mat = mMaterials["treeMat1"].get();
	treeRitem->Geo = mGeometries["treeGeo"].get();
	treeRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	treeRitem->IndexCount = treeRitem->Geo->DrawArgs["tree"].IndexCount;
	treeRitem->StartIndexLocation = treeRitem->Geo->DrawArgs["tree"].StartIndexLocation;
	treeRitem->BaseVertexLocation = treeRitem->Geo->DrawArgs["tree"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Trees].push_back(treeRitem.get());

	auto treeRitem1 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&treeRitem1->World, XMMatrixRotationX(-MathHelper::Pi / 2)* XMMatrixRotationY(-MathHelper::Pi / 2)* XMMatrixTranslation(-25.0f, 10.0f, -15.0f));
	treeRitem1->TexTransform = MathHelper::Identity4x4();
	treeRitem1->ObjCBIndex = 10;
	treeRitem1->Mat = mMaterials["treeMat1"].get();
	treeRitem1->Geo = mGeometries["treeGeo"].get();
	treeRitem1->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	treeRitem1->IndexCount = treeRitem1->Geo->DrawArgs["tree"].IndexCount;
	treeRitem1->StartIndexLocation = treeRitem1->Geo->DrawArgs["tree"].StartIndexLocation;
	treeRitem1->BaseVertexLocation = treeRitem1->Geo->DrawArgs["tree"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Trees].push_back(treeRitem1.get());

	auto treeRitem2 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&treeRitem2->World, XMMatrixRotationX(-MathHelper::Pi / 2)* XMMatrixRotationY(-MathHelper::Pi / 2)* XMMatrixTranslation(-22.0f, 10.0f, -5.0f));
	treeRitem2->TexTransform = MathHelper::Identity4x4();
	treeRitem2->ObjCBIndex = 11;
	treeRitem2->Mat = mMaterials["treeMat1"].get();
	treeRitem2->Geo = mGeometries["treeGeo"].get();
	treeRitem2->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	treeRitem2->IndexCount = treeRitem2->Geo->DrawArgs["tree"].IndexCount;
	treeRitem2->StartIndexLocation = treeRitem2->Geo->DrawArgs["tree"].StartIndexLocation;
	treeRitem2->BaseVertexLocation = treeRitem2->Geo->DrawArgs["tree"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Trees].push_back(treeRitem2.get());

	auto bushRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&bushRitem->World, XMMatrixRotationX(-MathHelper::Pi / 2)* XMMatrixRotationY(-MathHelper::Pi / 2)* XMMatrixTranslation(-22.0f, 1.0f, -10.0f));
	bushRitem->TexTransform = MathHelper::Identity4x4();
	bushRitem->ObjCBIndex = 12;
	bushRitem->Mat = mMaterials["treeMat2"].get();
	bushRitem->Geo = mGeometries["treeGeo"].get();
	bushRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	bushRitem->IndexCount = bushRitem->Geo->DrawArgs["bush"].IndexCount;
	bushRitem->StartIndexLocation = bushRitem->Geo->DrawArgs["bush"].StartIndexLocation;
	bushRitem->BaseVertexLocation = bushRitem->Geo->DrawArgs["bush"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Trees].push_back(bushRitem.get());

	auto grassRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&grassRitem->World, XMMatrixScaling(1.0f, 1.0f, 1.0f)* XMMatrixRotationY(-MathHelper::Pi / 2.0f)* XMMatrixTranslation(-30.0f, 0.0f, -10.0f));
	grassRitem->TexTransform = MathHelper::Identity4x4();
	grassRitem->ObjCBIndex = 13;
	grassRitem->Mat = mMaterials["grassMat"].get();
	grassRitem->Geo = mGeometries["roomGeo"].get();
	grassRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	grassRitem->IndexCount = grassRitem->Geo->DrawArgs["floor"].IndexCount;
	grassRitem->StartIndexLocation = grassRitem->Geo->DrawArgs["floor"].StartIndexLocation;
	grassRitem->BaseVertexLocation = grassRitem->Geo->DrawArgs["floor"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Trees].push_back(grassRitem.get());


	auto grassSkull = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&grassSkull->World, XMMatrixScaling(0.55f, 0.55f, 0.55f) * XMMatrixRotationY(-MathHelper::Pi) * XMMatrixTranslation(-18.0f, 2.0f, -10.0f));
	grassSkull->TexTransform = MathHelper::Identity4x4();
	grassSkull->ObjCBIndex = 14;
	grassSkull->Mat = mMaterials["greenSkullMat"].get();
	grassSkull->Geo = mGeometries["skullGeo"].get();
	grassSkull->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	grassSkull->IndexCount = grassSkull->Geo->DrawArgs["skull"].IndexCount;
	grassSkull->StartIndexLocation = grassSkull->Geo->DrawArgs["skull"].StartIndexLocation;
	grassSkull->BaseVertexLocation = grassSkull->Geo->DrawArgs["skull"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::TreeSkull].push_back(grassSkull.get());

	auto treeRitem3 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&treeRitem3->World, XMMatrixRotationX(-MathHelper::Pi / 2)* XMMatrixRotationY(-MathHelper::Pi / 2)* XMMatrixTranslation(-28.0f, 10.0f, -13.0f));
	treeRitem3->TexTransform = MathHelper::Identity4x4();
	treeRitem3->ObjCBIndex = 15;
	treeRitem3->Mat = mMaterials["treeMat1"].get();
	treeRitem3->Geo = mGeometries["treeGeo"].get();
	treeRitem3->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	treeRitem3->IndexCount = treeRitem3->Geo->DrawArgs["tree"].IndexCount;
	treeRitem3->StartIndexLocation = treeRitem3->Geo->DrawArgs["tree"].StartIndexLocation;
	treeRitem3->BaseVertexLocation = treeRitem3->Geo->DrawArgs["tree"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Trees].push_back(treeRitem3.get());

	auto treeRitem4 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&treeRitem4->World, XMMatrixRotationX(-MathHelper::Pi / 2)* XMMatrixRotationY(-MathHelper::Pi / 2)* XMMatrixTranslation(-28.0f, 10.0f, -7.0f));
	treeRitem4->TexTransform = MathHelper::Identity4x4();
	treeRitem4->ObjCBIndex = 16;
	treeRitem4->Mat = mMaterials["treeMat1"].get();
	treeRitem4->Geo = mGeometries["treeGeo"].get();
	treeRitem4->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	treeRitem4->IndexCount = treeRitem4->Geo->DrawArgs["tree"].IndexCount;
	treeRitem4->StartIndexLocation = treeRitem4->Geo->DrawArgs["tree"].StartIndexLocation;
	treeRitem4->BaseVertexLocation = treeRitem4->Geo->DrawArgs["tree"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Trees].push_back(treeRitem4.get());

	auto puddleRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&puddleRitem->World, XMMatrixScaling(1.0f, 1.0f, 1.0f)* XMMatrixRotationY(-MathHelper::Pi / 2.0f)* XMMatrixTranslation(0.0f, 0.1f, -10.0f));
	puddleRitem->TexTransform = MathHelper::Identity4x4();
	puddleRitem->ObjCBIndex = 17;
	puddleRitem->Mat = mMaterials["puddleMat"].get();
	puddleRitem->Geo = mGeometries["roomGeo"].get();
	puddleRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	puddleRitem->IndexCount = puddleRitem->Geo->DrawArgs["floor"].IndexCount;
	puddleRitem->StartIndexLocation = puddleRitem->Geo->DrawArgs["floor"].StartIndexLocation;
	puddleRitem->BaseVertexLocation = puddleRitem->Geo->DrawArgs["floor"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Puddle].push_back(puddleRitem.get());

	// Reflected skull will have different world matrix, so it needs to be its own render item.
	auto reflectedSkullRitem = std::make_unique<RenderItem>();
	*reflectedSkullRitem = *skullRitem;
	reflectedSkullRitem->ObjCBIndex = 18;
	mReflectedSkullRitem = reflectedSkullRitem.get();
	mRitemLayer[(int)RenderLayer::Reflected].push_back(reflectedSkullRitem.get());
	//mRitemLayer[(int)RenderLayer::Opaque].push_back(reflectedSkullRitem.get());



	mAllRitems.push_back(std::move(floorRitem));
	mAllRitems.push_back(std::move(wallsRitem));
	mAllRitems.push_back(std::move(skullRitem));
	mAllRitems.push_back(std::move(shadowedSkullRitemWall));
	mAllRitems.push_back(std::move(shadowedSkullRitemFloor));
	mAllRitems.push_back(std::move(doorRitemCenter));
	mAllRitems.push_back(std::move(doorRitemLeft));
	mAllRitems.push_back(std::move(doorRitemRight));
	mAllRitems.push_back(std::move(carRitem));
	mAllRitems.push_back(std::move(treeRitem));
	mAllRitems.push_back(std::move(treeRitem1));
	mAllRitems.push_back(std::move(treeRitem2));
	mAllRitems.push_back(std::move(bushRitem));
	mAllRitems.push_back(std::move(grassRitem));
	mAllRitems.push_back(std::move(grassSkull));
	mAllRitems.push_back(std::move(treeRitem3));
	mAllRitems.push_back(std::move(treeRitem4));
	mAllRitems.push_back(std::move(puddleRitem));
	mAllRitems.push_back(std::move(reflectedSkullRitem));

}

void d3dApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

	CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	cmdList->SetGraphicsRootDescriptorTable(0, tex);

	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		D3D12_VERTEX_BUFFER_VIEW vbv = ri->Geo->VertexBufferView();
		cmdList->IASetVertexBuffers(0, 1, &vbv);
		D3D12_INDEX_BUFFER_VIEW ibv = ri->Geo->IndexBufferView();
		cmdList->IASetIndexBuffer(&ibv);
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;

		cmdList->SetGraphicsRootDescriptorTable(0, tex);
		cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
		cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> d3dApp::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}

void d3dApp::OnResize()
{
	assert(md3dDevice);
	assert(mSwapChain);
	assert(mDirectCmdListAlloc);

	FlushCommandQueue();

	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	for (int i = 0; i < SwapChainBufferCount; ++i) {
		mSwapChainBuffer[i].Reset();
	}
	mDepthStencilBuffer.Reset();

	ThrowIfFailed(mSwapChain->ResizeBuffers(
		SwapChainBufferCount,
		mClientWidth, mClientHeight,
		mBackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
	));

	mCurrBackBuffer = 0;

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < SwapChainBufferCount; i++)
	{
		ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i])));
		md3dDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
		rtvHeapHandle.Offset(1, mRtvDescriptorSize);
	}

	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = mClientWidth;
	depthStencilDesc.Height = mClientHeight;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;

	depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;

	depthStencilDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	depthStencilDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;

	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = mDepthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;

	CD3DX12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())));

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = mDepthStencilFormat;
	dsvDesc.Texture2D.MipSlice = 0;
	md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &dsvDesc, DepthStencilView());

	CD3DX12_RESOURCE_BARRIER resBarr = CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	mCommandList->ResourceBarrier(1, &resBarr);

	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	FlushCommandQueue();

	mScreenViewport.TopLeftX = 0;
	mScreenViewport.TopLeftY = 0;
	mScreenViewport.Width = static_cast<float>(mClientWidth);
	mScreenViewport.Height = static_cast<float>(mClientHeight);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;

	mScissorRect = { 0, 0, mClientWidth, mClientHeight };

	// The window resized, so update the aspect ratio and recompute the projection matrix.
	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);
}

int d3dApp::Run()
{
	MSG msg = { 0 };

	mTimer.Reset();

	while(msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			mTimer.Tick();

			if (!mAppPaused) {
				CalculateFrameStats();
				Update(mTimer);
				Draw(mTimer);
			}
			else {
				Sleep(100);
			}
		}
	}

	return (int)msg.wParam;
}

void d3dApp::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);
	UpdateCamera(gt);

	// Cycle through the circular frame resource array.
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
	UpdateReflectedPassCB(gt);
}

void d3dApp::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	if (mIsWireframe)
	{
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque_wireframe"].Get()));
	}
	else
	{
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));
	}

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Indicate a state transition on the resource usage.
	CD3DX12_RESOURCE_BARRIER resBarr = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mCommandList->ResourceBarrier(1, &resBarr);

	// Clear the back buffer and depth buffer.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), (float*)&mMainPassCB.FogColor, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	D3D12_CPU_DESCRIPTOR_HANDLE backBuff = CurrentBackBufferView();
	D3D12_CPU_DESCRIPTOR_HANDLE dsvView = DepthStencilView();
	mCommandList->OMSetRenderTargets(1, &backBuff, true, &dsvView);

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

	// Draw opaque items--floors, walls, skull.
	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

	mCommandList->OMSetStencilRef(0);
	mCommandList->SetPipelineState(mPSOs["opaque"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Skull]);

	// Restore main pass constants and stencil ref.
	//mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

	mCommandList->OMSetStencilRef(1);
	mCommandList->SetPipelineState(mPSOs["markStencilWallAndFloor"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	//mCommandList->OMSetStencilRef(0);
	// Draw shadows
	mCommandList->SetPipelineState(mPSOs["shadow"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Shadow]);

	mCommandList->OMSetStencilRef(2);
	// Draw mirror with transparency so reflection blends through.
	mCommandList->SetPipelineState(mPSOs["transparent"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Transparent]);

	// Draw mirror with transparency so reflection blends through.
	mCommandList->SetPipelineState(mPSOs["car"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Car]);


	mCommandList->OMSetStencilRef(3);
	// Draw mirror with transparency so reflection blends through.
	mCommandList->SetPipelineState(mPSOs["trees"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Trees]);

	// Draw mirror with transparency so reflection blends through.
	mCommandList->SetPipelineState(mPSOs["treeskull"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::TreeSkull]);


	mCommandList->OMSetStencilRef(5);
	mCommandList->SetPipelineState(mPSOs["puddle"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Puddle]);

	mCommandList->SetPipelineState(mPSOs["reflected"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Reflected]);
	

	// Indicate a state transition on the resource usage.
	resBarr = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	mCommandList->ResourceBarrier(1, &resBarr);

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

// Swap the back and front buffers
ThrowIfFailed(mSwapChain->Present(0, 0));
mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

// Advance the fence value to mark commands up to this fence point.
mCurrFrameResource->Fence = ++mCurrentFence;

// Add an instruction to the command queue to set a new fence point. 
// Because we are on the GPU timeline, the new fence point won't be 
// set until the GPU finishes processing all the commands prior to this Signal().
mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void d3dApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void d3dApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void d3dApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		// Update angles based on input to orbit camera around box.
		mTheta += dx;
		mPhi += dy;

		// Restrict the angle mPhi.
		mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);

	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		// Make each pixel correspond to 0.2 unit in the scene.
		float dx = 0.05f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.05f * static_cast<float>(y - mLastMousePos.y);

		// Update the camera radius based on input.
		mRadius += dx - dy;

		// Restrict the radius.
		mRadius = MathHelper::Clamp(mRadius, 1.0f, 150.0f);

	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void d3dApp::OnKeyboardInput(const GameTimer& gt)
{
	//
	// Allow user to move skull.
	//

	const float dt = gt.DeltaTime();

	//::OutputDebugString(std::to_wstring(GetAsyncKeyState('S') & 0x8000).c_str());

	if (GetAsyncKeyState('A') & 0x8000)
		mSkullTranslation.x -= 1.0f * dt;

	if (GetAsyncKeyState('D') & 0x8000)
		mSkullTranslation.x += 1.0f * dt;

	if (GetAsyncKeyState('W') & 0x8000)
		mSkullTranslation.y += 1.0f * dt;

	if (GetAsyncKeyState('S') & 0x8000)
		mSkullTranslation.y -= 1.0f * dt;

	// Don't let user move below ground plane.
	mSkullTranslation.y = MathHelper::Max(mSkullTranslation.y, 0.0f);

	// Update the new world matrix.
	XMMATRIX skullRotate = XMMatrixRotationY(0.5f * MathHelper::Pi);
	XMMATRIX skullScale = XMMatrixScaling(0.45f, 0.45f, 0.45f);
	XMMATRIX skullOffset = XMMatrixTranslation(mSkullTranslation.x, mSkullTranslation.y, mSkullTranslation.z);
	XMMATRIX skullWorld = skullRotate * skullScale * skullOffset;
	XMStoreFloat4x4(&mSkullRitem->World, skullWorld);


	// Update reflection world matrix.
	XMVECTOR mirrorPlane = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); // xz plane
	XMMATRIX R = XMMatrixReflect(mirrorPlane);
	XMStoreFloat4x4(&mReflectedSkullRitem->World, skullWorld * R);

	// Update shadow world matrix.
	XMVECTOR shadowPlane = XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f); // xy plane
	XMVECTOR toMainLight = -XMLoadFloat3(&mMainPassCB.Lights[0].Direction);
	XMMATRIX S = XMMatrixShadow(shadowPlane, toMainLight);
	XMMATRIX shadowOffsetZ = XMMatrixTranslation(0.0f, 0.0f, -0.01f);
	XMStoreFloat4x4(&mShadowedSkullRitemWall->World, skullWorld * S * shadowOffsetZ);

	// Update shadow world matrix.
	shadowPlane = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); // xz plane
	S = XMMatrixShadow(shadowPlane, toMainLight);
	XMMATRIX shadowOffsetY = XMMatrixTranslation(0.0f, 0.01f, 0.0f);
	XMStoreFloat4x4(&mShadowedSkullRitemFloor->World, skullWorld * S * shadowOffsetY);

	mSkullRitem->NumFramesDirty = gNumFrameResources;
	mShadowedSkullRitemWall->NumFramesDirty = gNumFrameResources;
	mShadowedSkullRitemFloor->NumFramesDirty = gNumFrameResources;
	mReflectedSkullRitem->NumFramesDirty = gNumFrameResources;
}

void d3dApp::UpdateCamera(const GameTimer& gt)
{
	// Convert Spherical to Cartesian coordinates.
	mEyePos.x = mRadius * sinf(mPhi) * cosf(mTheta);
	mEyePos.z = mRadius * sinf(mPhi) * sinf(mTheta);
	mEyePos.y = mRadius * cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);
}

void d3dApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (auto& e : mAllRitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

void d3dApp::UpdateMaterialCBs(const GameTimer& gt)
{
	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
	for (auto& e : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void d3dApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMVECTOR det = XMMatrixDeterminant(view);
	XMMATRIX invView = XMMatrixInverse(&det, view);
	det = XMMatrixDeterminant(proj);
	XMMATRIX invProj = XMMatrixInverse(&det, proj);
	det = XMMatrixDeterminant(viewProj);
	XMMATRIX invViewProj = XMMatrixInverse(&det, viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mEyePos;
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void d3dApp::UpdateReflectedPassCB(const GameTimer& gt)
{
	mReflectedPassCB = mMainPassCB;

	XMVECTOR mirrorPlane = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); // xz plane
	XMMATRIX R = XMMatrixReflect(mirrorPlane);

	// Reflect the lighting.
	for (int i = 0; i < 3; ++i)
	{
		XMVECTOR lightDir = XMLoadFloat3(&mMainPassCB.Lights[i].Direction);
		XMVECTOR reflectedLightDir = XMVector3TransformNormal(lightDir, R);
		XMStoreFloat3(&mReflectedPassCB.Lights[i].Direction, reflectedLightDir);
	}

	// Reflected pass stored in index 1
	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(1, mReflectedPassCB);
}

LRESULT d3dApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) 
	{

	case WM_ACTIVATE:
		if(LOWORD(wParam) == WA_INACTIVE)
		{
			mAppPaused = true;
			mTimer.Stop();
		}
		else {
			mAppPaused = false;
			mTimer.Start();
		}
		return 0;

	case WM_SIZE:
		mClientWidth = LOWORD(lParam);
		mClientHeight = HIWORD(lParam);
		if (md3dDevice) {
			if (wParam == SIZE_MINIMIZED) {
				mAppPaused = true;
				mMinimized = true;
				mMaximized = false;
			}
			else if (wParam == SIZE_MAXIMIZED) {
				mAppPaused = false;
				mMinimized = false;
				mMaximized = true;
				OnResize();
			}
			else if (wParam == SIZE_RESTORED) {

				if (mMinimized) {
					mAppPaused = false;
					mMinimized = false;
					OnResize();
				}
				else if (mMaximized) {
					mAppPaused = false;
					mMaximized = false;
					OnResize();
				}
				else if (mResizing) {
					// nothing
				}
				else {
					OnResize();
				}
			}
		}
		return 0;

	case WM_ENTERSIZEMOVE:
		mAppPaused = true;
		mResizing = true;
		mTimer.Stop();
		return 0;

	case WM_EXITSIZEMOVE:
		mAppPaused = false;
		mResizing = false;
		mTimer.Start();
		OnResize();
		return 0;

	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(mhAppInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hwnd, About);
			return 0;
		case IDM_EXIT:
			DestroyWindow(hwnd);
			return 0;
		}
	}
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);
		// TODO: Add any drawing code that uses hdc here...
		EndPaint(hwnd, &ps);
	}
	break;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_MENUCHAR:
		return MAKELRESULT(0, MNC_CLOSE);

	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_KEYUP:
		if (wParam == VK_ESCAPE)
		{
			PostQuitMessage(0);
		}
		else if (wParam == VK_F2) {
			Set4xMsaaState(!m4xMsaaState);
		}
		else if (wParam == VK_F1) {
			mIsWireframe = !mIsWireframe;
		}
		return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void d3dApp::FlushCommandQueue()
{
	mCurrentFence++;

	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

	if (mFence->GetCompletedValue() < mCurrentFence)
	{
		HANDLE eventHandle = CreateEventEx(NULL, NULL, false, EVENT_ALL_ACCESS);

		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));

		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

ID3D12Resource* d3dApp::CurrentBackBuffer()const
{
	return mSwapChainBuffer[mCurrBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE d3dApp::CurrentBackBufferView()const
{
	D3D12_CPU_DESCRIPTOR_HANDLE descHandle = mRtvHeap->GetCPUDescriptorHandleForHeapStart();
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		descHandle,
		mCurrBackBuffer,
		mRtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE d3dApp::DepthStencilView()const
{
	return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void d3dApp::CalculateFrameStats()
{
	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	if ((mTimer.TotalTime() - timeElapsed) >= 1.0f)
	{
		float fps = (float)frameCnt;
		float mspf = 1000.0f / fps;

		wstring fpsStr = to_wstring(fps);
		wstring mspfStr = to_wstring(mspf);

		wstring windowText = mMainWndCaption +
			L"   fps: " + fpsStr +
			L"  mspf: " + mspfStr;

		SetWindowText(mhMainWnd, windowText.c_str());

		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}

void d3dApp::LogAdapters()
{
	UINT i = 0;
	IDXGIAdapter* adapter = nullptr;
	std::vector<IDXGIAdapter*> adapterList;
	while (mdxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc(&desc);

		std::wstring text = L"***Adapter: ";
		text += desc.Description;
		text += L"\n";

		OutputDebugString(text.c_str());

		adapterList.push_back(adapter);

		++i;
	}

	for (size_t i = 0; i < adapterList.size(); ++i)
	{
		LogAdapterOutputs(adapterList[i]);
		ReleaseCom(adapterList[i]);
	}
}

void d3dApp::LogAdapterOutputs(IDXGIAdapter* adapter)
{
	UINT i = 0;
	IDXGIOutput* output = nullptr;
	while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_OUTPUT_DESC desc;
		output->GetDesc(&desc);

		std:wstring text = L"***Output: ";
		text += desc.DeviceName;
		text += L"\n";
		OutputDebugString(text.c_str());

		LogOutputDisplayModes(output, mBackBufferFormat);

		ReleaseCom(output);

		++i;
	}
}

void d3dApp::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
{
	UINT count = 0;
	UINT flags = 0;

	output->GetDisplayModeList(format, flags, &count, nullptr);

	std::vector<DXGI_MODE_DESC> modeList(count);
	output->GetDisplayModeList(format, flags, &count, &modeList[0]);

	for (auto& x : modeList)
	{
		UINT n = x.RefreshRate.Numerator;
		UINT d = x.RefreshRate.Denominator;
	std:wstring text =
		L"Width = " + std::to_wstring(x.Width) + L" " +
		L"Height = " + std::to_wstring(x.Height) + L" " +
		L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) +
		L"\n";

	::OutputDebugString(text.c_str());
	}
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

