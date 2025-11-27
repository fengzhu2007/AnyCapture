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

    int64_t videoPts = 0;
    int64_t audioPts = 0;
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
   

    auto ret = this->initVideo();
    if (!ret) {
        return false;
    }

    /*ret = this->initAudio();
    if (!ret) {
        return false;
    }*/

    if (avformat_write_header(d->fmtCtx, nullptr) < 0) {
        qWarning() << "Error occurred when writing header";
        return false;
    }




    qDebug() << "init:" << d->videoStream->time_base.num << d->videoStream->time_base.den;

    d->video = new VideoCapture(this);
    d->audio = new AudioCapture(this);


    connect(d->video, &QThread::finished, this, &Recorder::writeTrailer);



    if(!d->video->init()){
        return false;
    }

    if(!d->audio->init()){
        return false;
    }

    d->opened = true;
    return true;
}



bool Recorder::initVideo() {
    int fps = 30;
    const AVCodec* vcodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!vcodec) { qWarning("No H264 encoder"); return false; }

    d->videoStream = avformat_new_stream(d->fmtCtx, nullptr);
    if (!d->videoStream) {
        qWarning() << "Failed to create new stream";
        return false;
    }

    d->vencCtx = avcodec_alloc_context3(vcodec);
    if (!d->vencCtx) {
        emit errorOccurred("Could not allocate video codec context");
        return false;
    }

    d->vencCtx->width = d->resolution.width();
    d->vencCtx->height = d->resolution.height();
    d->vencCtx->time_base = AVRational{ 1, fps };
    d->vencCtx->framerate = AVRational{ fps, 1 };
    d->vencCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    d->vencCtx->gop_size = 12;
    d->vencCtx->max_b_frames = 2;

    av_opt_set(d->vencCtx->priv_data, "preset", "veryfast", 0);
    if (d->fmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
        d->vencCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(d->vencCtx, vcodec, nullptr) < 0) {
        qWarning("Open vcodec failed"); return false;
    }

    if (avcodec_parameters_from_context(d->videoStream->codecpar, d->vencCtx) < 0) {
        qWarning() << "Failed to copy codec params to stream";
        return false;
    }

    d->videoStream->time_base = d->vencCtx->time_base;

    /*d->videoFrame = av_frame_alloc();
    d->videoFrame->format = d->vencCtx->pix_fmt;
    d->videoFrame->width = d->resolution.width();
    d->videoFrame->height = d->resolution.height();
    av_frame_get_buffer(d->videoFrame, 0);*/


    d->sws = sws_getContext(d->resolution.width(), d->resolution.height(), AV_PIX_FMT_BGRA,
        d->resolution.width(), d->resolution.height(), AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr);


    return true;

}
bool Recorder::initAudio() {
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
    d->aencCtx->time_base = AVRational{ 1, d->aencCtx->sample_rate };


    if (d->fmtCtx->oformat->flags & AVFMT_GLOBALHEADER) d->aencCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    if (avcodec_open2(d->aencCtx, acodec, nullptr) < 0) {
        qWarning("Open acodec failed"); return false;
    }
    avcodec_parameters_from_context(d->audioStream->codecpar, d->aencCtx);
    d->audioStream->time_base = AVRational{ 1, d->aencCtx->sample_rate };


    avcodec_parameters_from_context(d->audioStream->codecpar, d->aencCtx);


    d->audioFrame = av_frame_alloc();

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
    return true;
}

bool Recorder::start(){
    if(!d->opened){
        auto ret = this->init();
        if(!ret){
            qDebug()<<"recorder init failed";
            return false;
        }
    }
    auto ret = d->video->startRecording();
    if(!ret){
        qDebug()<<"video start failed";
        return false;
    }
    /*ret = d->audio->startRecording();
    if(!ret){
        qDebug()<<"audio start failed";
        return false;
    }*/
    qDebug()<<"recorder start ok";
    return true;
}

bool Recorder::start(const QString& output,const QSize size,int fps){
    this->setOutput(output);
    this->setResolution(size);
    this->setFps(fps);
    return this->start();
}


void Recorder::stop(){
    d->video->stopRecording();
    d->audio->stopRecording();
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


void Recorder::pushVideoFrame(const QImage& image){
    QMutexLocker locker(&d->mutex);
    //qDebug()<<"image:"<<image;

    QImage src = image;
    if (src.format() != QImage::Format_ARGB32 && src.format() != QImage::Format_RGBA8888)
        src = src.convertToFormat(QImage::Format_ARGB32);

    // create AVFrame (BGRA)
    AVFrame *srcFrame = av_frame_alloc();
    srcFrame->format = AV_PIX_FMT_BGRA;
    srcFrame->width = d->resolution.width();
    srcFrame->height = d->resolution.height();
    av_image_alloc(srcFrame->data, srcFrame->linesize, d->resolution.width(), d->resolution.height(), AV_PIX_FMT_BGRA, 1);




    qDebug() << "size:" << d->resolution<<image;

    for (int y = 0; y < d->resolution.height(); ++y) {
        const uchar *scanLine = src.constScanLine(y);
        memcpy(srcFrame->data[0] + y * srcFrame->linesize[0], scanLine, d->resolution.width() * 4);
    }

    AVFrame *yuvFrame = av_frame_alloc();
    yuvFrame->format = AV_PIX_FMT_YUV420P;
    yuvFrame->width = d->resolution.width();
    yuvFrame->height = d->resolution.height();
    av_frame_get_buffer(yuvFrame, 32);


    sws_scale(d->sws, srcFrame->data, srcFrame->linesize, 0, d->resolution.height(), yuvFrame->data, yuvFrame->linesize);


    yuvFrame->pts = d->videoPts++;

    //qDebug()<<"write video frame";
    int ret = this->writeFrame(yuvFrame,d->videoStream,d->vencCtx);

    av_freep(&srcFrame->data[0]);
    av_frame_free(&srcFrame);
    av_frame_free(&yuvFrame);
}


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

    int dstSamples = av_rescale_rnd(swr_get_delay(d->swr, 48000) + d->srcSampleRate,48000, 48000, AV_ROUND_UP);

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
       // qDebug() << "output:" << codecContext->time_base.num << codecContext->time_base.den<<d->videoFrame->time_base.num<<d->videoFrame->time_base.den;
        qDebug() << "time base" << stream->time_base.num << stream->time_base.den << stream->index;
        av_packet_rescale_ts(pkt, codecContext->time_base, stream->time_base);
        pkt->stream_index = stream->index;
        av_interleaved_write_frame(d->fmtCtx, pkt);
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);

    return true;

}


void Recorder::writeTrailer() {
    if (d->vencCtx) {
        avcodec_send_frame(d->vencCtx, nullptr);
        AVPacket* packet = av_packet_alloc();
        while (true) {
            int ret = avcodec_receive_packet(d->vencCtx, packet);
            if (ret == AVERROR_EOF) {
                break;
            }
            else if (ret < 0 && ret != AVERROR(EAGAIN)) {
                qWarning() << "Error receiving final video packets:" << ret;
                break;
            }
            else if (ret >= 0) {
                av_packet_rescale_ts(packet, d->vencCtx->time_base, d->videoStream->time_base);
                packet->stream_index = d->videoStream->index;

                if (av_interleaved_write_frame(d->fmtCtx, packet) < 0) {
                    qWarning() << "Error writing final video packet";
                }
                av_packet_unref(packet);
            }
        }
    }

    // Write trailer
    if (d->fmtCtx && d->fmtCtx->pb) {
        av_write_trailer(d->fmtCtx);
    }
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
