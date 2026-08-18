#ifndef _AUDIO_TEST_H
#define _AUDIO_TEST_H

#include "audio_codec.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class AudioTest {
public:
    AudioTest(AudioCodec* codec);
    
    void PlayTone(int frequency, int duration_ms, int volume = 50);
    void PlayBeepSequence();
    void TestMicrophoneLoopback(int duration_ms);
    void PlaySineWave(int frequency, int duration_ms);
    
private:
    AudioCodec* codec_;
    
    void GenerateSineWave(int16_t* buffer, int samples, int frequency, int sample_rate);
};

#endif // _AUDIO_TEST_H