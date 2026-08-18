# XiaoWu ESP32-S3 Dual-Screen E-Paper Terminal

([中文](README_zh.md) | English)

## Project Overview

XiaoWu is a custom dual-screen e-paper terminal based on ESP32-S3. The mainboard
can independently drive two 24-pin e-paper displays (Screen A and Screen B). With
the optional 24-pin-to-50-pin adapter, it can also drive a 7-inch E Ink E6 panel.

The firmware combines e-paper information display, voice interaction, local media
storage and smart-home control in one low-power device.

## Features

- Independent output for two 24-pin e-paper displays
- 7-inch E Ink E6 support through the 24-pin-to-50-pin adapter
- Album, calendar, class schedule, notes, e-book and token balance dashboard
- Remote image delivery and local image playback from TF/microSD card
- XiaoZhi voice interaction, audio recording and local audio playback
- Local Home Assistant integration for LAN-based IoT control

> E-book mode requires a fast-refresh e-paper panel. Hardware combinations should
> be validated on the target board.

## Custom Hardware

The mainboard integrates two 24-pin e-paper interfaces, audio input and output,
TF/microSD storage, USB-C and the ESP32-S3 controller. A dedicated adapter adds
50-pin e-paper panel support.

<table>
  <tr>
    <td align="center"><a href="https://upisqueen-blip.github.io/xiaowu/"><img src="docs/xiaowu/dual-screen-mainboard.jpg" width="360" alt="Custom ESP32-S3 dual-screen e-paper mainboard"></a><br><b>ESP32-S3 dual-screen mainboard</b></td>
    <td align="center"><a href="https://upisqueen-blip.github.io/xiaowu/"><img src="docs/xiaowu/seven-inch-e6-display.jpg" width="360" alt="7-inch E Ink E6 display"></a><br><b>7-inch E Ink E6 display</b></td>
  </tr>
  <tr>
    <td colspan="2" align="center"><a href="https://upisqueen-blip.github.io/xiaowu/"><img src="docs/xiaowu/voice-note-product.jpg" width="520" alt="XiaoWu voice e-paper terminal"></a><br><b>XiaoWu voice e-paper terminal</b></td>
  </tr>
</table>

## Introduction

[https://upisqueen-blip.github.io/xiaowu/](https://upisqueen-blip.github.io/xiaowu/)
