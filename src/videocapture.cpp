#include "videocapture.h"

#ifdef Q_OS_WIN

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


#endif

#include "recorder.h"

#include <QElapsedTimer>
#include <QRect>
#include <QImage>
#include <QDebug>

namespace adc{
class VideoCapturePrivate{
public:
    Recorder* instance;
    Microsoft::WRL::ComPtr<ID3D11Device> d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3dContext;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> stagingTexture;

    winrt::Windows::Graphics::Capture::GraphicsCaptureItem captureItem{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool framePool{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession captureSession{ nullptr };

    //QTimer m_captureTimer;
    bool capturing = false;
    bool initialized = false;
    bool paused = false;
    UINT width = 0;
    UINT height = 0;
    float interval = 0;
    int fps = 30;
    long long pts = 0;

    HWND hwnd=0;
    HMONITOR monitor=0;
    QRect rect;
    VideoCapture::Mode mode = VideoCapture::Screen;
    QElapsedTimer timer;
};

VideoCapture::VideoCapture(Recorder* instance)
    : QThread{(QObject*)instance}
{
    d = new VideoCapturePrivate;
    d->instance = instance;
    connect(this, &QThread::finished, this, &VideoCapture::onFinished);

}

void VideoCapture::setMode(Mode mode){
    d->mode = mode;
}

void VideoCapture::setScreen(HMONITOR monitor){
    d->monitor = monitor;
}

void VideoCapture::setWindow(HWND hwnd){
    d->hwnd = hwnd;
}

void VideoCapture::setRegion(const QRect& rc){
    d->rect = rc;
}

bool VideoCapture::init()
{
    if (d->initialized) {
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
    d->initialized = true;
    qDebug() << "Graphics Capture initialized successfully";
    return true;
}

bool VideoCapture::startRecording(){
    qDebug()<<"VideoCapture::start";
    if (d->capturing) {
        stopRecording();
    }
    if (!init()) {
        return false;
    }
    qDebug()<<"VideoCapture::start1";
    if(d->mode==Screen || d->mode==Region){
        if(d->monitor==0){
            if (!createCaptureItem(MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY))) {
                return false;
            }
        }else{
            if (!createCaptureItem(d->monitor)) {
                return false;
            }
        }
        qDebug()<<"VideoCapture::start2";
    }else{
        if (!createCaptureItem(d->hwnd)) {
            return false;
        }
    }
    if (!createFramePool()) {
        return false;
    }
    if (!createCaptureSession()) {
        return false;
    }
    try {
        d->captureSession.StartCapture();
        d->capturing = true;
        qDebug() << "Graphics Capture started successfully";
        this->start();
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

void VideoCapture::stopRecording(){
    d->capturing = false;
}

VideoCapture::~VideoCapture()
{
    this->stopRecording();
    delete d;
}


bool VideoCapture::createD3DDevice()
{
    HRESULT hr = D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE, nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&d->d3dDevice, nullptr,&d->d3dContext);
    if (FAILED(hr)) {
        qDebug() << "Failed to create D3D11 device:" << hr;
        return false;
    }
    return true;
}

bool VideoCapture::createCaptureItem(HWND hwnd)
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
        d->captureItem = item;
        auto size = d->captureItem.Size();
        d->width = size.Width;
        d->height = size.Height;
        qDebug() << "Capture item created. Size:" << d->width << "x" << d->height;
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

bool VideoCapture::createCaptureItem(HMONITOR monitor){
    try {
        auto factory = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>();
        auto interop = factory.as<IGraphicsCaptureItemInterop>();
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem item{ nullptr };
        winrt::check_hresult(interop->CreateForMonitor(monitor,winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),winrt::put_abi(item)));
        d->captureItem = item;
        auto size = d->captureItem.Size();
        d->width = size.Width;
        d->height = size.Height;
        qDebug() << "Monitor capture item created. Size:" << d->width << "x" << d->height;
        return true;
    }catch (const winrt::hresult_error& error) {
        qDebug() << "Failed to create monitor capture item. Error:" << error.code() << QString::fromWCharArray(error.message().c_str());
        return false;
    }catch (...) {
        qDebug() << "Unknown error creating monitor capture item";
        return false;
    }
}


bool VideoCapture::createFramePool()
{
    try {
        if (!d->captureItem) {
            qDebug() << "No capture item available";
            return false;
        }
        Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
        HRESULT hr = d->d3dDevice.As(&dxgiDevice);
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
        d->framePool = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::Create(direct3DDevice,winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,2,d->captureItem.Size());
        qDebug() << "Frame pool created successfully";
        return true;
    }catch (const winrt::hresult_error& error) {
        qDebug() << "Failed to create frame pool. Error:" << error.code() << QString::fromWCharArray(error.message().c_str());
        return false;
    }catch (...) {
        qDebug() << "Unknown error creating frame pool";
        return false;
    }
}

bool VideoCapture::createCaptureSession()
{
    try {
        if (!d->framePool || !d->captureItem) {
            qDebug() << "Frame pool or capture item not available";
            return false;
        }
        d->captureSession = d->framePool.CreateCaptureSession(d->captureItem);
        qDebug() << "Capture session created successfully";
        return true;
    }catch (const winrt::hresult_error& error) {
        qDebug() << "Failed to create capture session. Error:" << error.code() << QString::fromWCharArray(error.message().c_str());
        return false;
    }catch (...) {
        qDebug() << "Unknown error creating capture session";
        return false;
    }
}

QImage VideoCapture::captureFrame()
{
    try {
        auto frame = d->framePool.TryGetNextFrame();
        if (!frame) {
            return QImage();
        }
        auto surface = frame.Surface();
        winrt::com_ptr<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess> dxgiInterface;
        dxgiInterface = surface.as<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();

        Microsoft::WRL::ComPtr<ID3D11Texture2D> pTexture;
        HRESULT hr = dxgiInterface->GetInterface(IID_PPV_ARGS(&pTexture));
        if (FAILED(hr)) {
            qDebug() << "Failed to get texture interface:" << hr;
            return QImage();
        }
        auto texture = pTexture.Get();
        {
            if (!texture || !d->d3dDevice) {
                return QImage();
            }
            D3D11_TEXTURE2D_DESC desc;
            texture->GetDesc(&desc);
            // staging texture
            if (!d->stagingTexture || desc.Width != d->width || desc.Height != d->height) {
                d->stagingTexture.Reset();

                desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                desc.Usage = D3D11_USAGE_STAGING;
                desc.BindFlags = 0;
                desc.MiscFlags = 0;

                HRESULT hr = d->d3dDevice->CreateTexture2D(&desc, nullptr, &d->stagingTexture);
                if (FAILED(hr)) {
                    qDebug() << "Failed to create staging texture:" << hr;
                    return QImage();
                }
            }
            d->d3dContext->CopyResource(d->stagingTexture.Get(), texture);
            D3D11_MAPPED_SUBRESOURCE mapped;
            HRESULT hr = d->d3dContext->Map(d->stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
            if (SUCCEEDED(hr)) {
                QImage image(static_cast<uchar*>(mapped.pData),desc.Width, desc.Height, mapped.RowPitch,QImage::Format_ARGB32);
                QImage result = image.copy();
                d->d3dContext->Unmap(d->stagingTexture.Get(), 0);
                return result;
            }
            return QImage();
        }
    }catch (const winrt::hresult_error& error) {
        qDebug() << "Error capturing frame:" << error.code() << QString::fromWCharArray(error.message().c_str());
        return QImage();
    }catch (...) {
        qDebug() << "Unknown error capturing frame";
        return QImage();
    }
}


std::vector<HMONITOR> VideoCapture::availableMonitors(){
    std::vector<HMONITOR> monitors;
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam) -> BOOL {
        auto monitors = reinterpret_cast<std::vector<HMONITOR>*>(lParam);
        monitors->push_back(hMonitor);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&monitors));
    return monitors;
}


void VideoCapture::run(){
    while(d->capturing){
        if(!d->paused){
            int msec = d->timer.elapsed();
            if(msec>=d->interval){
                d->timer.restart();
                auto image = this->captureFrame();
                if (!image.isNull()) {
                    d->instance->pushVideoFrame(image);
                }
            }
        }
    }
}

void VideoCapture::onFinished() {
        try {
            if (d->captureSession) {
                d->captureSession.Close();
                d->captureSession = nullptr;
            }
            if (d->framePool) {
                d->framePool.Close();
                d->framePool = nullptr;
            }
            d->captureItem = nullptr;
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
