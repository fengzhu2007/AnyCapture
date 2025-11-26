#include "audiocapture.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include "recorder.h"
#include <QDebug>
namespace adc{
class AudioCapturePrivate{
public:
    Recorder* instance;
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioClient* client = nullptr;
    IAudioCaptureClient* capture = nullptr;
    WAVEFORMATEX* pwfx = nullptr;

    bool capturing = false;
};


AudioCapture::AudioCapture(Recorder* instance)
    :QThread((QObject*)instance) {

    d = new AudioCapturePrivate;
    CoInitialize(nullptr);
}


bool AudioCapture::init(){
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&d->enumerator));
    if (FAILED(hr)) return false;
    d->enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &d->device);
    d->device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&d->client);

    d->client->GetMixFormat(&d->pwfx);
    // Initialize for loopback
    hr = d->client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 10000000, 0, d->pwfx, nullptr);
    if (FAILED(hr)){
        qDebug()<<("AudioClient Initialize failed:")<<hr;
        return false;
    }

    return true;
}


void AudioCapture::start(){
    if(d->capturing){
        return ;
    }
    d->capturing = true;
    d->client->GetService(IID_PPV_ARGS(&d->capture));
    d->client->Start();
}

void AudioCapture::stop(){
    if(d->capturing){
        d->client->Stop();
    }
    if (d->capture) d->capture->Release();
    if (d->client) d->client->Release();
    if (d->device) d->device->Release();
    if (d->enumerator) d->enumerator->Release();
}

AudioCapture::~AudioCapture(){
    CoUninitialize();
}

void AudioCapture::run(){
    UINT32 packetLength = 0;
    while(d->capturing){
        d->capture->GetNextPacketSize(&packetLength);
        while (packetLength > 0){
            BYTE* pData;
            UINT32 numFrames;
            DWORD flags;
            d->capture->GetBuffer(&pData, &numFrames, &flags, nullptr, nullptr);
            int bytes = numFrames * d->pwfx->nBlockAlign;

            d->instance->pushAudioFrame(pData,bytes,d->pwfx->nSamplesPerSec,d->pwfx->nChannels);
            d->capture->ReleaseBuffer(numFrames);
            d->capture->GetNextPacketSize(&packetLength);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

}
