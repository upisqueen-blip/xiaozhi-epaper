#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>
#include <driver/spi_master.h>

#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// ES8311 + NS4150B pins from the "s3单屏幕+外接50pin" schematic.
// NOTE: the PCB_Main snapshot embedded in the 2026-08-18 .epro2 project does
// not yet contain the audio components. Update the PCB from the schematic
// before manufacturing; these audio nets cannot be PCB-verified yet.
#define AUDIO_CODEC_I2C_SDA_PIN GPIO_NUM_13
#define AUDIO_CODEC_I2C_SCL_PIN GPIO_NUM_14
#define AUDIO_I2S_GPIO_MCLK     GPIO_NUM_21
#define AUDIO_I2S_GPIO_BCLK     GPIO_NUM_18
#define AUDIO_I2S_GPIO_WS       GPIO_NUM_16
#define AUDIO_I2S_GPIO_DOUT     GPIO_NUM_15
#define AUDIO_I2S_GPIO_DIN      GPIO_NUM_17
#define AUDIO_CODEC_PA_PIN      GPIO_NUM_38
#define AUDIO_CODEC_PA_VOLTAGE  3.3f
#define AUDIO_CODEC_ES8311_ADDR ES8311_CODEC_DEFAULT_ADDR

#define BOOT_BUTTON_GPIO GPIO_NUM_0

// SSD2683ZA 4.2-inch 400x300 2-bpp panel.
#define EPD_SPI_NUM   SPI3_HOST
#define EPD_MOSI_PIN  GPIO_NUM_6
#define EPD_SCK_PIN   GPIO_NUM_8
#define EPD_CS_PIN    GPIO_NUM_9
#define EPD_DC_PIN    GPIO_NUM_10
#define EPD_RST_PIN   GPIO_NUM_11
#define EPD_BUSY_PIN  GPIO_NUM_12
#define EPD_BUSY_LEVEL 0

// XKTF-015-G microSD socket, verified from both schematic and PCB PAD_NET.
// The socket is wired for SDSPI (DAT1/DAT2 are intentionally unused).
#define SD_SPI_NUM   SPI2_HOST
#define SD_CS_PIN    GPIO_NUM_1
#define SD_MISO_PIN  GPIO_NUM_2
#define SD_SCK_PIN   GPIO_NUM_4
#define SD_MOSI_PIN  GPIO_NUM_5

#define DISPLAY_WIDTH  400
#define DISPLAY_HEIGHT 300

#endif
