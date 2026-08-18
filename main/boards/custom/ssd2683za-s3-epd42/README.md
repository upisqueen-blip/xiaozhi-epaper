# SSD2683ZA ESP32-S3 ePaper 4.2

Custom ESP32-S3R8 board with a 400x300 SSD2683ZA 2-bpp e-paper panel,
ES8311 audio codec, ZTS6216 analog microphone and NS4150B amplifier.

The pin assignments in `config.h` were taken from
`Netlist_PCB_Main_2026-08-13.net`. The display initialization and refresh
sequence were ported from the hardware-tested
`firmware-2683ZA-s3-epd42-2bpp-fix-source` project.

The board uses XiaoZhi's standard AFE wake-word, microphone capture, cloud
conversation and speaker playback pipeline. During a conversation the EPD
shows `小悟 listening` while recording and `小悟 speaking` while playing the
assistant response.

## Independent information dashboard

`epaper_dashboard.cc/.h` is an original implementation for this board. It does
not copy code, layouts, images, fonts, or web resources from the non-commercial
`miaomiao` project.

Implemented pages:

- Home page with local date/time and usage hints.
- 400x300 monthly calendar with Monday-first layout and today highlighting.
- NVS-backed daily schedule.
- Album entry and persistent photo caption (image persistence is planned).
- NVS-backed generic API quota summary.

The BOOT button keeps its original single-click voice action. Double-clicking
cycles through dashboard pages. Dashboard pages are hidden while XiaoZhi is
listening or speaking and restored after the voice session.

The following MCP tools let XiaoZhi control the display by voice:

- `self.epaper.show_page`
- `self.epaper.set_schedule`
- `self.epaper.set_album_caption`
- `self.epaper.set_quota`

The quota page intentionally stores only numeric summary values. It does not
store an OpenAI login, browser cookie, or account password. A future network
adapter may fetch a user-selected service's documented JSON endpoint.

The LVGL flush callback only updates a PSRAM framebuffer. A dedicated
FreeRTOS task coalesces UI changes and performs the slow panel refresh, so
EPD BUSY waits do not block XiaoZhi audio capture, playback or networking.

Verified with ESP-IDF 5.5.2:

```sh
idf.py -DBOARD_TYPE=custom/ssd2683za-s3-epd42 -DBOARD_NAME=ssd2683za-s3-epd42 build
```

Hardware validation should cover microphone capture, wake word, playback,
NS4150B mute/unmute, Wi-Fi provisioning, and repeated full display refreshes.
