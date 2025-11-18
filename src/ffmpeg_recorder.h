#ifndef FFMPEG_RECORDER_H
#define FFMPEG_RECORDER_H

#include <QObject>
#include <QImage>


extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
}


namespace adc{
class FFmpegRecorder : public QObject
{
    Q_OBJECT
public:
    FFmpegRecorder(QObject *parent = nullptr);
    ~FFmpegRecorder();

    bool open(const QString &filename, int width, int height, int fps);
    bool writeFrame(const QImage &img);
    void close();


private:
    AVFormatContext *m_fmtCtx = nullptr;
    AVStream *m_videoStream = nullptr;
    AVStream* m_audioStream = nullptr;
    AVCodecContext *m_codecCtx = nullptr;
    AVCodecContext* m_audioCodecCtx = nullptr;
    SwsContext *m_swsCtx = nullptr;
    SwrContext* m_swrCtx = nullptr;
    int m_frameIndex = 0;
    int m_width = 0;
    int m_height = 0;
    int m_fps = 0;
    bool m_opened = false;
};
}
#endif // FFMPEG_RECORDER_H
