#include "recorder.h"
#include "audiocapture.h"
#include "videocapture.h"
extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/opt.h>

}
#include <QSize>
#include <QImage>
#include <QAudioFormat>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QDebug>

namespace adc{
class RecorderPrivate{
public:
    QMutex mutex;
    AudioCapture* audio;
    VideoCapture* video;

    bool opened = false;

    AVFormatContext* fmtCtx = nullptr;
    AVStream* videoStream = nullptr;
    AVStream* audioStream = nullptr;
    AVCodecContext* vencCtx = nullptr;
    AVCodecContext* aencCtx = nullptr;
    SwsContext* sws = nullptr;
    SwrContext* swr = nullptr;

    QAudioFormat audioFormat;

    AVFrame* videoFrame = nullptr;
    AVFrame* audioFrame = nullptr;


    int srcSampleRate = 0;
    int srcChannels = 0;

    QSize resolution;
    int fps;
    int interval;
    QString filename;
    WId target;

    int64_t videoPts;
    int64_t audioPts;
};


Recorder::Recorder(QObject *parent)
    : QObject{parent}
{

    d = new RecorderPrivate;

    d->fps = 30;
    d->resolution = {1920,1080};
    d->videoPts = d->audioPts = 0;

}

bool Recorder::init(){
    if(d->opened){
        return false;
    }
    avformat_alloc_output_context2(&d->fmtCtx, nullptr, nullptr, d->filename.toUtf8().data());
    if (!d->fmtCtx) return false;
    // Open output file
    if (!(d->fmtCtx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&d->fmtCtx->pb, d->filename.toUtf8().constData(), AVIO_FLAG_WRITE) < 0) {
            qDebug()<<"Could not open output file: " + d->filename;
            return false;
        }
    }
    int fps = 30;
    const AVCodec* vcodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!vcodec) { qWarning("No H264 encoder"); return false; }

    d->videoStream = avformat_new_stream(d->fmtCtx, nullptr);
    d->vencCtx = avcodec_alloc_context3(vcodec);
    d->vencCtx->width = d->resolution.width();
    d->vencCtx->height = d->resolution.height();
    d->vencCtx->time_base = AVRational{1, fps};
    d->vencCtx->framerate = AVRational{fps, 1};
    d->vencCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    d->vencCtx->gop_size = 12;
    d->vencCtx->max_b_frames = 2;

    av_opt_set(d->vencCtx->priv_data, "preset", "veryfast", 0);
    if (d->fmtCtx->oformat->flags & AVFMT_GLOBALHEADER) d->vencCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    if (avcodec_open2(d->vencCtx, vcodec, nullptr) < 0) { qWarning("Open vcodec failed"); return false; }
    avcodec_parameters_from_context(d->videoStream->codecpar, d->vencCtx);
    d->videoStream->time_base = d->vencCtx->time_base;




    // AUDIO encoder (AAC)
    const AVCodec* acodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!acodec) { qWarning("No AAC encoder"); return false; }
    d->audioStream = avformat_new_stream(d->fmtCtx, nullptr);

    int channels = 2;
    int sampleRate = 48000;
    d->aencCtx = avcodec_alloc_context3(acodec);
    d->aencCtx->sample_rate = sampleRate;
    av_channel_layout_default(&d->aencCtx->ch_layout, channels);
    d->aencCtx->sample_fmt = acodec->sample_fmts ? acodec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
    d->aencCtx->time_base = AVRational{1, d->aencCtx->sample_rate};


    if (d->fmtCtx->oformat->flags & AVFMT_GLOBALHEADER) d->aencCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    if (avcodec_open2(d->aencCtx, acodec, nullptr) < 0) { qWarning("Open acodec failed"); return false; }
    avcodec_parameters_from_context(d->audioStream->codecpar, d->aencCtx);
    d->audioStream->time_base = AVRational{1, d->aencCtx->sample_rate};


    if (avcodec_open2(d->aencCtx, acodec, nullptr) < 0)
        return false;

    avcodec_parameters_from_context(d->audioStream->codecpar, d->aencCtx);

    d->video = new VideoCapture(this);
    d->audio = new AudioCapture(this);

    if(!d->video->init()){
        return false;
    }
    if(!d->audio->init()){
        return false;
    }
    d->videoFrame = av_frame_alloc();
    d->audioFrame = av_frame_alloc();


    d->videoFrame->format = d->vencCtx->pix_fmt;
    d->videoFrame->width = d->resolution.width();
    d->videoFrame->height =  d->resolution.height();
    av_frame_get_buffer(d->videoFrame, 0);


    d->sws = sws_getContext(d->resolution.width(), d->resolution.height(), AV_PIX_FMT_BGRA,
                         d->resolution.width(), d->resolution.height(), AV_PIX_FMT_YUV420P,
                         SWS_BILINEAR, nullptr, nullptr, nullptr);




    d->audioFrame->format = d->aencCtx->sample_fmt;
    d->audioFrame->nb_samples = d->aencCtx->frame_size;
    d->audioFrame->sample_rate = d->aencCtx->sample_rate;
    av_channel_layout_copy(&d->audioFrame->ch_layout, &d->aencCtx->ch_layout);
    av_frame_get_buffer(d->audioFrame, 0);
    AVChannelLayout inLayout;
    av_channel_layout_default(&inLayout, channels);  //input PCM S16
    swr_alloc_set_opts2(
        &d->swr,
        &d->aencCtx->ch_layout, d->aencCtx->sample_fmt, d->aencCtx->sample_rate,  // output
        &inLayout, AV_SAMPLE_FMT_S16, sampleRate,               // input
        0, nullptr
        );
    swr_init(d->swr);

    d->opened = true;
    return true;
}



bool Recorder::start(){
    if(!d->opened){
        auto ret = this->init();
        if(!ret){
            return false;
        }
    }
    d->video->start();
    d->audio->start();
    return true;
}

bool Recorder::start(const QString& output,const QSize size,int fps){
    this->setOutput(output);
    this->setResolution(size);
    this->setFps(fps);
    return this->start();
}


void Recorder::stop(){
    d->video->stop();
    d->audio->stop();
}


void Recorder::setFps(int fps){
    d->fps = fps;
}

void Recorder::setResolution(const QSize& size){
    d->resolution = size;
}

void Recorder::setOutput(const QString& filename){
    d->filename = filename;
}

void Recorder::setTargetWindow(WId id){
    d->target = id;
    if(d->video){
        if(id==0){
            d->video->setMode(VideoCapture::Screen);
            d->video->setScreen(VideoCapture::availableMonitors()[0]);
        }else{
            d->video->setMode(VideoCapture::Window);
            d->video->setWindow((HWND)id);
        }
    }
}


/*void Recorder::pushVideoFrame(const uint8_t* rgba, int width, int height){
    QMutexLocker locker(&d->mutex);
    if (!d->opened) return;
    if (!d->sws){
        d->sws = sws_getContext(width, height, AV_PIX_FMT_RGBA, d->vencCtx->width, d->vencCtx->height, d->vencCtx->pix_fmt, SWS_BILINEAR, nullptr, nullptr, nullptr);
    }


    AVFrame* frame = av_frame_alloc();
    frame->format = d->vencCtx->pix_fmt;
    frame->width = d->vencCtx->width;
    frame->height = d->vencCtx->height;
    av_frame_get_buffer(frame, 0);


    // src frame
    AVFrame* src = av_frame_alloc();
    src->format = AV_PIX_FMT_RGBA;
    src->width = width; src->height = height;
    av_image_fill_arrays(src->data, src->linesize, rgba, AV_PIX_FMT_RGBA, width, height, 1);


    sws_scale(d->sws, src->data, src->linesize, 0, height, frame->data, frame->linesize);


    frame->pts = d->audioPts++;


    // encode
    AVPacket pkt; av_init_packet(&pkt); pkt.data = nullptr; pkt.size = 0;
    int got = 0;
    int ret = avcodec_send_frame(d->vencCtx, frame);
    while (ret >= 0){
        ret = avcodec_receive_packet(d->vencCtx, &pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        pkt.stream_index = d->videoStream->index;
        // convert pts (already ms) to stream timebase - here same base, so ok
        av_interleaved_write_frame(d->fmtCtx, &pkt);
        av_packet_unref(&pkt);
    }
    av_frame_free(&src);
    av_frame_free(&frame);
}*/


void Recorder::pushAudioFrame(const uint8_t* pcm, int bytes, int sampleRate, int channels){

    if(d->audioFrame==nullptr){
        d->srcSampleRate = sampleRate;
        d->srcChannels   = channels;

        AVChannelLayout srcLayout;
        av_channel_layout_default(&srcLayout, channels);

        d->audioFrame = av_frame_alloc();
        d->audioFrame->format         = d->aencCtx->sample_fmt;
        d->audioFrame->ch_layout =  d->aencCtx->ch_layout;
        d->audioFrame->sample_rate    =  d->aencCtx->sample_rate;
        d->audioFrame->nb_samples     =  d->aencCtx->frame_size;

        av_frame_get_buffer(d->audioFrame, 0);
    }

    // Convert audio data to AVFrame
    const uint8_t *srcData[1] = { pcm };
    //int srcSamples = audioData.size() / (2 * sizeof(int16_t)); // stereo S16

    if (d->srcSampleRate <= 0) {
        return ;
    }

    int dstSamples = av_rescale_rnd(swr_get_delay(d->swr, 44100) + d->srcSampleRate,44100, 44100, AV_ROUND_UP);

    if (dstSamples > d->audioFrame->nb_samples) {
        // Reallocate audio frame if needed
        av_frame_unref(d->audioFrame);
        d->audioFrame->nb_samples = dstSamples;
        if (av_frame_get_buffer(d->audioFrame, 0) < 0) {
            return ;
        }
    }

    int ret = swr_convert(d->swr, d->audioFrame->data, dstSamples,
                          srcData, d->srcSampleRate);
    if (ret < 0) {
        return ;
    }

   // d->audioFrame->pts = m_audioPts - m_pauseAudioPtsOffset;
   // m_audioPts += ret;
    d->audioFrame->pts = d->audioPts;
    d->audioPts += ret;
    d->audioFrame->nb_samples = ret;

    this->writeFrame(d->audioFrame, d->audioStream, d->aencCtx);
}


bool Recorder::writeFrame(AVFrame *frame, AVStream *stream, AVCodecContext *codecContext){
    int ret = avcodec_send_frame(codecContext, frame);
    if (ret < 0) {
        qWarning() << "Error sending frame to encoder" << ret;
    }

    AVPacket *pkt = av_packet_alloc();
    while (ret >= 0) {
        ret = avcodec_receive_packet(codecContext, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            qWarning() << "Error during encoding" << ret;
            break;
        }
        av_packet_rescale_ts(pkt, codecContext->time_base,stream->time_base);
        pkt->stream_index = stream->index;
        av_interleaved_write_frame(d->fmtCtx, pkt);
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);

    return true;

}

QImage Recorder::scaleToSizeWithBlackBorder(const QImage& src, const QSize& size){
    QImage dest(size, QImage::Format_RGB32);
    dest.fill(Qt::black);

    QImage scaled = src.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QPainter painter(&dest);
    painter.drawImage((size.width() - scaled.width()) / 2,
                      (size.height() - scaled.height()) / 2, scaled);

    return dest;
}


}
