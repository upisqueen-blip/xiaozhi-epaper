#include "audio_test_controller.h"
#include "config.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "AudioTestController"

AudioTestController::AudioTestController(AudioCodec* codec) 
    : audio_test_(new AudioTest(codec)),
      test_button_(TOUCH_BUTTON_GPIO) {
}

void AudioTestController::OnTestButtonPressed() {
    ESP_LOGI(TAG, "Test button pressed, starting audio tests...");
    RunAllTests();
}

void AudioTestController::RunAllTests() {
    ESP_LOGI(TAG, "=== Starting Audio Hardware Test ===");
    
    ESP_LOGI(TAG, "1. Testing Speaker Output...");
    audio_test_->PlayBeepSequence();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    ESP_LOGI(TAG, "2. Testing Microphone Loopback...");
    audio_test_->TestMicrophoneLoopback(5000);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    ESP_LOGI(TAG, "3. Testing Frequency Response...");
    int frequencies[] = {200, 500, 1000, 2000, 4000, 8000};
    for (int freq : frequencies) {
        ESP_LOGI(TAG, "Playing %d Hz tone...", freq);
        audio_test_->PlayTone(freq, 300, 25);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    ESP_LOGI(TAG, "=== Audio Hardware Test Completed ===");
}

void AudioTestController::Initialize() {
    ESP_LOGI(TAG, "Initializing Audio Test Controller");
    
    test_button_.OnPressDown([this]() {
        OnTestButtonPressed();
    });
    
    ESP_LOGI(TAG, "Audio Test Controller initialized. Press touch button to start test.");
}

void AudioTestController::Run() {
    Initialize();
}