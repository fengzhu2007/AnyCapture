// graphicscapture.h
#pragma once

#include <QObject>
#include <QTimer>
#include <QImage>
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

// WinRT header files
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/Windows.Foundation.h>

#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>


namespace adc{

class WindowCapture : public QObject
{
    Q_OBJECT

public:
    WindowCapture(QObject *parent = nullptr);
    ~WindowCapture();

    bool initialize();
    bool startCapture(HWND hwnd);
    void stopCapture();
    bool isCapturing() const { return m_capturing; }
    QImage captureFrame();

    static std::vector<HMONITOR> availableMonitors();

signals:
    void frameCaptured(const QImage &image);

private slots:
    void onCaptureTimeout();

private:
    bool createD3DDevice();
    bool createCaptureItem(HWND hwnd);
    bool createCaptureItem(HMONITOR monitor);
    bool createFramePool();
    bool createCaptureSession();

    QImage textureToImage(ID3D11Texture2D* texture);

    Microsoft::WRL::ComPtr<ID3D11Device> m_d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3dContext;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_stagingTexture;

    winrt::Windows::Graphics::Capture::GraphicsCaptureItem m_captureItem{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession m_captureSession{ nullptr };

    QTimer m_captureTimer;
    bool m_capturing = false;
    bool m_initialized = false;
    UINT m_width = 0;
    UINT m_height = 0;
};
}
