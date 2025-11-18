#include "ffmpeg_recorder.h"
#include <QDebug>
extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/opt.h>

}

namespace adc{

FFmpegRecorder::FFmpegRecorder(QObject *parent) : QObject(parent) {
    av_log_set_level(AV_LOG_QUIET);
}

FFmpegRecorder::~FFmpegRecorder() {
    close();
}

bool FFmpegRecorder::open(const QString &filename, int width, int height, int fps) {
    if (m_opened) return false;

    m_width = width;
    m_height = height;

    if(m_width % 2==1){
        m_width -= 1;
    }
    if(m_height % 2==1){
        m_height -= 1;
    }

    m_fps = fps;


    avformat_alloc_output_context2(&m_fmtCtx, nullptr, nullptr, filename.toStdString().c_str());
    if (!m_fmtCtx) {
        qWarning() << "Could not allocate output context";
        return false;
    }








    //const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    const AVCodec *codec = avcodec_find_encoder_by_name("libx264");
    if (!codec)
        codec = avcodec_find_encoder(AV_CODEC_ID_H264); // fallback
    if (!codec) {
        qWarning() << "H.264 encoder not found";
        return false;
    }


    m_videoStream = avformat_new_stream(m_fmtCtx, nullptr);
    if (!m_videoStream) {
        qWarning() << "Failed to create new stream";
        return false;
    }
    m_codecCtx = avcodec_alloc_context3(codec);
    m_codecCtx->width = m_width;
    m_codecCtx->height = m_height;
    m_codecCtx->time_base = AVRational{1, m_fps};
    m_codecCtx->framerate = AVRational{m_fps, 1};
    m_codecCtx->gop_size = 12;
    m_codecCtx->max_b_frames = 2;
    m_codecCtx->pix_fmt = AV_PIX_FMT_YUV420P;

    av_opt_set(m_codecCtx->priv_data, "preset", "veryfast", 0);


    if (m_fmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
        m_codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    int ret = avcodec_open2(m_codecCtx, codec, nullptr);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        qWarning() << "Failed to open codec:"<<errbuf;
        return false;
    }


    if (avcodec_parameters_from_context(m_videoStream->codecpar, m_codecCtx) < 0) {
        qWarning() << "Failed to copy codec params to stream";
        return false;
    }

    m_videoStream->time_base = m_codecCtx->time_base;


    if (!(m_fmtCtx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&m_fmtCtx->pb, filename.toStdString().c_str(), AVIO_FLAG_WRITE) < 0) {
            qWarning() << "Could not open output file";
            return false;
        }
    }


    if (avformat_write_header(m_fmtCtx, nullptr) < 0) {
        qWarning() << "Error occurred when writing header";
        return false;
    }



    m_swsCtx = sws_getContext(m_width, m_height, AV_PIX_FMT_BGRA,
                              m_width, m_height, AV_PIX_FMT_YUV420P,
                              SWS_BILINEAR, nullptr, nullptr, nullptr);


    m_frameIndex = 0;
    m_opened = true;
    return true;
}


bool FFmpegRecorder::writeFrame(const QImage &img) {
    if (!m_opened) return false;
    if (img.isNull()) return false;



    QImage src = img;
    if (src.format() != QImage::Format_ARGB32 && src.format() != QImage::Format_RGBA8888)
        src = src.convertToFormat(QImage::Format_ARGB32);


    // create AVFrame (BGRA)
    AVFrame *srcFrame = av_frame_alloc();
    srcFrame->format = AV_PIX_FMT_BGRA;
    srcFrame->width = m_width;
    srcFrame->height = m_height;
    av_image_alloc(srcFrame->data, srcFrame->linesize, m_width, m_height, AV_PIX_FMT_BGRA, 1);



    for (int y = 0; y < m_height; ++y) {
        const uchar *scanLine = src.constScanLine(y);
        memcpy(srcFrame->data[0] + y * srcFrame->linesize[0], scanLine, m_width * 4);
    }

    AVFrame *yuvFrame = av_frame_alloc();
    yuvFrame->format = AV_PIX_FMT_YUV420P;
    yuvFrame->width = m_width;
    yuvFrame->height = m_height;
    av_frame_get_buffer(yuvFrame, 32);


    sws_scale(m_swsCtx, srcFrame->data, srcFrame->linesize, 0, m_height, yuvFrame->data, yuvFrame->linesize);


    yuvFrame->pts = m_frameIndex++;



    int ret = avcodec_send_frame(m_codecCtx, yuvFrame);
    if (ret < 0) {
        qWarning() << "Error sending frame to encoder" << ret;
    }


    AVPacket *pkt = av_packet_alloc();
    while (ret >= 0) {
        ret = avcodec_receive_packet(m_codecCtx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            qWarning() << "Error during encoding" << ret;
            break;
        }


        av_packet_rescale_ts(pkt, m_codecCtx->time_base, m_videoStream->time_base);
        pkt->stream_index = m_videoStream->index;


        av_interleaved_write_frame(m_fmtCtx, pkt);
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);


    av_freep(&srcFrame->data[0]);
    av_frame_free(&srcFrame);
    av_frame_free(&yuvFrame);


    return true;
}

void FFmpegRecorder::close() {
    if (!m_opened) return;


    // flush encoder
    avcodec_send_frame(m_codecCtx, nullptr);
    AVPacket *pkt = av_packet_alloc();
    while (avcodec_receive_packet(m_codecCtx, pkt) == 0) {
        av_packet_rescale_ts(pkt, m_codecCtx->time_base, m_videoStream->time_base);
        pkt->stream_index = m_videoStream->index;
        av_interleaved_write_frame(m_fmtCtx, pkt);
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);


    av_write_trailer(m_fmtCtx);


    if (!(m_fmtCtx->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&m_fmtCtx->pb);
    }


    sws_freeContext(m_swsCtx);
    m_swsCtx = nullptr;


    avcodec_free_context(&m_codecCtx);
    avformat_free_context(m_fmtCtx);


    m_fmtCtx = nullptr;
    m_opened = false;
}

}
