#include "dxgiscreencapturer.h"

#include <QDebug>

DXGIScreenCapturer::DXGIScreenCapturer()
{
    auto ret = initialize();
    if(!ret){
        qDebug()<<"initialize failed";
    }
}

DXGIScreenCapturer::~DXGIScreenCapturer()
{
    cleanup();
}

QPixmap DXGIScreenCapturer::captureScreen()
{
    if (!m_initialized) {
        return QPixmap();
    }

    ComPtr<IDXGIResource> desktopResource;
    HRESULT hr = m_duplication->AcquireNextFrame(0, &m_frameInfo, &desktopResource);
    if (FAILED(hr)) {
        return QPixmap();
    }

    ComPtr<ID3D11Texture2D> acquiredTexture;
    hr = desktopResource.As(&acquiredTexture);
    if (FAILED(hr)) {
        m_duplication->ReleaseFrame();
        return QPixmap();
    }

    // 创建临时纹理用于拷贝
    D3D11_TEXTURE2D_DESC desc;
    acquiredTexture->GetDesc(&desc);

    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;

    ComPtr<ID3D11Texture2D> stagingTexture;
    hr = m_device->CreateTexture2D(&desc, nullptr, &stagingTexture);
    if (FAILED(hr)) {
        m_duplication->ReleaseFrame();
        return QPixmap();
    }

    // 拷贝纹理数据
    m_deviceContext->CopyResource(stagingTexture.Get(), acquiredTexture.Get());

    // 映射纹理数据
    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = m_deviceContext->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        m_duplication->ReleaseFrame();
        return QPixmap();
    }


    qDebug() << "纹理格式:" << desc.Format;
    qDebug() << "宽度:" << desc.Width << "高度:" << desc.Height;
    qDebug() << "RowPitch:" << mapped.RowPitch;

    QImage::Format imageFormat;
    switch (desc.Format) {
    case DXGI_FORMAT_B8G8R8A8_UNORM:
        qDebug() << "检测到BGRA格式";
        imageFormat = QImage::Format_ARGB32;
        break;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
        qDebug() << "检测到RGBA格式";
        imageFormat = QImage::Format_RGBA8888;
        break;
    default:
        qDebug() << "未知格式，使用默认ARGB32";
        imageFormat = QImage::Format_ARGB32;
    }

    // 转换为QImage然后转为QPixmap
    QImage image(desc.Width, desc.Height, QImage::Format_ARGB32);

    uchar* srcData = static_cast<uchar*>(mapped.pData);
    uchar* dstData = image.bits();

    for (UINT y = 0; y < desc.Height; ++y) {
        const uchar* srcLine = srcData + y * mapped.RowPitch;
        uchar* dstLine = dstData + y * image.bytesPerLine();

        for (UINT x = 0; x < desc.Width; ++x) {
            // BGRA 转 ARGB
            uchar b = srcLine[x * 4 + 0];
            uchar g = srcLine[x * 4 + 1];
            uchar r = srcLine[x * 4 + 2];
            uchar a = srcLine[x * 4 + 3];

            // 设置ARGB格式
            dstLine[x * 4 + 0] = a; // Alpha
            dstLine[x * 4 + 1] = r; // Red
            dstLine[x * 4 + 2] = g; // Green
            dstLine[x * 4 + 3] = b; // Blue
        }
    }

    if (image.isNull()) {
        qDebug() << "image create failed";
    }
    image.save("1.png");

    QPixmap pixmap = QPixmap::fromImage(image);

    //QPixmap pixmap = QPixmap::fromImage(image.copy());

    m_deviceContext->Unmap(stagingTexture.Get(), 0);
    m_duplication->ReleaseFrame();

    return pixmap;
}


bool DXGIScreenCapturer::initialize()
{
    // 创建D3D11设备
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        featureLevels, ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION, &m_device, nullptr, &m_deviceContext
        );

    if (FAILED(hr)) {
        return false;
    }

    // 获取DXGI设备
    ComPtr<IDXGIDevice> dxgiDevice;
    hr = m_device.As(&dxgiDevice);
    if (FAILED(hr)) {
        return false;
    }

    // 获取DXGI适配器
    ComPtr<IDXGIAdapter> dxgiAdapter;
    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    if (FAILED(hr)) {
        return false;
    }

    // 获取DXGI输出（显示器）
    ComPtr<IDXGIOutput> dxgiOutput;
    hr = dxgiAdapter->EnumOutputs(0, &dxgiOutput);
    if (FAILED(hr)) {
        return false;
    }

    // 获取输出1接口
    ComPtr<IDXGIOutput1> dxgiOutput1;
    hr = dxgiOutput.As(&dxgiOutput1);
    if (FAILED(hr)) {
        return false;
    }

    // 创建桌面复制接口
    hr = dxgiOutput1->DuplicateOutput(m_device.Get(), &m_duplication);
    if (FAILED(hr)) {
        return false;
    }

    m_initialized = true;
    return true;
}

void DXGIScreenCapturer::cleanup()
{
    if (m_duplication) {
        m_duplication->ReleaseFrame();
    }
    m_initialized = false;
}
