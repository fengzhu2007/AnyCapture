// graphicscapture.cpp
#include "windowcapture.h"
#include <QDebug>


#ifdef Q_OS_WIN
#pragma comment(lib, "windowsapp")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

extern "C" {
HRESULT __stdcall CreateDirect3D11DeviceFromDXGIDevice(
    ::IDXGIDevice* dxgiDevice,
    ::IInspectable** graphicsDevice);
}

#endif

namespace adc{
WindowCapture::WindowCapture(QObject *parent)
    : QObject(parent)
    , m_captureTimer(this)
{
    connect(&m_captureTimer, &QTimer::timeout, this, &WindowCapture::onCaptureTimeout);
}

WindowCapture::~WindowCapture()
{
    stopCapture();
}

bool WindowCapture::initialize()
{
    if (m_initialized) {
        return true;
    }

    if (!winrt::Windows::Graphics::Capture::GraphicsCaptureSession::IsSupported()) {
        qDebug() << "Graphics Capture is not supported on this system";
        return false;
    }

    if (!createD3DDevice()) {
        qDebug() << "Failed to create D3D device";
        return false;
    }

    m_initialized = true;
    qDebug() << "Graphics Capture initialized successfully";
    return true;
}

bool WindowCapture::createD3DDevice()
{
    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &m_d3dDevice,
        nullptr,
        &m_d3dContext
        );

    if (FAILED(hr)) {
        qDebug() << "Failed to create D3D11 device:" << hr;
        return false;
    }

    return true;
}

bool WindowCapture::createCaptureItem(HWND hwnd)
{
    try {
        auto factory = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>();
        auto interop = factory.as<IGraphicsCaptureItemInterop>();

        winrt::Windows::Graphics::Capture::GraphicsCaptureItem item{ nullptr };


        if(hwnd){
            winrt::check_hresult(interop->CreateForWindow(hwnd,
                                                          winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
                                                          winrt::put_abi(item)));
        } else {
            winrt::check_hresult(interop->CreateForMonitor(
                MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY),
                winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
                winrt::put_abi(item)));
        }
        m_captureItem = item;

        auto size = m_captureItem.Size();
        m_width = size.Width;
        m_height = size.Height;

        qDebug() << "Capture item created. Size:" << m_width << "x" << m_height;
        return true;
    }
    catch (const winrt::hresult_error& error) {
        qDebug() << "Failed to create capture item. Error:" << error.code() << QString::fromWCharArray(error.message().c_str());
        return false;
    }
    catch (...) {
        qDebug() << "Unknown error creating capture item";
        return false;
    }
}

bool WindowCapture::createCaptureItem(HMONITOR monitor){
    try {
        auto factory = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>();
        auto interop = factory.as<IGraphicsCaptureItemInterop>();

        winrt::Windows::Graphics::Capture::GraphicsCaptureItem item{ nullptr };

        winrt::check_hresult(interop->CreateForMonitor(
            monitor,
            winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
            winrt::put_abi(item)));

        m_captureItem = item;

        auto size = m_captureItem.Size();
        m_width = size.Width;
        m_height = size.Height;

        qDebug() << "Monitor capture item created. Size:" << m_width << "x" << m_height;
        return true;
    }
    catch (const winrt::hresult_error& error) {
        qDebug() << "Failed to create monitor capture item. Error:" << error.code() << QString::fromWCharArray(error.message().c_str());
        return false;
    }
    catch (...) {
        qDebug() << "Unknown error creating monitor capture item";
        return false;
    }
}


bool WindowCapture::createFramePool()
{
    try {
        if (!m_captureItem) {
            qDebug() << "No capture item available";
            return false;
        }

        Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
        HRESULT hr = m_d3dDevice.As(&dxgiDevice);
        if (FAILED(hr)) {
            qDebug() << "Failed to get DXGI device:" << hr;
            return false;
        }

        winrt::com_ptr<IInspectable> inspectable;
        hr = CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.Get(), inspectable.put());
        if (FAILED(hr)) {
            qDebug() << "Failed to create Direct3D device:" << hr;
            return false;
        }

        auto direct3DDevice = inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();

        m_framePool = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::Create(
            direct3DDevice,
            winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
            2,
            m_captureItem.Size()
            );

        qDebug() << "Frame pool created successfully";
        return true;
    }
    catch (const winrt::hresult_error& error) {
        qDebug() << "Failed to create frame pool. Error:" << error.code() << QString::fromWCharArray(error.message().c_str());
        return false;
    }
    catch (...) {
        qDebug() << "Unknown error creating frame pool";
        return false;
    }
}

bool WindowCapture::createCaptureSession()
{
    try {
        if (!m_framePool || !m_captureItem) {
            qDebug() << "Frame pool or capture item not available";
            return false;
        }

        m_captureSession = m_framePool.CreateCaptureSession(m_captureItem);
        qDebug() << "Capture session created successfully";
        return true;
    }
    catch (const winrt::hresult_error& error) {
        qDebug() << "Failed to create capture session. Error:" << error.code() << QString::fromWCharArray(error.message().c_str());
        return false;
    }
    catch (...) {
        qDebug() << "Unknown error creating capture session";
        return false;
    }
}

bool WindowCapture::startCapture(HWND hwnd)
{
    if (m_capturing) {
        stopCapture();
    }

    if (!initialize()) {
        return false;
    }

    if (!createCaptureItem(hwnd)) {
        return false;
    }

    if (!createFramePool()) {
        return false;
    }

    if (!createCaptureSession()) {
        return false;
    }

    try {
        m_captureSession.StartCapture();
        m_capturing = true;
        m_captureTimer.start(33); // ~30 FPS

        qDebug() << "Graphics Capture started successfully";
        return true;
    }
    catch (const winrt::hresult_error& error) {
        qDebug() << "Failed to start capture. Error:" << error.code() << QString::fromWCharArray(error.message().c_str());
        return false;
    }
    catch (...) {
        qDebug() << "Unknown error starting capture";
        return false;
    }
}

void WindowCapture::stopCapture()
{
    if (m_capturing) {
        m_captureTimer.stop();
        m_capturing = false;

        try {
            if (m_captureSession) {
                m_captureSession.Close();
                m_captureSession = nullptr;
            }

            if (m_framePool) {
                m_framePool.Close();
                m_framePool = nullptr;
            }

            m_captureItem = nullptr;
        }
        catch (const winrt::hresult_error& error) {
            qDebug() << "Error during stop capture:" << error.code() << QString::fromWCharArray(error.message().c_str());
        }
        catch (...) {
            qDebug() << "Unknown error during stop capture";
        }

        qDebug() << "Graphics Capture stopped";
    }
}

void WindowCapture::onCaptureTimeout()
{
    if (!m_capturing || !m_framePool) {
        return;
    }

    QImage image = captureFrame();
    if (!image.isNull()) {
        emit frameCaptured(image);
    }else{
        qDebug()<<"image is null";
    }
}

QImage WindowCapture::captureFrame()
{
    try {
        auto frame = m_framePool.TryGetNextFrame();
        if (!frame) {
            return QImage();
        }

        auto surface = frame.Surface();

        winrt::com_ptr<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess> dxgiInterface;
        dxgiInterface = surface.as<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();

        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        HRESULT hr = dxgiInterface->GetInterface(IID_PPV_ARGS(&texture));
        if (FAILED(hr)) {
            qDebug() << "Failed to get texture interface:" << hr;
            return QImage();
        }

        return textureToImage(texture.Get());
    }
    catch (const winrt::hresult_error& error) {
        qDebug() << "Error capturing frame:" << error.code() << QString::fromWCharArray(error.message().c_str());
        return QImage();
    }
    catch (...) {
        qDebug() << "Unknown error capturing frame";
        return QImage();
    }
}

QImage WindowCapture::textureToImage(ID3D11Texture2D* texture)
{
    if (!texture || !m_d3dDevice) {
        return QImage();
    }

    D3D11_TEXTURE2D_DESC desc;
    texture->GetDesc(&desc);

    // staging texture
    if (!m_stagingTexture || desc.Width != m_width || desc.Height != m_height) {
        m_stagingTexture.Reset();

        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.MiscFlags = 0;

        HRESULT hr = m_d3dDevice->CreateTexture2D(&desc, nullptr, &m_stagingTexture);
        if (FAILED(hr)) {
            qDebug() << "Failed to create staging texture:" << hr;
            return QImage();
        }
    }

    m_d3dContext->CopyResource(m_stagingTexture.Get(), texture);

    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = m_d3dContext->Map(m_stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (SUCCEEDED(hr)) {
        QImage image(static_cast<uchar*>(mapped.pData),
                     desc.Width, desc.Height, mapped.RowPitch,
                     QImage::Format_ARGB32);

        QImage result = image.copy();
        m_d3dContext->Unmap(m_stagingTexture.Get(), 0);
        return result;
    }

    return QImage();
}


std::vector<HMONITOR> WindowCapture::availableMonitors(){
    std::vector<HMONITOR> monitors;
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam) -> BOOL {
        auto monitors = reinterpret_cast<std::vector<HMONITOR>*>(lParam);
        monitors->push_back(hMonitor);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&monitors));

    return monitors;
}

}
