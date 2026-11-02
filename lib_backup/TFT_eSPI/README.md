# TFT_eSPI 库改动备份

本目录保存了「ESP32C3Clock」项目对 TFT_eSPI 库的**改动后文件**，用于在库被重装/更新/污染后快速恢复。

## 库位置

- 原始库路径：`/home/dk/Arduino/libraries/TFT_eSPI/`
- 库内已留原版备份（`.bak`），但重装/更新库会一并丢失，故此处也备份了改动后版本。

## 屏配置切换机制（重要）

本项目**不再直接改库根目录的 `User_Setup.h`**（它保持原始默认、已被绕过），
而是通过 `User_Setup_Select.h` 里的「自定义切换」块激活自定义配置：

- `User_Setup_Select.h`：注释掉默认的 `#include <User_Setup.h>`，新增一行
  `#include <User_Setups/Setup90_ESP32C3_WeatherClock.h>`（同一时间只保留一行不注释）。
- `User_Setups/Setup90_ESP32C3_WeatherClock.h`：自定义屏配置（真正生效）。

## 改动文件与内容（共 3 处）

### 1. `User_Setup_Select.h`

在 `// Only ONE line below should be uncommented...` 下方插入「自定义切换」块：

```cpp
//#include <User_Setup.h>           // Default setup is root library folder

// ---- 自定义切换（同一时间只保留一行不注释）----
#include <User_Setups/Setup90_ESP32C3_WeatherClock.h>  // ESP32-C3 时钟：ST7789 240x240
```

> 本质是把默认的 `User_Setup.h` 注释掉，改激活 `Setup90_ESP32C3_WeatherClock.h`。

### 2. `User_Setups/Setup90_ESP32C3_WeatherClock.h`

整个文件是新建的自定义配置，适配合宙 CORE-ESP32-C3 + 1.3 寸 ST7789 240×240 IPS：

| 项 | 值 |
|---|---|
| 驱动 | `ST7789_DRIVER` |
| 颜色顺序 | `TFT_RGB_ORDER TFT_BGR` |
| 分辨率 | `TFT_WIDTH 240` / `TFT_HEIGHT 240` |
| 背光 | `TFT_BACKLIGHT_ON HIGH`，`TFT_BL 07` |
| 引脚 | `TFT_MISO 19`（未接，7 针屏无 MISO）、`TFT_SCLK 02`、`TFT_MOSI 03`、`TFT_RST 05`、`TFT_DC 04`、`TFT_BL 07` |
| SPI 频率 | `80000000` |
| 字体 | `LOAD_GLCD / FONT2 / FONT4 / FONT6 / FONT7 / FONT8 / GFXFF`，并 `SMOOTH_FONT` |

> **坏引脚绕过**：屏的 RES / DC 原本接 GPIO10 / GPIO06，但那两个焊盘已脱落。
> 现改接 **RES→IO05**、**DC→IO04**。屏无 CS 脚（7 针屏），CS 直接接 GND，故**不定义 `TFT_CS`**。

### 3. `Processors/TFT_eSPI_ESP32_C3.h`

修复 ESP-IDF 5.x（Arduino core 3.x）下 `REG_SPI_BASE(i)` 定义错误导致的编译失败。

原始代码：
```c
#ifndef REG_SPI_BASE
  #define REG_SPI_BASE(i) DR_REG_SPI2_BASE
#endif
```

改为（先 `#undef` 再强制定义）：
```c
#ifdef REG_SPI_BASE
  #undef REG_SPI_BASE
#endif
#define REG_SPI_BASE(i) DR_REG_SPI2_BASE
```

> 原因：ESP-IDF 5.x 里 `REG_SPI_BASE(i)` 已被定义为「仅当 `i==2` 才返回地址，否则 0」，
> 而该库使用 `SPI_PORT == SPI2_HOST == 1`，导致取到空地址崩溃。C3 只有一个通用 SPI（SPI2），
> 所以直接强制指向 `DR_REG_SPI2_BASE` 是安全的。

## 如何恢复

重装/更新 TFT_eSPI 库后，用本目录的改动后文件覆盖回原库对应路径即可：

```bash
LIB=~/Arduino/libraries/TFT_eSPI
cp lib_backup/TFT_eSPI/User_Setup_Select.h                   "$LIB/User_Setup_Select.h"
cp lib_backup/TFT_eSPI/User_Setups/Setup90_ESP32C3_WeatherClock.h   "$LIB/User_Setups/Setup90_ESP32C3_WeatherClock.h"
cp lib_backup/TFT_eSPI/Processors/TFT_eSPI_ESP32_C3.h        "$LIB/Processors/TFT_eSPI_ESP32_C3.h"
```

覆盖后重新编译上传即可。若库版本升级导致这些文件结构变化，建议重新对照本 README 的改动点手工应用，而不是直接覆盖。
