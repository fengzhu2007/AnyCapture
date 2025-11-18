#include "ffmpeg_muxer.h"
#include <QDebug>


extern "C" {
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}
namespace adc{
FFmpegMuxer::FFmpegMuxer(QObject *parent)
    : QObject{parent}
{

}


FFmpegMuxer::~FFmpegMuxer() {
    stopAndClose();
}

bool FFmpegMuxer::init(const QString& filename, int width, int height, int fps, int sampleRate, int channels) {


    m_videoPts = 0;
    m_audioPts = 0;
    return true;
}


bool FFmpegMuxer::pushVideoFrame(AVFrame* frame) {
    QMutexLocker locker(&mtx);
    if (!fmtCtx || !videoCodecCtx) return false;


    frame->pts = m_videoPts++;
    int ret = avcodec_send_frame(videoCodecCtx, frame);
    if (ret < 0) return false;


    AVPacket pkt;
    av_init_packet(&pkt);
    while (ret >= 0) {
        ret = avcodec_receive_packet(videoCodecCtx, &pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) {
            av_packet_unref(&pkt);
            return false;
        }
        av_packet_rescale_ts(&pkt, videoCodecCtx->time_base, videoStream->time_base);
        pkt.stream_index = videoStream->index;
        av_interleaved_write_frame(fmtCtx, &pkt);
        av_packet_unref(&pkt);
    }
    return true;
}


bool FFmpegMuxer::pushAudioPacket(AVPacket* pkt) {
    QMutexLocker locker(&mtx);
    if (!fmtCtx) return false;
    // assume pkt already has proper stream index/pts from capture thread
    av_interleaved_write_frame(fmtCtx, pkt);
    return true;
}

void FFmpegMuxer::writeAudioFrame(AVFrame* frame) {
    frame->pts = av_rescale_q(m_audioPts++, {1, m_audioCodecCtx->sample_rate}, m_audioCodecCtx->time_base);
    encodeAudioFrame(frame);
}


void FFmpegMuxer::encodeAudioFrame(AVFrame *frame)
{
    if (!m_audioCodecCtx || !m_audioStream || !m_formatCtx)
        return;

    AVPacket *pkt = av_packet_alloc();
    if (!pkt)
        return;

    int ret = avcodec_send_frame(m_audioCodecCtx, frame);
    if (ret < 0) {
        qWarning() << "avcodec_send_frame(audio) failed:" << ret;
        av_packet_free(&pkt);
        return;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(m_audioCodecCtx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        else if (ret < 0) {
            qWarning() << "avcodec_receive_packet(audio) failed:" << ret;
            break;
        }

        pkt->stream_index = m_audioStream->index;
        av_packet_rescale_ts(pkt, m_audioCodecCtx->time_base, m_audioStream->time_base);

        ret = av_interleaved_write_frame(m_formatCtx, pkt);
        if (ret < 0)
            qWarning() << "Error writing audio frame:" << ret;

        av_packet_unref(pkt);
    }

    av_packet_free(&pkt);
}

void FFmpegMuxer::stopAndClose() {
    QMutexLocker locker(&mtx);
    if (!fmtCtx) return;


    // flush video encoder
    if (videoCodecCtx) {
        avcodec_send_frame(videoCodecCtx, nullptr);
        AVPacket pkt; av_init_packet(&pkt);
        while (avcodec_receive_packet(videoCodecCtx, &pkt) == 0) {
            av_packet_rescale_ts(&pkt, videoCodecCtx->time_base, videoStream->time_base);
            pkt.stream_index = videoStream->index;
            av_interleaved_write_frame(fmtCtx, &pkt);
            av_packet_unref(&pkt);
        }
    }


    av_write_trailer(fmtCtx);
    if (!(fmtCtx->oformat->flags & AVFMT_NOFILE)) avio_closep(&fmtCtx->pb);


    if (swsCtx) sws_freeContext(swsCtx);
    if (swrCtx) swr_free(&swrCtx);
    if (videoCodecCtx) avcodec_free_context(&videoCodecCtx);
    if (audioCodecCtx) avcodec_free_context(&audioCodecCtx);
    avformat_free_context(fmtCtx);
    fmtCtx = nullptr;
}

}
