#ifndef AUDIO_CAPTURE_WORKER_H
#define AUDIO_CAPTURE_WORKER_H

#include <QObject>
#include <QThread>
#include <QDebug>
#include <atomic>
#include "ffmpeg_muxer.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}
namespace adc{
class FFmpegMuxer;
class AudioCaptureWorker : public QThread
{
public:
    explicit AudioCaptureWorker(FFmpegMuxer* muxer, const QString& deviceName,QObject *parent = nullptr);
    void stop() { m_running.store(false); }

protected:
    void run() override {
        AVFormatContext* fmtCtx = nullptr;
        AVInputFormat* inputFmt = av_find_input_format("dshow");
        if (avformat_open_input(&fmtCtx, m_deviceName.toUtf8().constData(), inputFmt, nullptr) < 0) {
            qWarning() << "open audio device failed:" << m_deviceName;
            return;
        }

        AVCodec* codec = nullptr;
        AVStream* stream = fmtCtx->streams[0];
        codec = avcodec_find_decoder(stream->codecpar->codec_id);
        AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(codecCtx, stream->codecpar);
        avcodec_open2(codecCtx, codec, nullptr);

        AVPacket* pkt = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();

        m_running.store(true);
        while (m_running.load()) {
            if (av_read_frame(fmtCtx, pkt) < 0)
                continue;

            if (avcodec_send_packet(codecCtx, pkt) == 0) {
                while (avcodec_receive_frame(codecCtx, frame) == 0) {
                    m_muxer->writeAudioFrame(frame);
                }
            }
            av_packet_unref(pkt);
        }

        av_frame_free(&frame);
        av_packet_free(&pkt);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
    }

private:
    FFmpegMuxer* m_muxer;
    QString m_deviceName;
    std::atomic<bool> m_running;
};
}
#endif // AUDIO_CAPTURE_WORKER_H
