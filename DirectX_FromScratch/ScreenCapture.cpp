//#include "ScreenCapture.h"
//
//void ScreenCapture::GetPixelsFromIDXGIOutput(IDXGIOutput* pOutput) {
//    // Step 1: Get the output description
//    DXGI_OUTPUT_DESC outputDesc;
//    pOutput->GetDesc(&outputDesc);
//
//    // Step 2: Create a D3D11 device and context
//    ComPtr<ID3D11Device> device;
//    ComPtr<ID3D11DeviceContext> context;
//    D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, &context);
//
//    // Step 3: Get the DXGI output duplication
//    ComPtr<IDXGIOutput1> output1;
//    pOutput->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void**>(output1.GetAddressOf()));
//
//    ComPtr<IDXGIOutputDuplication> outputDuplication;
//    output1->DuplicateOutput(device.Get(), &outputDuplication);
//
//    // Step 4: Acquire the next frame
//    DXGI_OUTDUPL_FRAME_INFO frameInfo;
//    ComPtr<IDXGIResource> desktopResource;
//    outputDuplication->AcquireNextFrame(0, &frameInfo, &desktopResource);
//
//    // Step 5: Query for the texture interface
//    ComPtr<ID3D11Texture2D> desktopImage;
//    desktopResource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(desktopImage.GetAddressOf()));
//
//    // Step 6: Create a staging texture
//    D3D11_TEXTURE2D_DESC desc;
//    desktopImage->GetDesc(&desc);
//    desc.Usage = D3D11_USAGE_STAGING;
//    desc.BindFlags = 0;
//    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
//    desc.MiscFlags = 0;
//
//    ComPtr<ID3D11Texture2D> stagingTexture;
//    device->CreateTexture2D(&desc, nullptr, &stagingTexture);
//
//    // Step 7: Copy the output to the staging texture
//    context->CopyResource(stagingTexture.Get(), desktopImage.Get());
//
//    // Step 8: Map the staging texture to access the pixel data
//    D3D11_MAPPED_SUBRESOURCE mappedResource;
//    context->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mappedResource);
//
//    // Access the pixel data
//    BYTE* pData = reinterpret_cast<BYTE*>(mappedResource.pData);
//    // Process the pixel data as needed...
//
//    // Unmap the resource
//    context->Unmap(stagingTexture.Get(), 0);
//
//    // Release the frame
//    outputDuplication->ReleaseFrame();
//}