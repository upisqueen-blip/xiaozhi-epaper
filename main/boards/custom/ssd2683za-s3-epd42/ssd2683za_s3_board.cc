#include <driver/i2c_master.h>

#include "application.h"
#include "button.h"
#include "codecs/es8311_audio_codec.h"
#include "config.h"
#include "mcp_server.h"
#include "ssd2683za_display.h"
#include "wifi_board.h"

class Ssd2683zaS3Board : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    Button boot_button_;
    Ssd2683zaDisplay* display_ = nullptr;

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
    }

public:
    Ssd2683zaS3Board() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeButton();
        display_ = new Ssd2683zaDisplay();
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
