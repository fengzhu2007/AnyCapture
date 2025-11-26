#ifndef SCREEN_RECORDER_H
#define SCREEN_RECORDER_H

#include <QThread>
#include <QTimer>
#include <QSize>
#include <QAudioInput>
#include <QAudioDeviceInfo>
#include <QBuffer>
#include <QQueue>
#include <QIcon>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavdevice/avdevice.h>
#include <libavutil/imgutils.h>
}

#include "windowcapture.h"

namespace adc{
class ScreenRecorder : public QThread
{
    Q_OBJECT
public:
    explicit ScreenRecorder(QObject *parent = nullptr);

    ~ScreenRecorder();

    void setTargetWindow(WId id);
    bool startRecording(const QString &outputFile,
                        const QSize &resolution = QSize(1920, 1080),
                        int fps = 30);
    void stopRecording();
    void pauseRecording();
    void resumeRecording();
    QPixmap captureImage();
    QPixmap dxgi();
    QPixmap winDxgi();



    bool isRecording() const { return m_isRecording; }
    bool isPaused() const { return m_isPaused; }

protected:
    virtual void run() override;

signals:
    void recordingStarted();
    void recordingStopped();
    void recordingPaused();
    void recordingResumed();
    void errorOccurred(const QString &error);

private slots:
    void captureFrame();
    void onAudioDataReady();
    void onStopped();

private:
    bool initVideo();
    bool initAudio();
    void cleanup();
    void processAudioBuffer();

    bool encodeVideoFrame(const QImage &image);
    bool encodeAudioFrame(const QByteArray &audioData);
    bool writeFrame(AVFrame *frame, AVStream *stream, AVCodecContext *codecContext);
    QImage scaleToSizeWithBlackBorder(const QImage& src, const QSize& size);

    QElapsedTimer m_videoTimer;
    std::atomic<bool> m_isRecording;
    std::atomic<bool> m_isPaused;
    WId m_target;
    //bool m_isPaused;

    WindowCapture* m_capture;

    // Video
    QSize m_resolution;
    int m_fps;
    int m_interval;
    AVPixelFormat m_pixelFormat;

    // Audio
    QAudioFormat m_audioFormat;
    QAudioInput *m_audioInput;
    QBuffer m_audioBuffer;
    QQueue<QByteArray> m_audioQueue;

    // FFmpeg
    AVFormatContext *m_formatContext;
    AVCodecContext *m_videoCodecContext;
    AVCodecContext *m_audioCodecContext;
    AVStream *m_videoStream;
    AVStream *m_audioStream;
    SwsContext *m_swsContext;
    SwrContext *m_swrContext;

    //AVFrame *m_videoFrame;
    AVFrame *m_audioFrame;
    AVPacket *m_packet;

    int64_t m_videoPts;
    int64_t m_audioPts;
    QString m_outputFile;

    int64_t m_pauseVideoPtsOffset;
    int64_t m_pauseAudioPtsOffset;
    int64_t m_pauseStartVideoPts;
    int64_t m_pauseStartAudioPts;

};
}

#endif // SCREEN_RECORDER_H
