#include "screen_recorder.h"
#include <QScreen>
#include <QGuiApplication>
#include <QWindow>
#include <QDebug>
#include <QAudioDeviceInfo>
#include <QElapsedTimer>

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
            qDebug()<<"msec:"<<msec<<m_interval;
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
    // Initialize FFmpeg components
    if (!initVideo() || !initAudio() ) {
        cleanup();
        return false;
    }
    m_isRecording.store(true);
    emit recordingStarted();
    qDebug() << "Recording started:" << m_outputFile;
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

QPixmap ScreenRecorder::captureImage(){
    //qputenv("QT_QUICK_BACKEND", "software");
    //qputenv("QMLSCENE_DEVICE", "softwarecontext");
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        emit errorOccurred("No screen available");
        return {};
    }
    // Capture screen
    QPixmap pixmap = screen->grabWindow(this->m_target);
    return pixmap;
}

QPixmap ScreenRecorder::dxgi(){
    auto inputFmt = av_find_input_format("dxgi");
    if (!inputFmt) {
        qWarning() << "dxgi input format not found";
        return {};
    }
    AVFormatContext *fmtCtx = nullptr;
    AVDictionary *opts = nullptr;
    QString hwndStr = QString("0x%1").arg((qulonglong)m_target, 0, 16);
    av_dict_set(&opts, "hwnd", hwndStr.toUtf8().data(), 0);

    if (avformat_open_input(&fmtCtx, "dxgi", inputFmt, &opts) < 0) {
        qWarning() << "can not open DXGI input device";
        return {};
    }


    AVPacket pkt;
    if (av_read_frame(fmtCtx, &pkt) < 0) {
        qWarning() << "No frame captured";
        avformat_close_input(&fmtCtx);
        return {};
    }

    int videoStreamIndex = -1;
    for (unsigned i = 0; i < fmtCtx->nb_streams; ++i) {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            break;
        }
    }
    if (videoStreamIndex < 0) {
        qWarning() << "No video stream found";
        avformat_close_input(&fmtCtx);
        return {};
    }

    AVCodecParameters *codecpar = fmtCtx->streams[videoStreamIndex]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);
    AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, codecpar);
    avcodec_open2(codecCtx, codec, nullptr);

    AVFrame *frame = av_frame_alloc();
    int ret = avcodec_send_packet(codecCtx, &pkt);
    if (ret >= 0) ret = avcodec_receive_frame(codecCtx, frame);
    av_packet_unref(&pkt);

    if (ret < 0) {
        qWarning() << "Decode failed";
        av_frame_free(&frame);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        return {};
    }

    SwsContext *sws = sws_getContext(
        frame->width, frame->height, (AVPixelFormat)frame->format,
        frame->width, frame->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr
        );

    AVFrame *rgbFrame = av_frame_alloc();
    int rgbBufSize = av_image_get_buffer_size(AV_PIX_FMT_RGB24, frame->width, frame->height, 1);
    uint8_t *rgbBuf = (uint8_t*)av_malloc(rgbBufSize);
    av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, rgbBuf,
                         AV_PIX_FMT_RGB24, frame->width, frame->height, 1);

    sws_scale(sws, frame->data, frame->linesize, 0, frame->height,
              rgbFrame->data, rgbFrame->linesize);

    // QImage → QPixmap
    QImage img(rgbFrame->data[0], frame->width, frame->height, rgbFrame->linesize[0], QImage::Format_RGB888);
    QPixmap pixmap = QPixmap::fromImage(img.copy());

    sws_freeContext(sws);
    av_free(rgbBuf);
    av_frame_free(&rgbFrame);
    av_frame_free(&frame);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&fmtCtx);

    return pixmap;

}

QPixmap ScreenRecorder::winDxgi(){

    RECT rc;
    GetWindowRect((HWND)m_target, &rc);
    int winWidth = rc.right - rc.left;
    int winHeight = rc.bottom - rc.top;


    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL featureLevel;
    D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                      nullptr, 0, D3D11_SDK_VERSION, &device, &featureLevel, &context);


    IDXGIDevice* dxgiDevice = nullptr;
    device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);

    IDXGIAdapter* adapter = nullptr;
    dxgiDevice->GetAdapter(&adapter);

    IDXGIOutput* output = nullptr;
    adapter->EnumOutputs(0, &output);

    IDXGIOutput1* output1 = nullptr;
    output->QueryInterface(__uuidof(IDXGIOutput1), (void**)&output1);

    IDXGIOutputDuplication* duplication = nullptr;
    output1->DuplicateOutput(device, &duplication);

    DXGI_OUTDUPL_FRAME_INFO frameInfo;
    IDXGIResource* desktopResource = nullptr;

    HRESULT hr = duplication->AcquireNextFrame(500, &frameInfo, &desktopResource);
    if(SUCCEEDED(hr)) {
        ID3D11Texture2D* desktopTex = nullptr;
        desktopResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&desktopTex);

        // 5
        D3D11_TEXTURE2D_DESC desc;
        desktopTex->GetDesc(&desc);
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.MiscFlags = 0;

        ID3D11Texture2D* cpuTex = nullptr;
        device->CreateTexture2D(&desc, nullptr, &cpuTex);
        context->CopyResource(cpuTex, desktopTex);

        // Map to CPU
        D3D11_MAPPED_SUBRESOURCE mapped;
        context->Map(cpuTex, 0, D3D11_MAP_READ, 0, &mapped);

        // 6.to QImage
        QImage img((uchar*)mapped.pData, desc.Width, desc.Height, mapped.RowPitch, QImage::Format_RGBA8888);
        //
        QImage cropped = img.copy(rc.left, rc.top, winWidth, winHeight);

        context->Unmap(cpuTex, 0);

        auto pixmap = QPixmap::fromImage(cropped.rgbSwapped());


        // free
        cpuTex->Release();
        desktopTex->Release();
        desktopResource->Release();
        duplication->ReleaseFrame();
        return pixmap;
    }else{
        qDebug()<<"capture failed";
        return {};
    }

}

void ScreenRecorder::stopRecording()
{
    if (!m_isRecording) {
        return;
    }
    //m_videoTimer->stop();
    m_isRecording.store(false);
}

void ScreenRecorder::onStopped(){
    //stop capture
    if (m_audioInput) {
        m_audioInput->stop();
        delete m_audioInput;
        m_audioInput = nullptr;
    }

    if (!m_audioQueue.isEmpty()) {
        processAudioBuffer();
    }

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

    if (m_audioCodecContext) {
        avcodec_send_frame(m_audioCodecContext, nullptr);
        while (true) {
            int ret = avcodec_receive_packet(m_audioCodecContext, m_packet);
            if (ret == AVERROR_EOF) {
                break;
            } else if (ret < 0 && ret != AVERROR(EAGAIN)) {
                qWarning() << "Error receiving final audio packets:" << ret;
                break;
            } else if (ret >= 0) {
                av_packet_rescale_ts(m_packet, m_audioCodecContext->time_base, m_audioStream->time_base);
                m_packet->stream_index = m_audioStream->index;

                if (av_interleaved_write_frame(m_formatContext, m_packet) < 0) {
                    qWarning() << "Error writing final audio packet";
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

bool ScreenRecorder::initAudio()
{
    // Find audio codec
    const AVCodec *audioCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!audioCodec) {
        emit errorOccurred("Could not find AAC encoder");
        return false;
    }

    m_audioCodecContext = avcodec_alloc_context3(audioCodec);
    if (!m_audioCodecContext) {
        emit errorOccurred("Could not allocate audio codec context");
        return false;
    }

    // Configure audio codec context using new API
    m_audioCodecContext->sample_fmt = audioCodec->sample_fmts ? audioCodec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
    m_audioCodecContext->bit_rate = 128000;
    m_audioCodecContext->sample_rate = 44100;

    m_audioCodecContext->ch_layout = AV_CHANNEL_LAYOUT_STEREO;

    m_audioCodecContext->frame_size = 1024;

    if (audioCodec->sample_fmts) {
        const enum AVSampleFormat *p = audioCodec->sample_fmts;
        while (*p != AV_SAMPLE_FMT_NONE) {
            if (*p == m_audioCodecContext->sample_fmt) break;
            p++;
        }
        if (*p == AV_SAMPLE_FMT_NONE) {
            m_audioCodecContext->sample_fmt = audioCodec->sample_fmts[0];
        }
    }

    if (audioCodec->supported_samplerates) {
        const int *p = audioCodec->supported_samplerates;
        int best_samplerate = 0;
        while (*p) {
            if (!best_samplerate || abs(44100 - *p) < abs(44100 - best_samplerate))
                best_samplerate = *p;
            p++;
        }
        if (best_samplerate) {
            m_audioCodecContext->sample_rate = best_samplerate;
        }
    }

    AVDictionary *opts = nullptr;
    av_dict_set(&opts, "strict", "experimental", 0);

    if (avcodec_open2(m_audioCodecContext, audioCodec, &opts) < 0) {
        av_dict_free(&opts);
        emit errorOccurred("Could not open audio codec");
        return false;
    }
    av_dict_free(&opts);

    // Allocate audio frame
    m_audioFrame = av_frame_alloc();
    if (!m_audioFrame) {
        emit errorOccurred("Could not allocate audio frame");
        return false;
    }

    m_audioFrame->format = m_audioCodecContext->sample_fmt;
    m_audioFrame->ch_layout = m_audioCodecContext->ch_layout;
    m_audioFrame->sample_rate = m_audioCodecContext->sample_rate;
    m_audioFrame->nb_samples = m_audioCodecContext->frame_size;

    if (av_frame_get_buffer(m_audioFrame, 0) < 0) {
        emit errorOccurred("Could not allocate audio frame buffer");
        return false;
    }

    // Initialize audio resampler with new channel layout API
    m_swrContext = swr_alloc();
    if (!m_swrContext) {
        emit errorOccurred("Could not allocate resampler context");
        return false;
    }

    AVChannelLayout in_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO;

    if (swr_alloc_set_opts2(&m_swrContext,
                            &out_ch_layout, m_audioCodecContext->sample_fmt, m_audioCodecContext->sample_rate,
                            &in_ch_layout, AV_SAMPLE_FMT_S16, 44100,
                            0, nullptr) < 0) {
        emit errorOccurred("Could not set resampler options");
        return false;
    }

    if (swr_init(m_swrContext) < 0) {
        emit errorOccurred("Could not initialize resampler");
        return false;
    }

    // Setup Qt5 audio input (microphone)
    m_audioFormat.setSampleRate(44100);
    m_audioFormat.setChannelCount(2);
    m_audioFormat.setSampleSize(16);
    m_audioFormat.setCodec("audio/pcm");
    m_audioFormat.setByteOrder(QAudioFormat::LittleEndian);
    m_audioFormat.setSampleType(QAudioFormat::SignedInt);

    QAudioDeviceInfo inputDevice = QAudioDeviceInfo::defaultInputDevice();
    if (!inputDevice.isFormatSupported(m_audioFormat)) {
        m_audioFormat = inputDevice.nearestFormat(m_audioFormat);
        qWarning() << "Using nearest supported audio format:" << m_audioFormat.sampleRate() << "Hz";
    }

    m_audioInput = new QAudioInput(inputDevice, m_audioFormat, this);
    m_audioBuffer.open(QIODevice::ReadWrite);

    // Connect to stateChanged signal for audio data
    connect(m_audioInput, &QAudioInput::stateChanged, [this](QAudio::State state) {
        if (state == QAudio::StoppedState && m_isRecording) {
            // Handle audio device error
            emit errorOccurred("Audio input device stopped unexpectedly");
        }
    });

    m_audioInput->start(&m_audioBuffer);

    qDebug() << "Audio initialized: sample_rate=" << m_audioCodecContext->sample_rate
             << "channels=" << m_audioCodecContext->ch_layout.nb_channels
             << "format=" << m_audioCodecContext->sample_fmt;



    m_audioStream = avformat_new_stream(m_formatContext, nullptr);
    if (!m_audioStream) {
        emit errorOccurred("Could not create audio stream");
        return false;
    }
    m_audioStream->id = m_formatContext->nb_streams - 1;
    avcodec_parameters_from_context(m_audioStream->codecpar, m_audioCodecContext);
    m_audioStream->time_base = {1, m_audioCodecContext->sample_rate};

    return true;
}


void ScreenRecorder::captureFrame()
{
    qDebug()<<"frame number:"<<m_videoPts;
    if (m_isPaused) {
        return;
    }

    auto pixmap = this->captureImage();
    QImage image = pixmap.toImage().scaled(m_resolution, Qt::KeepAspectRatio, Qt::SmoothTransformation);


    if (!encodeVideoFrame(image)) {
        emit errorOccurred("Failed to encode video frame");
    }

    // Also capture audio data periodically
    if (m_audioInput && m_audioBuffer.bytesAvailable() > 0) {
        qDebug()<<"onAudioDataReady";
        onAudioDataReady();
    }
}

void ScreenRecorder::onAudioDataReady()
{
    if (m_isPaused) {
        QByteArray audioData = m_audioBuffer.readAll();
        if (!audioData.isEmpty()) {
            m_audioQueue.enqueue(audioData);
        }
        return;
    }

    if (!m_audioQueue.isEmpty()) {
        processAudioBuffer();
    }

    if (m_audioBuffer.bytesAvailable() > 0) {
        QByteArray audioData = m_audioBuffer.readAll();
        if (!encodeAudioFrame(audioData)) {
            qWarning() << "Failed to encode audio frame";
        }
    }
}

void ScreenRecorder::processAudioBuffer()
{
    while (!m_audioQueue.isEmpty()) {
        QByteArray audioData = m_audioQueue.dequeue();
        if (!encodeAudioFrame(audioData)) {
            qWarning() << "Failed to encode cached audio frame";
        }
    }
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

bool ScreenRecorder::encodeAudioFrame(const QByteArray &audioData)
{
    if (audioData.isEmpty()) {
        return true;
    }

    // Convert audio data to AVFrame
    const uint8_t *srcData[1] = { reinterpret_cast<const uint8_t*>(audioData.constData()) };
    int srcSamples = audioData.size() / (2 * sizeof(int16_t)); // stereo S16

    if (srcSamples <= 0) {
        return true;
    }

    int dstSamples = av_rescale_rnd(swr_get_delay(m_swrContext, 44100) + srcSamples,
                                    44100, 44100, AV_ROUND_UP);

    if (dstSamples > m_audioFrame->nb_samples) {
        // Reallocate audio frame if needed
        av_frame_unref(m_audioFrame);
        m_audioFrame->nb_samples = dstSamples;
        if (av_frame_get_buffer(m_audioFrame, 0) < 0) {
            return false;
        }
    }

    int ret = swr_convert(m_swrContext, m_audioFrame->data, dstSamples,
                          srcData, srcSamples);
    if (ret < 0) {
        return false;
    }

    m_audioFrame->pts = m_audioPts - m_pauseAudioPtsOffset;
    m_audioPts += ret;
    m_audioFrame->nb_samples = ret;

    return writeFrame(m_audioFrame, m_audioStream, m_audioCodecContext);
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


        av_packet_rescale_ts(pkt, codecContext->time_base, m_videoStream->time_base);
        pkt->stream_index = m_videoStream->index;


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
