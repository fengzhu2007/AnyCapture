#ifndef FFMPEG_MUXER_H
#define FFMPEG_MUXER_H

#include <QObject>
#include <QMutex>
#include <QString>


extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

namespace adc{
class FFmpegMuxer : public QObject
{
    Q_OBJECT
public:
    explicit FFmpegMuxer(QObject *parent = nullptr);
    ~FFmpegMuxer();

    bool init(const QString& filename, int width, int height, int fps, int sampleRate, int channels);
    bool pushVideoFrame(AVFrame* frame);
    bool pushAudioPacket(AVPacket* pkt);
    void stopAndClose();

    void writeAudioFrame(AVFrame *frame);
    void encodeAudioFrame(AVFrame *frame);

private:
    QMutex mtx;
    AVFormatContext* fmtCtx = nullptr;
    AVStream* videoStream = nullptr;
    AVStream* m_audioStream = nullptr;
    AVCodecContext* videoCodecCtx = nullptr;
    AVCodecContext* m_audioCodecCtx = nullptr;
    SwsContext* swsCtx = nullptr;
    SwrContext* swrCtx = nullptr;
    int64_t m_videoPts = 0;
    int64_t m_audioPts = 0;
};
}
#endif // FFMPEG_MUXER_H
