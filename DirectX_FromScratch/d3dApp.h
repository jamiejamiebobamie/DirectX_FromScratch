#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "d3dUtil.h"
#include "GameTimer.h"
#include "MathHelper.h"
#include "UploadBuffer.h"
#include "GeometryGenerator.h"
#include "FrameResource.h"
#include "GpuWaves.h"
#include "SobelFilter.h"
#include "RenderTarget.h"

// Link necessary d3d12 libraries.
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

enum class RenderLayer : int
{
	Opaque = 0,
	Transparent,
	AlphaTested,
	AlphaTestedTreeSprites,
	GpuWaves,
	Count
};

extern const int gNumFrameResources;

struct TreeSpriteVertex
{
	XMFLOAT3 Pos;
	XMFLOAT2 Size;
	XMFLOAT3 Slope;
};

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in the world.
	XMFLOAT4X4 World = MathHelper::Identity4x4();

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Used for GPU waves render items.
	DirectX::XMFLOAT2 DisplacementMapTexelSize = { 1.0f, 1.0f };
	float GridSpatialStep = 1.0f;

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify object data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

class d3dApp
{

public:

	static d3dApp* GetApp();

	d3dApp(HINSTANCE hInstance);
	d3dApp(const d3dApp& rhs) = delete;
	d3dApp& operator=(const d3dApp& rhs) = delete;
	~d3dApp();

	HINSTANCE AppInst() const;
	HWND MainWnd() const;
	float AspectRatio() const;

	bool Get4xMsaaState() const;
	void Set4xMsaaState(bool value);

	int Run();

	bool Initialize();
	LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
	void UpdateCamera(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateWavesGPU(const GameTimer& gt);

	void LoadTextures();
	void BuildRootSignature();
	void BuildWavesRootSignature();
	void BuildPostProcessRootSignature();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	void BuildLandGeometry();
	void BuildWavesGeometry();
	void BuildBoxGeometry();
	void BuildTreeSpritesGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

	float GetHillsHeight(float x, float z)const;
	XMFLOAT3 GetHillsNormal(float x, float z)const;
	XMFLOAT3 GetHillsSlope(float x, float z)const;

private:
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	UINT mCbvSrvDescriptorSize = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12RootSignature> mWavesRootSignature = nullptr;
	ComPtr<ID3D12RootSignature> mPostProcessRootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mStdInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mTreeSpriteInputLayout;

	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	std::unique_ptr<GpuWaves> mWaves;

	std::unique_ptr<RenderTarget> mOffscreenRT = nullptr;

	std::unique_ptr<SobelFilter> mSobelFilter = nullptr;

	PassConstants mMainPassCB;

	bool mIsWireframe = false;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mTheta = 1.5f * XM_PI;
	float mPhi = 0.2f * XM_PI;
	float mRadius = 15.0f;

	POINT mLastMousePos;

protected:

	 void CreateRtvAndDsvDescriptorHeaps();
	 void OnResize();
	 void Update(const GameTimer& gt);
	 void Draw(const GameTimer& gt);
	 void DrawFullscreenQuad(ID3D12GraphicsCommandList* cmdList);

	 void OnMouseDown(WPARAM btnState, int x, int y);
	 void OnMouseUp(WPARAM btnState, int x, int y);
	 void OnMouseMove(WPARAM btnState, int x, int y);

protected:

	bool InitMainWindow();
	bool InitDirect3D();
	void CreateCommandObjects();
	void CreateSwapChain();

	void FlushCommandQueue();

	ID3D12Resource* CurrentBackBuffer()const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

	void CalculateFrameStats();

	void LogAdapters();
	void LogAdapterOutputs(IDXGIAdapter* adapter);
	void LogOutputDisplayModes(IDXGIOutput* ouput, DXGI_FORMAT format);

protected:

	static d3dApp* mApp;

	HINSTANCE mhAppInst = nullptr; // application instance handle
	HWND mhMainWnd = nullptr; // main window handle
	bool mAppPaused = false; // is the app paused?
	bool mMinimized = false; // is the app minimized?
	bool mMaximized = false; // is the app maximized?
	bool mResizing = false; // is the app resizing?
	bool mFullscreen = false; // is fullscreen enabled?

	bool m4xMsaaState = false; // is 4X MSAA enabled?
	UINT m4xMsaaQuality = 0; // quality level of 4X MSAA

	GameTimer mTimer;

	Microsoft::WRL::ComPtr<IDXGIFactory4> mdxgiFactory;
	Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;
	Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice;

	Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
	UINT mCurrentFence = 0;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;

	static const int SwapChainBufferCount = 2;
	int mCurrBackBuffer = 0;
	Microsoft::WRL::ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];
	Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;

	D3D12_VIEWPORT mScreenViewport;
	D3D12_RECT mScissorRect;

	UINT mRtvDescriptorSize = 0;
	UINT mDsvDescriptorSize = 0;
	UINT mCbvSrvUavDescriptorSize = 0;

	std::wstring mMainWndCaption = L"DirectX from scratch";
	D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	int mClientWidth = 800;
	int mClientHeight = 600;

	bool mIsEdited = false;

};

