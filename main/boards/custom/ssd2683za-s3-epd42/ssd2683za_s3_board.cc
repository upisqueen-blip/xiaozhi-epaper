#include <driver/i2c_master.h>

#include "application.h"
#include "button.h"
#include "codecs/es8311_audio_codec.h"
#include "config.h"
#include "mcp_server.h"
#include "sd_card_storage.h"
#include "ssd2683za_display.h"
#include "wifi_board.h"
#include "lvgl_image.h"

#include <esp_heap_caps.h>

class Ssd2683zaS3Board : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    Button boot_button_;
    Ssd2683zaDisplay* display_ = nullptr;
    SdCardStorage storage_;

    void InitializeI2c() {
        i2c_master_bus_config_t config = {};
        config.i2c_port = I2C_NUM_0;
        config.sda_io_num = AUDIO_CODEC_I2C_SDA_PIN;
        config.scl_io_num = AUDIO_CODEC_I2C_SCL_PIN;
        config.clk_source = I2C_CLK_SRC_DEFAULT;
        config.glitch_ignore_cnt = 7;
        config.flags.enable_internal_pullup = 1;
        ESP_ERROR_CHECK(i2c_new_master_bus(&config, &i2c_bus_));
    }

    void InitializeButton() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
            } else {
                app.ToggleChatState();
            }
        });
        boot_button_.OnDoubleClick([this]() {
            display_->dashboard().Next();
        });
    }

    void InitializeDashboardTools() {
        auto& mcp = McpServer::GetInstance();
        mcp.AddTool("self.epaper.show_page",
                    "显示墨水屏信息页。page 可选 home、calendar、schedule、album、quota。",
                    PropertyList({Property("page", kPropertyTypeString)}),
                    [this](const PropertyList& properties) -> ReturnValue {
                        display_->dashboard().Show(properties["page"].value<std::string>());
                        return display_->dashboard().CurrentPageName();
                    });
        mcp.AddTool("self.epaper.set_schedule",
                    "保存并显示今日课程表，content 为每行一节课的纯文本。",
                    PropertyList({Property("content", kPropertyTypeString)}),
                    [this](const PropertyList& properties) -> ReturnValue {
                        display_->dashboard().SetSchedule(properties["content"].value<std::string>());
                        display_->dashboard().Show(EpaperDashboard::Page::kSchedule);
                        return true;
                    });
        mcp.AddTool("self.epaper.set_album_caption",
                    "设置电子相册当前照片说明文字。",
                    PropertyList({Property("caption", kPropertyTypeString)}),
                    [this](const PropertyList& properties) -> ReturnValue {
                        display_->dashboard().SetAlbumCaption(properties["caption"].value<std::string>());
                        display_->dashboard().Show(EpaperDashboard::Page::kAlbum);
                        return true;
                    });
        mcp.AddTool("self.epaper.set_quota",
                    "更新通用 API 额度看板数据。",
                    PropertyList({
                        Property("balance", kPropertyTypeInteger),
                        Property("used", kPropertyTypeInteger),
                        Property("requests", kPropertyTypeInteger),
                        Property("tokens", kPropertyTypeInteger),
                    }),
                    [this](const PropertyList& properties) -> ReturnValue {
                        display_->dashboard().SetQuota(
                            properties["balance"].value<int>(), properties["used"].value<int>(),
                            properties["requests"].value<int>(), properties["tokens"].value<int>());
                        display_->dashboard().Show(EpaperDashboard::Page::kQuota);
                        return true;
                    });
        mcp.AddTool("self.storage.status", "查看 TF 卡挂载状态和剩余容量。", PropertyList(),
                    [this](const PropertyList&) -> ReturnValue { return storage_.Status(); });
        mcp.AddTool("self.storage.list", "列出 TF 卡文件。category 可选 images、audio、recordings。",
                    PropertyList({Property("category", kPropertyTypeString)}),
                    [this](const PropertyList& p) -> ReturnValue {
                        return storage_.List(p["category"].value<std::string>());
                    });
        mcp.AddTool("self.epaper.show_sd_image", "显示 TF 卡 /xiaozhi/images 中的 PNG/JPEG 图片。",
                    PropertyList({Property("name", kPropertyTypeString)}),
                    [this](const PropertyList& p) -> ReturnValue {
                        std::vector<uint8_t> bytes;
                        std::string error;
                        if (!storage_.Load("images", p["name"].value<std::string>(), bytes,
                                           6 * 1024 * 1024, error)) return error;
                        void* data = heap_caps_malloc(bytes.size(), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                        if (!data) data = heap_caps_malloc(bytes.size(), MALLOC_CAP_8BIT);
                        if (!data) return std::string("image memory allocation failed");
                        memcpy(data, bytes.data(), bytes.size());
                        try {
                            display_->SetPreviewImage(std::make_unique<LvglAllocatedImage>(data, bytes.size()));
                        } catch (...) {
                            heap_caps_free(data);
                            return std::string("unsupported or damaged image");
                        }
                        return true;
                    });
        mcp.AddTool("self.audio.play_sd_wav", "播放 TF 卡 /xiaozhi/audio 中的单声道 16-bit 24kHz PCM WAV。",
                    PropertyList({Property("name", kPropertyTypeString)}),
                    [this](const PropertyList& p) -> ReturnValue {
                        std::string error;
                        return storage_.PlayWav(GetAudioCodec(), p["name"].value<std::string>(), error)
                                   ? ReturnValue(true) : ReturnValue(error);
                    });
        mcp.AddTool("self.audio.record_sd_wav", "录音到 TF 卡 /xiaozhi/recordings，seconds 为 1~60 秒。",
                    PropertyList({Property("name", kPropertyTypeString),
                                  Property("seconds", kPropertyTypeInteger, 1, 60)}),
                    [this](const PropertyList& p) -> ReturnValue {
                        std::string error;
                        return storage_.RecordWav(GetAudioCodec(), p["name"].value<std::string>(),
                                                  p["seconds"].value<int>(), error)
                                   ? ReturnValue(true) : ReturnValue(error);
                    });
    }

public:
    Ssd2683zaS3Board() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeButton();
        display_ = new Ssd2683zaDisplay();
        storage_.Mount();  // Card absence must never prevent normal voice/display startup.
        InitializeDashboardTools();
    }

    AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec codec(
            i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN, AUDIO_CODEC_PA_PIN,
            AUDIO_CODEC_ES8311_ADDR);
        return &codec;
    }

    Display* GetDisplay() override { return display_; }
};

DECLARE_BOARD(Ssd2683zaS3Board);
