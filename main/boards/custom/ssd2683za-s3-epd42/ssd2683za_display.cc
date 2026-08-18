#include "ssd2683za_display.h"

#include <algorithm>
#include <cassert>
#include <cstring>

#include <esp_heap_caps.h>
#include <esp_log.h>
#include "assets/lang_config.h"
#include "config.h"
#include "esp_lvgl_port.h"

namespace {
constexpr char kTag[] = "Ssd2683zaDisplay";
constexpr size_t kLvglBufferBytes = DISPLAY_WIDTH * 40 * sizeof(uint16_t);
}

Ssd2683zaDisplay::Ssd2683zaDisplay()
    : LcdDisplay(nullptr, nullptr, DISPLAY_WIDTH, DISPLAY_HEIGHT) {
    InitializeSpi();
    mutex_ = xSemaphoreCreateMutex();
    frame_ = static_cast<uint8_t*>(heap_caps_malloc(kFrameBytes, MALLOC_CAP_SPIRAM));
    tx_frame_ = static_cast<uint8_t*>(heap_caps_malloc(kFrameBytes, MALLOC_CAP_SPIRAM));
    assert(mutex_ && frame_ && tx_frame_);
    memset(frame_, 0x55, kFrameBytes);  // Four white pixels per byte (01).

    lv_init();
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 2;
    port_cfg.timer_period_ms = 50;
    lvgl_port_init(&port_cfg);
    lvgl_port_lock(0);
    display_ = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_display_set_user_data(display_, this);
    lv_display_set_flush_cb(display_, FlushCallback);
    auto lvgl_buffer = static_cast<uint8_t*>(
        heap_caps_malloc(kLvglBufferBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    assert(lvgl_buffer);
    lv_display_set_buffers(display_, lvgl_buffer, nullptr, kLvglBufferBytes,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lvgl_port_unlock();

    xTaskCreatePinnedToCore(RefreshTaskEntry, "ssd2683_refresh", 4096, this, 3,
                            &refresh_task_, 1);
    Refresh(frame_);
    SetupUI();
    dashboard_.Show(EpaperDashboard::Page::kHome);
}

Ssd2683zaDisplay::~Ssd2683zaDisplay() {
    if (refresh_task_) vTaskDelete(refresh_task_);
    if (spi_) spi_bus_remove_device(spi_);
    if (mutex_) vSemaphoreDelete(mutex_);
    heap_caps_free(frame_);
    heap_caps_free(tx_frame_);
}

void Ssd2683zaDisplay::SetStatus(const char* status) {
    // Keep XiaoZhi's normal state machine, but make the two short-lived voice
    // states prominent enough to be useful on a slow, non-animated EPD.
    if (status != nullptr && strcmp(status, Lang::Strings::LISTENING) == 0) {
        voice_overlay_active_ = true;
        dashboard_.HideForVoice();
        LcdDisplay::SetStatus("小悟 listening");
        LcdDisplay::SetChatMessage("system", "小悟 listening");
        return;
    }
    if (status != nullptr && strcmp(status, Lang::Strings::SPEAKING) == 0) {
        voice_overlay_active_ = true;
        dashboard_.HideForVoice();
        LcdDisplay::SetStatus("小悟 speaking");
        LcdDisplay::SetChatMessage("system", "小悟 speaking");
        return;
    }
    LcdDisplay::SetStatus(status != nullptr ? status : "");
    if (voice_overlay_active_) {
        voice_overlay_active_ = false;
        dashboard_.RestoreAfterVoice();
    }
}

uint8_t Ssd2683zaDisplay::Rgb565To2bpp(uint16_t c) {
    const uint8_t r = ((c >> 11) & 0x1f) * 255 / 31;
    const uint8_t g = ((c >> 5) & 0x3f) * 255 / 63;
    const uint8_t b = (c & 0x1f) * 255 / 31;
    const uint8_t luminance = (77 * r + 150 * g + 29 * b) >> 8;
    // SSD2683ZA codes used by the verified firmware: 00 black, 01 white,
    // 10/11 intermediate levels.
    if (luminance < 64) return 0;
    if (luminance < 128) return 3;
    if (luminance < 192) return 2;
    return 1;
}

void Ssd2683zaDisplay::FlushCallback(lv_display_t* display, const lv_area_t* area,
                                     uint8_t* pixels) {
    auto self = static_cast<Ssd2683zaDisplay*>(lv_display_get_user_data(display));
    const auto src = reinterpret_cast<const uint16_t*>(pixels);
    const int source_width = area->x2 - area->x1 + 1;
    xSemaphoreTake(self->mutex_, portMAX_DELAY);
    for (int y = std::max(0, static_cast<int>(area->y1));
         y <= std::min(DISPLAY_HEIGHT - 1, static_cast<int>(area->y2)); ++y) {
        for (int x = std::max(0, static_cast<int>(area->x1));
             x <= std::min(DISPLAY_WIDTH - 1, static_cast<int>(area->x2)); ++x) {
            const auto color = Rgb565To2bpp(
                src[(y - area->y1) * source_width + x - area->x1]);
            const size_t index = static_cast<size_t>(y) * DISPLAY_WIDTH / 4 + x / 4;
            const unsigned shift = 6 - (x & 3) * 2;
            self->frame_[index] = (self->frame_[index] & ~(0x3u << shift)) |
                                  (color << shift);
        }
    }
    self->refresh_pending_ = true;
    xSemaphoreGive(self->mutex_);
    xTaskNotifyGive(self->refresh_task_);
    lv_display_flush_ready(display);
}

void Ssd2683zaDisplay::RefreshTaskEntry(void* arg) {
    static_cast<Ssd2683zaDisplay*>(arg)->RefreshTask();
}

void Ssd2683zaDisplay::RefreshTask() {
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(250));  // Coalesce rapid UI updates.
        xSemaphoreTake(mutex_, portMAX_DELAY);
        if (!refresh_pending_) {
            xSemaphoreGive(mutex_);
            continue;
        }
        memcpy(tx_frame_, frame_, kFrameBytes);
        refresh_pending_ = false;
        xSemaphoreGive(mutex_);
        Refresh(tx_frame_);
    }
}

void Ssd2683zaDisplay::InitializeSpi() {
    gpio_config_t outputs = {};
    outputs.pin_bit_mask = (1ULL << EPD_CS_PIN) | (1ULL << EPD_DC_PIN) |
                           (1ULL << EPD_RST_PIN);
    outputs.mode = GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&outputs));
    gpio_set_level(EPD_CS_PIN, 1);
    gpio_set_level(EPD_RST_PIN, 1);
    gpio_config_t input = {};
    input.pin_bit_mask = 1ULL << EPD_BUSY_PIN;
    input.mode = GPIO_MODE_INPUT;
    ESP_ERROR_CHECK(gpio_config(&input));

    spi_bus_config_t bus = {};
    bus.mosi_io_num = EPD_MOSI_PIN;
    bus.miso_io_num = -1;
    bus.sclk_io_num = EPD_SCK_PIN;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = 4096;
    ESP_ERROR_CHECK(spi_bus_initialize(EPD_SPI_NUM, &bus, SPI_DMA_CH_AUTO));
    spi_device_interface_config_t device = {};
    device.clock_speed_hz = 4 * 1000 * 1000;
    device.mode = 0;
    device.spics_io_num = -1;
    device.queue_size = 1;
    ESP_ERROR_CHECK(spi_bus_add_device(EPD_SPI_NUM, &device, &spi_));
}

void Ssd2683zaDisplay::HardwareReset() {
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(EPD_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(40));
    gpio_set_level(EPD_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
}

bool Ssd2683zaDisplay::WaitBusy(uint32_t timeout_ms) {
    const TickType_t start = xTaskGetTickCount();
    while (gpio_get_level(EPD_BUSY_PIN) == EPD_BUSY_LEVEL) {
        if ((xTaskGetTickCount() - start) * portTICK_PERIOD_MS > timeout_ms) {
            ESP_LOGE(kTag, "BUSY timeout after %lu ms", static_cast<unsigned long>(timeout_ms));
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return true;
}

void Ssd2683zaDisplay::SendCommand(uint8_t command) {
    gpio_set_level(EPD_DC_PIN, 0);
    gpio_set_level(EPD_CS_PIN, 0);
    spi_transaction_t transaction = {};
    transaction.length = 8;
    transaction.tx_buffer = &command;
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi_, &transaction));
    gpio_set_level(EPD_CS_PIN, 1);
}

void Ssd2683zaDisplay::SendData(uint8_t data) {
    SendBuffer(&data, 1);
}

void Ssd2683zaDisplay::SendBuffer(const uint8_t* data, size_t length) {
    gpio_set_level(EPD_DC_PIN, 1);
    gpio_set_level(EPD_CS_PIN, 0);
    while (length) {
        const size_t chunk = std::min(length, static_cast<size_t>(4096));
        spi_transaction_t transaction = {};
        transaction.length = chunk * 8;
        transaction.tx_buffer = data;
        ESP_ERROR_CHECK(spi_device_polling_transmit(spi_, &transaction));
        data += chunk;
        length -= chunk;
    }
    gpio_set_level(EPD_CS_PIN, 1);
}

void Ssd2683zaDisplay::InitializePanel() {
    HardwareReset();
    SendCommand(0x4D); SendData(0x78);
    SendCommand(0x00); SendData(0x0F); SendData(0x29);
    SendCommand(0x06);
    const uint8_t booster[] = {0x0D, 0x12, 0x24, 0x25, 0x12, 0x29, 0x10};
    SendBuffer(booster, sizeof(booster));
    SendCommand(0x30); SendData(0x08);
    SendCommand(0x50); SendData(0x37);
    SendCommand(0x61);
    SendData(DISPLAY_WIDTH >> 8); SendData(DISPLAY_WIDTH & 0xff);
    SendData(DISPLAY_HEIGHT >> 8); SendData(DISPLAY_HEIGHT & 0xff);
    SendCommand(0xAE); SendData(0xCF);
    SendCommand(0xB0); SendData(0x13);
    SendCommand(0xBD); SendData(0x07);
    SendCommand(0xBE); SendData(0xFE);
    SendCommand(0xE9); SendData(0x01);
    SendCommand(0x04);
    WaitBusy();
}

void Ssd2683zaDisplay::Refresh(const uint8_t* data) {
    InitializePanel();
    SendCommand(0x10);
    SendBuffer(data, kFrameBytes);
    SendCommand(0x12);
    SendData(0x00);
    WaitBusy();
    SendCommand(0x02);
    SendData(0x00);
    WaitBusy();
}
