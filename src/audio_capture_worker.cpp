#include "audio_capture_worker.h"
namespace adc{
AudioCaptureWorker::AudioCaptureWorker(FFmpegMuxer* muxer, const QString& deviceName,QObject *parent)
    : QThread{parent}, m_muxer(muxer), m_deviceName(deviceName), m_running(false)
{


}

}
