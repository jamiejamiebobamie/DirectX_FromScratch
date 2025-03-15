// DirectX_FromScratch.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "DirectX_FromScratch.h"
#include "d3dApp.h"

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        d3dApp theApp(hInstance); // declare instance
        if (!theApp.Initialize()) // init instance
            return 0;

        return theApp.Run(); // run app
    }
    catch (DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}


/*
* A. Call Initialize():
    *   1. InitMainWindow
    *       a. creates the Win32 window: CreateWindow
            b. shows window: ShowWindow
            c. calls: UpdateWindow to WM_PAINT the window to the screen
    *   2. InitDirect3D
    *		a. Creates the mdxgiFactory
    *		b. Creates the md3dDevice with the specified feature level -- D3D_FEATURE_LEVEL_11_0
    *			i. Creates a WARP adapter if hardware adapter fails
    *		c. Creates the fence
    *		d. Queries the Descriptor Heap Sizes for each descriptor heap type and caches them:
    *			i. D3D12_DESCRIPTOR_HEAP_TYPE_RTV
    *			ii. D3D12_DESCRIPTOR_HEAP_TYPE_DSV
    *			iii. D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
    *       e. Queries the feature support for multisampling and caches the m4xMsaaQuality level
    * 
    *  - - - - - - - - - - - - - - - - 
    * 
    *       f. Calls: CreateCommandObjects
    *           i. Creates command queue
    *           ii. Creates command allocator
    *           iii. Creates command list 
    *           iv. Closes the command list
    * 
    *       g. Calls: CreateSwapChain
    *			i. Reset the swapchain: mSwapChain.Reset()
    *			ii. Create the swapchain: mdxgiFactory->CreateSwapChain
    * 
    *       h. Calls: CreateRtvAndDsvDescriptorHeaps
    *           i. Create the RTV descriptor heaps: md3dDevice->CreateDescriptorHeap
    *           ii. Create the DSV descriptor heaps: md3dDevice->CreateDescriptorHeap
    *
    *  - - - - - - - - - - - - - - - -
    * 
    *   3. Calls: OnResize
    *			i.		Assert: md3dDevice, mSwapChain, mDirectCmdListAlloc);
    *			ii.		Call: FlushCommandQueue
    *			iii.	Reset command list: mCommandList->Reset
    *			iv.		Reset all buffers for the swap chain: mSwapChainBuffer[i].Reset();
    *			v.		Reset stencil buffer: mDepthStencilBuffer.Reset();
    *			vi.		Resize the swapchain buffers buffers: mSwapChain->ResizeBuffers
    *			vii.	Set the current back buffer to 0
    *			viii.	Create the RTV for each swapchain buffer and store it in the rtvHeap: md3dDevice->CreateRenderTargetView
    *			ix.		Create commited resource for the depth buffer: md3dDevice->CreateCommittedResource
    *			x.		Create the depth stencil buffer view: md3dDevice->CreateDepthStencilView
    *           xi.     Transition the depth stencil buffer to depth write
    *           xii.    Close the command list
    *           xiii.   Execute the command list
    *           xiv.    FlushCommandQueue
    *           xv.     Reset the viewport and scissorRect 
* B. Call Run():
    *   1. Init Win32 MSG variable
    *   2. Init the timer: mTimer.Reset()
    *   3. Enter while loop that continues while: msg.message != WM_QUIT
    *           i. Peek for messages: PeekMessage()
    *               -> Handle messages by calling: TranslateMessage(&msg) and DispatchMessage(&msg);
    *   4. If no messages:
    *           i. Tick timer: mTimer.Tick()
    *           ii. Check if game is not paused: !mAppPaused
    *               -> Call: CalculateFrameStats(), Update(mTimer), and Draw(mTimer)
    * 
    *  - - - - - - - - - - - - - - - -
    *
    *      Draw():
    *           1. Reset the command alloactor: mDirectCmdListAlloc->Reset()
    *           2. Reset the command list: mCommandList->Reset
    *           3. Transition the back buffer to render target: mCommandList->ResourceBarrier
    *           4. Reset the viewport: mCommandList->RSSetViewports
    *           5. Reset the scissor rect: mCommandList->RSSetScissorRects
    *           6. Clear the pixels in the back buffer to default color: mCommandList->ClearRenderTargetView
    *           7. Clear the values in the depth/stencil buffer to default value: mCommandList->ClearDepthStencilView
    *           8. Specify the rt/ds buffers we are going to render to: mCommandList->OMSetRenderTargets
    *           9. Transition the back buffer to present: mCommandList->ResourceBarrier
    *           10. Close the command list: mCommandList->Close
    *           11. Execute the commands in the command list: mCommandQueue->ExecuteCommandLists
    *           12. Swap the front/back buffers: mSwapChain->Present
    *           13. Increment the swapchain buffer index: mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount
    *           14. Wait for the all commands from the last frame to be executed: FlushCommandQueue
    * 
    *  - - - - - - - - - - - - - - - -
    * 
    *           iii. If game is paused
    *               -> Call: Sleep(100)
    * 
    * 
    *   
*/