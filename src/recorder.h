#ifndef RECORDER_H
#define RECORDER_H

#include <QObject>
#include <QIcon>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}



namespace adc{

class RecorderPrivate;
class Recorder : public QObject
{
    Q_OBJECT
public:
    explicit Recorder(QObject *parent = nullptr);
    ~Recorder();
    bool init();
    bool start();
    bool start(const QString& output,const QSize size,int fps);
    void stop();

    void pause();
    void resume();

    void setFps(int fps);
    void setResolution(const QSize& size);
    void setOutput(const QString& filename);
    void setTargetWindow(WId id);

    int mode();

    //void pushVideoFrame(const uint8_t* rgba, int width, int height);
    void pushVideoFrame(const QImage& image);
    void pushAudioFrame(const uint8_t* pcm, int bytes, int sampleRate, int channels);


    QString windowTitle() const ;


signals:
    void errorOccurred(const QString& message);
    void openOutput(const QString& path);


public slots:
    void writeTrailer();

private:
    bool initVideo();
    bool initAudio();
    bool writeFrame(AVFrame *frame, AVStream *stream, AVCodecContext *codecContext);
    QImage scaleToSizeWithBlackBorder(const QImage& src, const QSize& size);
    void cleanup();

    int64_t currentTimestampUs();
    int64_t currentVideoTimestampUs();
    int64_t currentAudioTimestampUs();

    inline int64_t nowUs() {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
            ).count();
    }
    

private:
    RecorderPrivate* d;
};

}

#endif // RECORDER_H
