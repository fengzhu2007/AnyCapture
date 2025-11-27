#include "screen_recorder.h"
#include <QScreen>
#include <QGuiApplication>
#include <QWindow>
#include <QDebug>
#include <QAudioDeviceInfo>
#include <QElapsedTimer>
#include <QPainter>
extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/opt.h>

}

#include <d3d11.h>
#include <dxgi1_2.h>

namespace adc{
ScreenRecorder::ScreenRecorder(QObject *parent)
    : QThread{parent}
    , m_isRecording(false)
    , m_isPaused(false)
    , m_target(0)
    , m_resolution(1920, 1080)
    , m_fps(30)
    , m_pixelFormat(AV_PIX_FMT_YUV420P)
    , m_audioInput(nullptr)
    , m_formatContext(nullptr)
    , m_videoCodecContext(nullptr)
    , m_audioCodecContext(nullptr)
    , m_videoStream(nullptr)
    , m_audioStream(nullptr)
    , m_swsContext(nullptr)
    , m_swrContext(nullptr)
    //, m_videoFrame(nullptr)
    , m_audioFrame(nullptr)
    , m_packet(nullptr)
    , m_videoPts(0)
    , m_audioPts(0)
    , m_pauseVideoPtsOffset(0)
    , m_pauseAudioPtsOffset(0)
    , m_pauseStartVideoPts(0)
    , m_pauseStartAudioPts(0)
{

    //connect(m_videoTimer, &QTimer::timeout, this, &ScreenRecorder::captureFrame);
    m_capture = new WindowCapture(this);
    connect(this,&QThread::finished,this,&ScreenRecorder::onStopped);
}

ScreenRecorder::~ScreenRecorder()
{
    //stopRecording();
    if(m_isRecording){
        this->stopRecording();
        this->wait();
        this->quit();
    }
}

void ScreenRecorder::run(){
    //qDebug()<<"run";
    this->m_videoTimer.start();
    this->captureFrame();
    while(m_isRecording){
        if(!m_isPaused){
            int msec = m_videoTimer.elapsed();
            //qDebug()<<"msec:"<<msec<<m_interval;
            if(msec>=m_interval){
                m_videoTimer.restart();
                this->captureFrame();
            }
        }
    }
}

void ScreenRecorder::setTargetWindow(WId id){
    this->m_target = id;
}

bool ScreenRecorder::startRecording(const QString &outputFile, const QSize &resolution, int fps)
{
    if (m_isRecording) {
        return true;
    }
    m_resolution = resolution;
    m_fps = fps;
    m_interval = 1000 / fps;
    m_outputFile = outputFile;
    m_isPaused = false;
    m_pauseVideoPtsOffset = 0;
    m_pauseAudioPtsOffset = 0;
    avformat_alloc_output_context2(&m_formatContext, nullptr, nullptr, m_outputFile.toStdString().c_str());
    if (!m_formatContext) {
        qWarning() << "Could not allocate output context";
        return false;
    }
    // Open output file
    if (!(m_formatContext->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&m_formatContext->pb, m_outputFile.toUtf8().constData(), AVIO_FLAG_WRITE) < 0) {
            emit errorOccurred("Could not open output file: " + m_outputFile);
            return false;
        }
    }
    // Allocate packet
    m_packet = av_packet_alloc();
    if (!m_packet) {
        emit errorOccurred("Could not allocate packet");
        return false;
    }

    //init window capture
    if(!m_capture->initialize()){
        emit errorOccurred("Window capture initialize failed");
        return false;
    }


    // Initialize FFmpeg components
    if (!initVideo() ) {
        cleanup();
        return false;
    }
    m_isRecording.store(true);
    emit recordingStarted();
    qDebug() << "Recording started:" << m_outputFile;
    m_capture->startCapture((HWND)this->m_target);
    this->start();
    return true;
}

void ScreenRecorder::pauseRecording()
{
    if (!m_isRecording || m_isPaused) {
        return;
    }

    //m_videoTimer->stop();
    m_isPaused.store(true);

    m_pauseStartVideoPts = m_videoPts;
    m_pauseStartAudioPts = m_audioPts;

    if (m_audioInput) {
        m_audioInput->suspend();
    }

    emit recordingPaused();
    qDebug() << "Recording paused";
}

void ScreenRecorder::resumeRecording()
{
    if (!m_isRecording || !m_isPaused) {
        return;
    }

    //m_videoTimer->start(1000 / m_fps);
    m_isPaused.store(false);

    m_pauseVideoPtsOffset += (m_videoPts - m_pauseStartVideoPts);
    m_pauseAudioPtsOffset += (m_audioPts - m_pauseStartAudioPts);

    if (m_audioInput) {
        m_audioInput->resume();
    }

    emit recordingResumed();
    qDebug() << "Recording resumed";
}


void ScreenRecorder::stopRecording()
{
    if (!m_isRecording) {
        return;
    }
    //m_videoTimer->stop();
    m_capture->stopCapture();
    m_isRecording.store(false);
}

void ScreenRecorder::onStopped(){
    //stop capture
    qDebug() << "Final video pts:" << m_videoPts << "Expected frames:" << m_videoPts;
    if (m_videoCodecContext) {
        avcodec_send_frame(m_videoCodecContext, nullptr);
        AVPacket *m_packet = av_packet_alloc();
        while (true) {
            int ret = avcodec_receive_packet(m_videoCodecContext, m_packet);
            if (ret == AVERROR_EOF) {
                break;
            } else if (ret < 0 && ret != AVERROR(EAGAIN)) {
                qWarning() << "Error receiving final video packets:" << ret;
                break;
            } else if (ret >= 0) {
                av_packet_rescale_ts(m_packet, m_videoCodecContext->time_base, m_videoStream->time_base);
                m_packet->stream_index = m_videoStream->index;

                if (av_interleaved_write_frame(m_formatContext, m_packet) < 0) {
                    qWarning() << "Error writing final video packet";
                }
                av_packet_unref(m_packet);
            }
        }
    }

    // Write trailer
    if (m_formatContext && m_formatContext->pb) {
        av_write_trailer(m_formatContext);
    }
    m_isPaused.store(false);
    emit recordingStopped();
    qDebug() << "Recording stopped. Total video frames processed:" << m_videoPts;

    cleanup();
}

bool ScreenRecorder::initVideo()
{
    // Find video codec using new API
    const AVCodec *videoCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!videoCodec) {
        emit errorOccurred("Could not find H.264 encoder");
        return false;
    }
    m_videoStream = avformat_new_stream(m_formatContext, nullptr);
    if (!m_videoStream) {
        qWarning() << "Failed to create new stream";
        return false;
    }


    m_videoCodecContext = avcodec_alloc_context3(videoCodec);
    if (!m_videoCodecContext) {
        emit errorOccurred("Could not allocate video codec context");
        return false;
    }

    // Configure video codec context
    m_videoCodecContext->width = m_resolution.width();
    m_videoCodecContext->height = m_resolution.height();
    m_videoCodecContext->time_base = AVRational{1, m_fps};
    m_videoCodecContext->framerate = AVRational{m_fps, 1};
    m_videoCodecContext->pix_fmt = m_pixelFormat;

    m_videoCodecContext->gop_size = 12;  //
    m_videoCodecContext->max_b_frames = 2; //
    //m_videoCodecContext->bit_rate = 4000000; // 4 Mbps

    av_opt_set(m_videoCodecContext->priv_data, "preset", "veryfast", 0);

    if (m_formatContext->oformat->flags & AVFMT_GLOBALHEADER)
        m_videoCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    int ret = avcodec_open2(m_videoCodecContext, videoCodec, nullptr);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        qWarning() << "Failed to open codec:"<<errbuf;
        return false;
    }


    if (avcodec_parameters_from_context(m_videoStream->codecpar, m_videoCodecContext) < 0) {
        qWarning() << "Failed to copy codec params to stream";
        return false;
    }

    m_videoStream->time_base = m_videoCodecContext->time_base;


    if (!(m_formatContext->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&m_formatContext->pb, m_outputFile.toStdString().c_str(), AVIO_FLAG_WRITE) < 0) {
            qWarning() << "Could not open output file";
            return false;
        }
    }


    if (avformat_write_header(m_formatContext, nullptr) < 0) {
        qWarning() << "Error occurred when writing header";
        return false;
    }



    m_swsContext = sws_getContext(m_resolution.width(), m_resolution.height(), AV_PIX_FMT_BGRA,
                              m_resolution.width(), m_resolution.height(), AV_PIX_FMT_YUV420P,
                              SWS_BILINEAR, nullptr, nullptr, nullptr);





    return true;
}


void ScreenRecorder::captureFrame()
{
    if (m_isPaused) {
        return;
    }
    auto image = m_capture->captureFrame()/*.scaled(m_resolution, Qt::KeepAspectRatio, Qt::SmoothTransformation)*/;
    if(image.isNull()){
        return ;
    }
    if (!encodeVideoFrame(scaleToSizeWithBlackBorder(image,m_resolution))) {
        emit errorOccurred("Failed to encode video frame");
    }
}


QImage ScreenRecorder::scaleToSizeWithBlackBorder(const QImage& src, const QSize& size)
{
    QImage dest(size, QImage::Format_RGB32);
    dest.fill(Qt::black);

    QImage scaled = src.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QPainter painter(&dest);
    painter.drawImage((size.width() - scaled.width()) / 2,
                      (size.height() - scaled.height()) / 2, scaled);

    return dest;
}

bool ScreenRecorder::encodeVideoFrame(const QImage &image)
{
    QImage src = image;
    if (src.format() != QImage::Format_ARGB32 && src.format() != QImage::Format_RGBA8888)
        src = src.convertToFormat(QImage::Format_ARGB32);

    // create AVFrame (BGRA)
    AVFrame *srcFrame = av_frame_alloc();
    srcFrame->format = AV_PIX_FMT_BGRA;
    srcFrame->width = m_resolution.width();
    srcFrame->height = m_resolution.height();
    av_image_alloc(srcFrame->data, srcFrame->linesize, m_resolution.width(), m_resolution.height(), AV_PIX_FMT_BGRA, 1);



    for (int y = 0; y < m_resolution.height(); ++y) {
        const uchar *scanLine = src.constScanLine(y);
        memcpy(srcFrame->data[0] + y * srcFrame->linesize[0], scanLine, m_resolution.width() * 4);
    }

    AVFrame *yuvFrame = av_frame_alloc();
    yuvFrame->format = AV_PIX_FMT_YUV420P;
    yuvFrame->width = m_resolution.width();
    yuvFrame->height = m_resolution.height();
    av_frame_get_buffer(yuvFrame, 32);


    sws_scale(m_swsContext, srcFrame->data, srcFrame->linesize, 0, m_resolution.height(), yuvFrame->data, yuvFrame->linesize);


    yuvFrame->pts = m_videoPts++;





    int ret = this->writeFrame(yuvFrame,m_videoStream,m_videoCodecContext);


    av_freep(&srcFrame->data[0]);
    av_frame_free(&srcFrame);
    av_frame_free(&yuvFrame);

    return true;
}

bool ScreenRecorder::writeFrame(AVFrame *frame, AVStream *stream, AVCodecContext *codecContext)
{

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
        av_packet_rescale_ts(pkt, codecContext->time_base, stream->time_base);
        qDebug() << "time base" << m_videoStream->time_base.num << stream->time_base.den << stream->index;
        pkt->stream_index = stream->index;
        av_interleaved_write_frame(m_formatContext, pkt);
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);

    return true;
}

void ScreenRecorder::cleanup()
{
    if (m_swsContext) {
        sws_freeContext(m_swsContext);
        m_swsContext = nullptr;
    }

    if (m_swrContext) {
        swr_free(&m_swrContext);
        m_swrContext = nullptr;
    }

    if (m_audioFrame) {
        av_frame_free(&m_audioFrame);
        m_audioFrame = nullptr;
    }

    if (m_videoCodecContext) {
        avcodec_free_context(&m_videoCodecContext);
        m_videoCodecContext = nullptr;
    }

    if (m_audioCodecContext) {
        avcodec_free_context(&m_audioCodecContext);
        m_audioCodecContext = nullptr;
    }

    if (m_formatContext) {
        if (m_formatContext->pb) {
            avio_closep(&m_formatContext->pb);
        }
        avformat_free_context(m_formatContext);
        m_formatContext = nullptr;
    }

    if (m_packet) {
        av_packet_free(&m_packet);
        m_packet = nullptr;
    }
}

}
