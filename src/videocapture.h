#ifndef VIDEOCAPTURE_H
#define VIDEOCAPTURE_H

#include <QThread>

#include <windows.h>

namespace adc{
class Recorder;
class VideoCapturePrivate;
//class ID3D11Texture2D;
class VideoCapture : public QThread
{
    Q_OBJECT
public:
    enum Mode{
        Screen,
        Region,
        Window
    };


    explicit VideoCapture(Recorder* instance);
    void setMode(Mode mode);
    void setScreen(HMONITOR monitor);
    void setWindow(HWND hwnd);
    void setRegion(const QRect& rc);
    bool init();
    bool start();
    void stop();
    ~VideoCapture();
    QImage captureFrame();

    static std::vector<HMONITOR> availableMonitors();

protected:
    void run() override;

signals:

private:
    bool createD3DDevice();
    bool createCaptureItem(HWND hwnd);
    bool createCaptureItem(HMONITOR monitor);
    bool createFramePool();
    bool createCaptureSession();


    //QImage textureToImage(ID3D11Texture2D* texture);

private:
    VideoCapturePrivate* d;
};

}

#endif // VIDEOCAPTURE_H
