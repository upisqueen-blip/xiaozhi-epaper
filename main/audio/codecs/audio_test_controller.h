#ifndef _AUDIO_TEST_CONTROLLER_H
#define _AUDIO_TEST_CONTROLLER_H

#include "audio_test.h"
#include "button.h"

class AudioTestController {
public:
    AudioTestController(AudioCodec* codec);
    
    void Initialize();
    void Run();
    
private:
    AudioTest* audio_test_;
    Button test_button_;
    
    void OnTestButtonPressed();
    void RunAllTests();
};

#endif // _AUDIO_TEST_CONTROLLER_H