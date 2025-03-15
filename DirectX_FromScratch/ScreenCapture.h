#pragma once
using Microsoft::WRL::ComPtr;
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

class ScreenCapture
{
public:
    ScreenCapture();
    ScreenCapture(const ScreenCapture& rhs) = delete;
    ScreenCapture& operator=(const ScreenCapture& rhs) = delete;
    ~ScreenCapture();
    void GetPixelsFromIDXGIOutput(IDXGIOutput* pOutput);
};

