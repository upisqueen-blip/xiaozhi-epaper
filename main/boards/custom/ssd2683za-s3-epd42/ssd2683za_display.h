#ifndef SSD2683ZA_DISPLAY_H
#define SSD2683ZA_DISPLAY_H

#include <driver/spi_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "config.h"
#include "epaper_dashboard.h"
#include "lcd_display.h"

class Ssd2683zaDisplay : public LcdDisplay {
public:
    Ssd2683zaDisplay();
    ~Ssd2683zaDisplay() override;
    void SetStatus(const char* status) override;
    EpaperDashboard& dashboard() { return dashboard_; }

private:
    static constexpr size_t kFrameBytes = DISPLAY_WIDTH * DISPLAY_HEIGHT / 4;
    spi_device_handle_t spi_ = nullptr;
    uint8_t* frame_ = nullptr;
    uint8_t* tx_frame_ = nullptr;
    SemaphoreHandle_t mutex_ = nullptr;
    TaskHandle_t refresh_task_ = nullptr;
    bool refresh_pending_ = false;
    EpaperDashboard dashboard_;
    bool voice_overlay_active_ = false;

    static void FlushCallback(lv_display_t* display, const lv_area_t* area, uint8_t* pixels);
    static void RefreshTaskEntry(void* arg);
    void RefreshTask();
    void InitializeSpi();
    void InitializePanel();
    void HardwareReset();
    bool WaitBusy(uint32_t timeout_ms = 45000);
    void SendCommand(uint8_t command);
    void SendData(uint8_t data);
    void SendBuffer(const uint8_t* data, size_t length);
    void Refresh(const uint8_t* data);
    static uint8_t Rgb565To2bpp(uint16_t color);
};

#endif
