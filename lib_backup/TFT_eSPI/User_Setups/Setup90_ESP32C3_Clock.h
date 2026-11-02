// 自定义 User_Setup.h —— ST7789 240x240，适配 合宙 CORE-ESP32-C3 经典版
// （原 GPIO10 / GPIO06 焊盘已坏：RES 改接 IO05，DC 改接 IO04）

#define USER_SETUP_INFO "ESP32C3Clock_ST7789_240x240"

// 驱动：ST7789
#define ST7789_DRIVER

// 颜色顺序 + 分辨率
#define TFT_RGB_ORDER TFT_BGR
#define TFT_WIDTH  240
#define TFT_HEIGHT 240

// 背光高电平点亮
#define TFT_BACKLIGHT_ON HIGH

// 引脚（7 针屏，无 CS：CS 接 GND；无 MISO：读不回）
#define TFT_MISO  19   // 未接（7 针屏无 MISO）
#define TFT_SCLK  02   // SPI 时钟  SCL/SCK
#define TFT_MOSI  03   // SPI 数据  SDA/SDI
#define TFT_RST   05   // 复位      RES（原 GPIO10 已坏，改 IO05）
#define TFT_DC    04   // 数据/命令 DC（原 GPIO06 已坏，改 IO04）
#define TFT_BL    07   // 背光      BLK

// 字体
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

// 平滑字体，节省约 20KB flash
#define SMOOTH_FONT

// SPI 频率
#define SPI_FREQUENCY  80000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000
