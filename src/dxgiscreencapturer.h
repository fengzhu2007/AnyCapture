#ifndef DXGISCREENCAPTURER_H
#define DXGISCREENCAPTURER_H
#include <QApplication>
#include <QPixmap>
#include <QImage>
#include <dxgi.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>


using namespace Microsoft::WRL;

class DXGIScreenCapturer
{
public:
    DXGIScreenCapturer();

    ~DXGIScreenCapturer();
    QPixmap captureScreen();


    bool initialize();
    void cleanup();

private:
    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_deviceContext;
    ComPtr<IDXGIOutputDuplication> m_duplication;
    DXGI_OUTDUPL_FRAME_INFO m_frameInfo;
    bool m_initialized = false;

};

#endif // DXGISCREENCAPTURER_H
