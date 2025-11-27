#ifndef AUDIOCAPTURE_H
#define AUDIOCAPTURE_H

#include <QObject>
#include <QThread>

namespace adc{
class AudioCapturePrivate;
class Recorder;
class AudioCapture : public QThread
{
    Q_OBJECT
public:
    AudioCapture(Recorder* instance);
    bool init();
    bool startRecording();
    void stopRecording();
    ~AudioCapture();

protected:
    void run() override;
private:
    AudioCapturePrivate* d;





};
}
#endif // AUDIOCAPTURE_H
