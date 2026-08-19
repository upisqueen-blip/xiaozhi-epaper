# s3单屏幕+外接50pin 硬件核对

核对来源：`双屏+8pin_墨水屏S3芯片_2026-08-18.epro2` 中的
`s3单屏幕+外接50pin`，对应 `schematic_main / PCB_Main`（board
`48320e038509bfa6`）。没有混用另一张 `s3双屏幕` 原理图。

## 固件引脚

| 功能 | 网络 / GPIO |
| --- | --- |
| ES8311 I2C SDA / SCL | GPIO13 / GPIO14 |
| ES8311 MCLK / BCLK / LRCLK | GPIO21 / GPIO18 / GPIO16 |
| ES8311 DAC 输入 / ADC 输出 | GPIO15 / GPIO17 |
| NS4150 CTRL | GPIO38 |
| 电子纸 DIN / CLK / CS / DC / RST / BUSY | GPIO6 / GPIO8 / GPIO9 / GPIO10 / GPIO11 / GPIO12 |
| TF CS / MISO / SCK / MOSI | GPIO1 / GPIO2 / GPIO4 / GPIO5 |
| BOOT / KEY+ / KEY_OK / KEY- | GPIO0 / GPIO45 / GPIO46 / GPIO0 |

电子纸的控制网络和现有 SSD2683ZA 驱动一致，因此没有修改初始化、
LUT 或刷新时序。

## 制板前必须处理

最新原理图包含 ES8311（U8）、NS4150（U20）、ZTS6216（MIC1）及其
外围，但同一工程内的 `PCB_Main` 元件和 PAD_NET 记录没有这些器件。
这表示原理图和 PCB 尚未同步，当前 PCB 文件不能证明音频走线已存在。
制板前应在嘉立创 EDA 中执行“更新 PCB”，完成器件摆放和布线，再运行
ERC/DRC；否则无论固件如何配置，实板音频都不会工作。

同时检查 GPIO45、GPIO46（ESP32-S3 启动绑带脚）上的按键在复位期间
没有被外部电路强制到错误电平。GPIO0 同时承担 BOOT 与 KEY-，固件只将
它作为启动/主按键使用，按住该键复位会进入下载模式，这是预期硬件行为。
