#include "audio_test.h"
#include <esp_log.h>
#include <cmath>

#define TAG "AudioTest"

AudioTest::AudioTest(AudioCodec* codec) : codec_(codec) {
}

void AudioTest::GenerateSineWave(int16_t* buffer, int samples, int frequency, int sample_rate) {
    const float PI = 3.14159265358979323846f;
    for (int i = 0; i < samples; i++) {
        float t = (float)i / (float)sample_rate;
        buffer[i] = (int16_t)(sinf(2 * PI * frequency * t) * INT16_MAX);
    }
}

void AudioTest::PlayTone(int frequency, int duration_ms, int volume) {
    if (!codec_) {
        ESP_LOGE(TAG, "Codec not initialized");
        return;
    }
    
    int sample_rate = codec_->output_sample_rate();
    int samples = (sample_rate * duration_ms) / 1000;
    std::vector<int16_t> buffer(samples);
    
    GenerateSineWave(buffer.data(), samples, frequency, sample_rate);
    
    codec_->SetOutputVolume(volume);
    codec_->EnableOutput(true);
    codec_->OutputData(buffer);
    codec_->EnableOutput(false);
}

void AudioTest::PlayBeepSequence() {
    ESP_LOGI(TAG, "Playing beep sequence...");
    
    int frequencies[] = {523, 659, 784, 1047};
    int durations[] = {200, 200, 200, 400};
    
    for (int i = 0; i < 4; i++) {
        PlayTone(frequencies[i], durations[i], 30);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGI(TAG, "Beep sequence completed");
}

void AudioTest::TestMicrophoneLoopback(int duration_ms) {
    if (!codec_) {
        ESP_LOGE(TAG, "Codec not initialized");
        return;
    }
    
    ESP_LOGI(TAG, "Starting microphone loopback test for %d ms", duration_ms);
    
    codec_->SetOutputVolume(80);
    codec_->EnableOutput(true);
    codec_->EnableInput(true);
    
    int sample_rate = codec_->input_sample_rate();
    int total_samples = (sample_rate * duration_ms) / 1000;
    int samples_processed = 0;
    
    const int block_size = 512;
    std::vector<int16_t> buffer(block_size);
    
    while (samples_processed < total_samples) {
        if (codec_->InputData(buffer)) {
            codec_->OutputData(buffer);
            samples_processed += block_size;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    codec_->EnableOutput(false);
    codec_->EnableInput(false);
    
    ESP_LOGI(TAG, "Microphone loopback test completed");
}

void AudioTest::PlaySineWave(int frequency, int duration_ms) {
    PlayTone(frequency, duration_ms, 40);
}