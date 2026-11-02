#include "./font/Ni7seg45.h"
#include "./font/msyhl25.h"
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiScan.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <EEPROM.h>
// tft 驱动库 
#include <TFT_eSPI.h>
// json解析库 
#include <ArduinoJson.h>
// gzip 解压（使用 ESP32-C3 ROM 自带的 tinfl/miniz）
#include "miniz.h"
// wifi配置二维码
#include "./img/WifiQrcode.h"
// 太空人图片  
#include "./img/taikonaut/i0.h" 
#include "./img/taikonaut/i1.h" 
#include "./img/taikonaut/i2.h" 
#include "./img/taikonaut/i3.h" 
#include "./img/taikonaut/i4.h" 
#include "./img/taikonaut/i5.h" 
#include "./img/taikonaut/i6.h" 
#include "./img/taikonaut/i7.h" 
#include "./img/taikonaut/i8.h" 
#include "./img/taikonaut/i9.h" 
// 湿度
#include "./img/humidity.h"
// 温度 
#include "./img/temperature.h"
// 天气
#include "./img/weather/sunny.h"      // 晴朗 
#include "./img/weather/overcast.h"   // 阴天 
#include "./img/weather/rain.h"      // 雨天 
#include "./img/weather/snow.h"      // 下雪 
#include "./img/weather/thunder.h"   // 打雷 
#include "./img/weather/cloudy.h"    // 多云 

#include "common_def.h"
#include "secrets.h"

WebServer ConfigWifiServer;
String WifiSSID;
String WifiPassword;
bool bSetWifiSuccess;
int currentCity = 0;
volatile bool cityButtonPressed = false;

HTTPClient http;
TFT_eSPI tft = TFT_eSPI();
weather_info WeatherData;
char gFormat_Str[64] = {0};
DynamicJsonDocument doc(6144);
uint32_t background_color;
uint32_t line_color;
const uint16_t* Taikonaut[] = { 
    gImage_i0, gImage_i1, gImage_i2, gImage_i3, gImage_i4, 
    gImage_i5, gImage_i6, gImage_i7, gImage_i8, gImage_i9
};

weather_img_list weather_img_data[] = {
    {"晴", gImage_sunny},
    {"阴", gImage_overcast},
    {"雨", gImage_rain},
    {"雪", gImage_snow},
    {"雷", gImage_thunder},
    {"云", gImage_cloudy},
};

void Led_On()
{
   digitalWrite(LED1, HIGH);
}

void Led_Off()
{
   digitalWrite(LED1, LOW);
}

// 统计UTF-8字符数 
int GetUtf8LetterNumber(const char *s, size_t len)
{
    if (s == NULL || len == 0)
    {
        return 0;
    }
    
    int nCount = 0;
    for (size_t i=0; i<len; i++)
    {
    	//if ((s[i] & 0b11000000) != 0b10000000) j++;
        if ((s[i] & 0xc0) != 0x80) 
        {
            nCount++;
        }
    }
    return nCount;
}

// 扫描到的wifi列表，放在html的option中 
String GetWifiOptionList()
{
    static String wifilist;
    static time_t last_time = 0;
    time_t current = time(NULL);
    if (last_time+3 < current)
    {
        wifilist = "";
        WiFiScanClass scanwifi;
        scanwifi.scanNetworks();
        while(true)
        {
            if (scanwifi.scanComplete() != WIFI_SCAN_RUNNING)
            {
                break;
            }
            delay(100);
        }

        int16_t nCount = scanwifi.scanComplete();
        if (nCount != WIFI_SCAN_FAILED)
        {
            for (int i=0; i<nCount; i++)
            {
                String ssid;
                uint8_t encType;
                int32_t rssi;
                uint8_t* bssid;
                int32_t channel;
                if (scanwifi.getNetworkInfo(i, ssid, encType, rssi, bssid, channel))
                {
                    wifilist = wifilist + "<option value='" + ssid + "'>" + ssid + "</option>";
                    Serial.println("SSID: " + ssid);
                }
            }
        }
        scanwifi.scanDelete();
        last_time = time(NULL);
    }
    return wifilist;
}

// 设置wifi页面 
void WifiConnectPage() 
{
    String arg = ConfigWifiServer.uri();
    ConfigWifiServer.send(200, "text/html", FrontHtml+GetWifiOptionList()+AfterHtml);
}

// 获取wifi配置 
void HomeConnectPage()
{
    if (ConfigWifiServer.args() == 3)
    {
        WifiSSID = ConfigWifiServer.arg(0);
        if (WifiSSID == "Other")
        {
            WifiSSID = ConfigWifiServer.arg(1);
        }
        WifiPassword = ConfigWifiServer.arg(2);
        bSetWifiSuccess = true;

        ConfigWifiServer.send(200, "text/plain", "config wait...");
    }
    else
    {
        WifiConnectPage();
    }
}

void ConfigWifi()
{
    IPAddress myIP;

    WiFi.mode(WIFI_AP);
    WiFi.disconnect();
    WiFi.setTxPower(WIFI_POWER_5dBm);

    // 显示配置二维码 
    tft.pushImage(0, 0, TFT_WIDTH, TFT_HEIGHT, gWifiConfig_Qrcode); 
    // 设置热点 
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);

    // 所有dns查询都被重定向到ap热点ip 
    DNSServer dnsserver;
    dnsserver.start(53, "*", myIP);

    // 拦截http请求 
    ConfigWifiServer.on("/", HTTP_POST, HomeConnectPage);
    ConfigWifiServer.onNotFound(WifiConnectPage);
    ConfigWifiServer.begin(80);
    bSetWifiSuccess = false;
    while(!bSetWifiSuccess)
    {
        // 边解析dns，边http请求处理 
        dnsserver.processNextRequest(); 
        ConfigWifiServer.handleClient();
        delay(50);
    }
    dnsserver.stop();
    ConfigWifiServer.close();
    WiFi.softAPdisconnect();
}

// nvs/SPIClass/Preferences 
bool ReadWifiConfig()
{
    Preferences prefs;
    if ( !prefs.begin("wifispace", false, "nvs") )
    {
        Serial.println("read open preferences error");
    }
    WifiSSID = prefs.getString("ssid", "");
    WifiPassword = prefs.getString("password", "");
    prefs.end();
    
    /*
    EEPROM.begin(1024);
    WifiSSID.clear();
    WifiPassword.clear();
    if (EEPROM.read(0) == 0x5A)
    {
        for (int i = 1; i < WIFI_STR_LEN+1; i++)
        {
            char ch = EEPROM.read(i);
            if (ch == 0)
            {
                break;
            }
            WifiSSID += ch;
        }
        for (int i = WIFI_STR_LEN+1; i < WIFI_STR_LEN*2+1; i++)
        {
            char ch = EEPROM.read(i);
            if (ch == 0)
            {
                break;
            }
            WifiPassword += ch;
        }
    }
    EEPROM.commit();
    */

    Serial.printf("read ssid: %s, password: %s\n", WifiSSID.c_str(), WifiPassword.c_str());

    if (WifiSSID.length()>0)
    {
        return true;
    }
    return false;    
}

void SaveWifiConfig()
{
    Preferences prefs;
    if ( !prefs.begin("wifispace", false, "nvs") )
    {
        Serial.println("write open preferences error");
    }
    prefs.putString("ssid", WifiSSID);
    prefs.putString("password", WifiPassword);
    prefs.end();
    
   /*
   EEPROM.begin(1024);
   EEPROM.write(0, 0x5A);
   for (int i = 1; i < WIFI_STR_LEN+1; i++)
    {
        if (i>=WifiSSID.length())
        {
            EEPROM.write(i, 0);
        }
        else
        {
            EEPROM.write(i, (uint8_t)WifiSSID[i]);
        }
    }
    for (int i = WIFI_STR_LEN+1; i < WIFI_STR_LEN*2+1; i++)
    {
        if (i-WIFI_STR_LEN >= WifiSSID.length())
        {
            EEPROM.write(i, 0);
        }
        else
        {
            EEPROM.write(i, (uint8_t)WifiPassword[i-WIFI_STR_LEN]);
        }
    }
    EEPROM.commit();
    */
}

void ShowWeather()
{
    // 定位太长了，截断 
    const char* parea = WeatherData.area.c_str();
    if (GetUtf8LetterNumber(parea, WeatherData.area.length()) > 6)
    {
        const char* pcity = strchr(parea, '省');
        if (pcity)
        {
            parea = pcity + 1;
            if (GetUtf8LetterNumber(parea, strlen(parea)) > 6)
            {
                const char* pcity = strchr(parea, '市');
                if (pcity)
                {
                    parea = pcity + 1;
                } 
            }
        }                
    }

    // 天气图片 
    const uint16_t* PWeatherPic = gImage_sunny;
    for (int i = 0; i < sizeof(weather_img_data)/sizeof(weather_img_data[0]); i++)
    {
        if (strstr(WeatherData.weather.c_str(), weather_img_data[i].name) != NULL)
        {
            PWeatherPic = weather_img_data[i].pic;
            break;
        }
    }
    int weather_len = GetUtf8LetterNumber(WeatherData.weather.c_str(), WeatherData.weather.length());
    
    sprintf(gFormat_Str, "%s°C", WeatherData.temperature);
    // 整个过程加载字体 
    tft.loadFont(msyhl25); 
    tft.setTextColor(TFT_BLACK, background_color, true);
    // 定位（城市名+天气两行；上移3px覆盖汉字 dY 溢出 ascent 18 的顶部笔画）
    tft.fillRect(4, 5-3, MSYHL25*6, MSYHL25*2+4+3, background_color);
    tft.drawString(parea, 4, 5);
    // 天气(不定长) 
    tft.drawString(WeatherData.weather, 5, 5+MSYHL25+4);
    tft.pushImage(MSYHL25*weather_len+10, 30, 24, 24, PWeatherPic);
    // 温度 
    tft.pushImage(SUPERIOR_LEFT+LINE_WIDTH, 16, 24, 24, gImage_temp); 
    tft.drawString(gFormat_Str, SUPERIOR_LEFT+LINE_WIDTH+25, 20);
    // 风向+风力（一行，右对齐；太空人已左移到 x=0，右侧文字区从 x=70 起）
    // 注：汉字 dY 最大 21 > 字体 ascent 18，顶部会向上溢出 3px，故清屏区域上移 3px 覆盖
    sprintf(gFormat_Str, "%s%s级", WeatherData.wind.c_str(), WeatherData.windPower.c_str());
    {
        int16_t w = tft.textWidth(gFormat_Str);
        tft.fillRect(70, SUPERIOR+LINE_WIDTH+MIDIUM-49-3, TFT_WIDTH-70, 26, background_color);
        tft.drawString(gFormat_Str, TFT_WIDTH-w-5, SUPERIOR+LINE_WIDTH+MIDIUM-49);
    }
    // 湿度（文字"湿度XX%"，右对齐，去掉图标）
    sprintf(gFormat_Str, "湿度%s%%", WeatherData.humidity.c_str());
    {
        int16_t w = tft.textWidth(gFormat_Str);
        tft.fillRect(70, SUPERIOR+LINE_WIDTH+MIDIUM-26, TFT_WIDTH-70, 23, background_color);
        tft.drawString(gFormat_Str, TFT_WIDTH-w-5, SUPERIOR+LINE_WIDTH+MIDIUM-26);
    }
    // 当日温度范围（太空人右侧、风向风力上方，右对齐；3d 失败则清空该行）
    tft.fillRect(70, SUPERIOR+MIDIUM+LINE_WIDTH-75, TFT_WIDTH-70, 23, background_color);
    if (WeatherData.tempMin.length() > 0 && WeatherData.tempMax.length() > 0)
    {
        sprintf(gFormat_Str, "%s~%s°C", WeatherData.tempMin.c_str(), WeatherData.tempMax.c_str());
        int16_t w = tft.textWidth(gFormat_Str);
        tft.drawString(gFormat_Str, TFT_WIDTH-w-5, SUPERIOR+MIDIUM+LINE_WIDTH-75);
    }
    // 卸载字体 
    tft.unloadFont();
}

// gzip 解压：和风天气服务器始终返回 gzip 压缩数据（无视 Accept-Encoding）。
// 用 ESP32-C3 ROM 自带的 tinfl 解压。成功返回堆上分配的 null 结尾字符串（调用方 free），
// 失败或非 gzip 返回 NULL。*out_len 输出实际字节数（不含结尾 0）。
char* Gunzip(const uint8_t* data, size_t len, size_t* out_len)
{
    if (out_len) *out_len = 0;

    // gzip 魔数与最小结构：10 字节固定头 + deflate + 8 字节尾(CRC32 + ISIZE)
    if (data == NULL || len < 19 || data[0] != 0x1F || data[1] != 0x8B || data[2] != 0x08)
    {
        return NULL;   // 不是 gzip 数据
    }

    // 跳过 gzip 头：10 字节固定头 + 可选字段（每一步都做边界检查）
    uint8_t flg = data[3];
    size_t pos = 10;
    if (flg & 0x04)   // FEXTRA
    {
        if (pos + 2 > len) return NULL;
        size_t xlen = (size_t)data[pos] | ((size_t)data[pos + 1] << 8);
        pos += 2 + xlen;
        if (pos > len) return NULL;
    }
    if (flg & 0x08)   // FNAME（以 0 结尾的字符串）
    {
        const uint8_t* end = (const uint8_t*)memchr(data + pos, 0, len - pos);
        if (!end) return NULL;
        pos = (size_t)(end - data) + 1;
    }
    if (flg & 0x10)   // FCOMMENT（以 0 结尾的字符串）
    {
        const uint8_t* end = (const uint8_t*)memchr(data + pos, 0, len - pos);
        if (!end) return NULL;
        pos = (size_t)(end - data) + 1;
    }
    if (flg & 0x02)   // FHCRC（头 CRC16）
    {
        pos += 2;
    }
    if (pos + 8 > len) return NULL;

    // deflate 原始流 = 总长 - 头 - 8 字节尾
    size_t src_len = len - pos - 8;

    // gzip 尾：CRC32(前4字节) + ISIZE(后4字节，小端，解压后大小 mod 2^32)
    uint32_t crc32 = (uint32_t)data[len - 8]
                   | ((uint32_t)data[len - 7] << 8)
                   | ((uint32_t)data[len - 6] << 16)
                   | ((uint32_t)data[len - 5] << 24);
    uint32_t isize = (uint32_t)data[len - 4]
                   | ((uint32_t)data[len - 3] << 8)
                   | ((uint32_t)data[len - 2] << 16)
                   | ((uint32_t)data[len - 1] << 24);

    // 按 ISIZE 精确分配，异常值兜底 16KB；+1 恒预留 null 结尾
    size_t cap = (isize > 0 && isize < 65536) ? (size_t)isize : 16384;
    uint8_t* out = (uint8_t*)malloc(cap + 1);
    if (!out) return NULL;

    // tinfl_decompressor 结构体约 11KB，不能放栈上（会撑爆 8KB 的 loopTask 栈），放到堆上
    tinfl_decompressor* r = (tinfl_decompressor*)malloc(sizeof(tinfl_decompressor));
    if (!r) { free(out); return NULL; }
    tinfl_init(r);

    size_t in_rem = src_len;
    size_t out_rem = cap;
    tinfl_status st = tinfl_decompress(r, data + pos, &in_rem, out, out, &out_rem,
                                       TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    free(r);
    if (st != TINFL_STATUS_DONE)
    {
        free(out);
        return NULL;
    }

    // 关键：miniz 3.x 的 tinfl_decompress 返回后，*pOut_buf_size 是「已写入的字节数」，
    // 不是「剩余空间」。所以解压长度就是 out_rem 本身，不能再 cap - out_rem。
    size_t n = out_rem;

    // 完整性校验：ISIZE 与 CRC32（ROM 自带 mz_crc32，与 gzip 标准一致）
    if ((isize != 0 && n != isize) || mz_crc32(MZ_CRC32_INIT, out, n) != crc32)
    {
        free(out);
        return NULL;
    }

    out[n] = '\0';   // cap+1 已预留，n <= cap 恒安全
    *out_len = n;
    return (char*)out;
}

bool UpdateWeather()
{
    bool bUpdate = false;
    Led_On();

    // 和风天气实时天气接口：/v7/weather/now?location={LocationID}
    String url = String("https://") + QWEATHER_API_HOST +
                 "/v7/weather/now?location=" + CITIES[currentCity].location;

    WiFiClientSecure client;
    client.setInsecure();   // 初版跳过证书校验

    http.begin(client, url);
    http.addHeader("X-QW-Api-Key", QWEATHER_API_KEY);
    http.setTimeout(10000);

    int httpCode = http.GET();
    if(httpCode == HTTP_CODE_OK) 
    {
        // 获取json数据（服务器返回 gzip 压缩，先解压）
        String pageData = http.getString();
        DeserializationError error;
        size_t jsonLen = 0;
        char* json = Gunzip((const uint8_t*)pageData.c_str(), pageData.length(), &jsonLen);
        if (json)
        {
            error = deserializeJson(doc, json, jsonLen);
            free(json);
        }
        else
        {
            // 兜底：服务器未压缩时直接解析
            error = deserializeJson(doc, pageData);
        }
        if (!error)
        {
            JsonObject obj = doc["now"].as<JsonObject>();

            // 最新数据放入结构体中保存 
            WeatherData.temperature = obj["temp"].as<String>();
            WeatherData.weather = obj["text"].as<String>();
            WeatherData.wind = obj["windDir"].as<String>();
            WeatherData.windPower = obj["windScale"].as<String>();
            WeatherData.humidity = obj["humidity"].as<String>();
            WeatherData.aqi = "";   // 初版省略 AQI
            WeatherData.area = CITIES[currentCity].name;
            WeatherData.update_time = time(NULL);
            WeatherData.last_update_time = time(NULL);

            // 本地计算星期 
            time_t ltime = time(NULL);
            struct tm *gmt = localtime( &ltime );
            const char* weekdays[] = {"星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"};
            WeatherData.week = weekdays[gmt->tm_wday];

            bUpdate = true;

            // 调试日志 
            Serial.print("城市: ");
            Serial.println(WeatherData.area);
            Serial.print("天气: ");
            Serial.println(WeatherData.weather);
            Serial.print("气温: ");
            Serial.println(WeatherData.temperature);
            Serial.print("风向: ");
            Serial.println(WeatherData.wind);
            Serial.print("风力: ");
            Serial.println(WeatherData.windPower);
            Serial.print("湿度: ");
            Serial.println(WeatherData.humidity);
        }
        else
        {
            Serial.print("Json Data analyze Error: ");
            Serial.print(error.c_str());
            Serial.print(" (len=");
            Serial.print(jsonLen);
            Serial.println(")");
            Serial.println(pageData);
        }
    }
    else
    {
        Serial.print("GET Weather Error: ");
        Serial.println(httpCode);
    }
    http.end();

    // 当日温度范围（3天预报接口，取 daily[0] 的 tempMax/tempMin）
    if (bUpdate)
    {
        String url3d = String("https://") + QWEATHER_API_HOST +
                       "/v7/weather/3d?location=" + CITIES[currentCity].location;
        http.begin(client, url3d);
        http.addHeader("X-QW-Api-Key", QWEATHER_API_KEY);
        http.setTimeout(10000);

        int code3d = http.GET();
        if (code3d == HTTP_CODE_OK)
        {
            String page3d = http.getString();
            size_t len3d = 0;
            char* json3d = Gunzip((const uint8_t*)page3d.c_str(), page3d.length(), &len3d);
            DeserializationError e3d;
            if (json3d)
            {
                e3d = deserializeJson(doc, json3d, len3d);
                free(json3d);
            }
            else
            {
                e3d = deserializeJson(doc, page3d);
            }
            if (!e3d)
            {
                JsonObject daily0 = doc["daily"][0].as<JsonObject>();
                WeatherData.tempMax = daily0["tempMax"].as<String>();
                WeatherData.tempMin = daily0["tempMin"].as<String>();
                Serial.print("温度范围: ");
                Serial.print(WeatherData.tempMin);
                Serial.print("~");
                Serial.println(WeatherData.tempMax);
            }
            else
            {
                Serial.print("3d Json Error: ");
                Serial.println(e3d.c_str());
            }
        }
        else
        {
            Serial.print("GET 3d Error: ");
            Serial.println(code3d);
        }
        http.end();
    }

    // now + 3d 都完成后统一重绘，保证温度范围拿到最新 tempMin/tempMax 再显示
    if (bUpdate) ShowWeather();

    Led_Off();
    return bUpdate;
}

void ShowTime(bool force_update = false)
{
    // 显示时间，画动画 
    time_t ltime = time(NULL);
    struct tm *gmt = localtime( &ltime );

    // 0时0秒到2秒更新日期 
    if (force_update || (gmt->tm_hour == 0 && gmt->tm_min == 0 && gmt->tm_sec<2) )
    {
        // 底部整块清屏
        tft.fillRect(0, SUPERIOR+MIDIUM+LINE_WIDTH, TFT_WIDTH,
                     TFT_HEIGHT-(SUPERIOR+MIDIUM+LINE_WIDTH), background_color);

        // 竖分隔线：顶满整条（y=198~240），x=160，两列布局
        tft.fillRect(160, SUPERIOR+MIDIUM+LINE_WIDTH, LINE_WIDTH,
                     TFT_HEIGHT-(SUPERIOR+MIDIUM+LINE_WIDTH), line_color);

        tft.loadFont(msyhl25);
        tft.setTextColor(TFT_BLACK, background_color, true);
        tft.setTextDatum(C_BASELINE);   // 基线定位

        // 左列：日期 2026-10-29，左对齐 x=8（C_BASELINE 下 x 为文字中心，故传 8+宽/2）
        sprintf(gFormat_Str, "%04u-%02u-%02u",
                gmt->tm_year+1900, gmt->tm_mon+1, gmt->tm_mday);
        int16_t dateW = tft.textWidth(gFormat_Str);
        tft.drawString(gFormat_Str, 8 + dateW/2, SUPERIOR+MIDIUM+LINE_WIDTH+30);

        // 右列：星期 周四，右列(x=160~240)内居中
        const char* weekdays[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
        tft.drawString(weekdays[gmt->tm_wday], 200, SUPERIOR+MIDIUM+LINE_WIDTH+30);

        tft.setTextDatum(TL_DATUM);   // 恢复默认左上对齐
        tft.unloadFont();
    }

    if (force_update || gmt->tm_sec == 0)
    {
        // 更新：时-分 
        sprintf(gFormat_Str, "%02u:%02u", gmt->tm_hour, gmt->tm_min);
        tft.loadFont(Ni7seg45);
        tft.setTextColor(TFT_BLACK, background_color, true);
        tft.drawString(gFormat_Str, 9, SUPERIOR+LINE_WIDTH+12);
        tft.unloadFont();
    }

    // 更新：秒 
    sprintf(gFormat_Str, "%02u", gmt->tm_sec);
    tft.loadFont(msyhl25);
    tft.setTextColor(TFT_BLACK, background_color, true);
    tft.drawString(gFormat_Str, NI7SEG45*3+20, SUPERIOR+LINE_WIDTH+NI7SEG45-15);
    tft.unloadFont();
}

void create_partition()
{
    background_color = TFT_WHITE; // tft.color565(195, 230, 245);
    line_color = tft.color565(180, 180, 180);

    tft.fillScreen(background_color);
    // 上中下区域划分  
    tft.fillRect(0, SUPERIOR, TFT_WIDTH, LINE_WIDTH, line_color);
    tft.fillRect(0, SUPERIOR+MIDIUM, TFT_WIDTH, LINE_WIDTH, line_color);
    // 顶部左右区域划分 
    tft.fillRect(SUPERIOR_LEFT, 0, LINE_WIDTH, SUPERIOR, line_color); 
}

bool ConnectWifi()
{
    bool conn_result = false;
    if (WifiSSID.length() == 0)
    {
        return conn_result;
    }
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.setTxPower(WIFI_POWER_5dBm);

    Led_On();
    WiFi.begin(WifiSSID.c_str(), WifiPassword.c_str());

    tft.fillScreen(TFT_BLACK);
    tft.loadFont(msyhl25);
    tft.setTextColor(TFT_WHITE, TFT_BLACK, true);
    tft.drawString("正在连接网络...", 10, 60);
    tft.unloadFont();

    // 进度条 
    int index = 0;
    int x1 = 0;
    int y1 = 100;
    int height = 30;
    int r = height/2;
    tft.fillCircle(x1+r, y1+r, r, TFT_GREEN);
    while (true) {
        index++;
        delay(100);
        Serial.print(".");
        // 圆角进度条 
        int width = index*24/4;
        tft.fillRect(x1+r, y1, width, height, TFT_GREEN);
        tft.fillCircle(width+r, y1+r, r, TFT_GREEN);

        // 查询连接结果 
        int status = WiFi.status();
        if (
            status == WL_CONNECT_FAILED || 
            status == WL_NO_SSID_AVAIL || 
            status == WL_CONNECTION_LOST
            )
        {
            conn_result = false;
            break;
        }
        else if (status == WL_CONNECTED)
        {
            conn_result = true;
            Led_Off();
            Serial.print("\nWiFi connected, IP address:");
            Serial.println(WiFi.localIP());
            delay(500);
            break;
        }

        if (index > 40)
        {
            // 连接超时 
            WiFi.disconnect();
            break;
        }
        Serial.println(status);
    }
    return conn_result;
}

void SyncSystemTime()
{
    long gmtOffset_sec = TIME_ZONE*60*60;
    int daylightOffset_sec = 0;
    int index = 0;
    int x1 = 0;
    int y1 = 100;
    int height = 30;
    int r = height/2;
    uint32_t ltime;

    // 显示进度条 
    tft.fillScreen(TFT_BLACK);
    tft.loadFont(msyhl25);
    tft.setTextColor(TFT_WHITE, TFT_BLACK, true);
    tft.drawString("正在同步时间...", 10, 60);
    tft.unloadFont();

    int nTotalTimes = 0;
    do {
        Led_On();
        configTime(gmtOffset_sec, daylightOffset_sec, NTP_HOST, "ntp1.aliyun.com", "ntp2.aliyun.com");
        tft.fillRect(x1, y1, 240, height+2, TFT_BLACK);  // 进度条重新刷黑 
        for (int i = 0; i < 31; i++)
        {
            int width = 7*i;
            tft.fillRect(x1+r, y1, width, height, TFT_GREEN);
            tft.fillCircle(width+r, y1+r, r, TFT_GREEN);
            delay(150);
        }
        Led_Off();
        delay(500);
        ltime = time(NULL);
        
        nTotalTimes++;
        if (nTotalTimes>3)
        {
            break;
        }        
    } while (ltime<24*60*60);
}

void ShowTaikonaut()
{
    static uint8_t index = 0;
    tft.pushImage(0, SUPERIOR+MIDIUM+LINE_WIDTH-75, 70, 70, Taikonaut[index]);   // 太空人左移顶格，给右侧风向/湿度腾出空间 
    index++;
    if (index >= sizeof(Taikonaut)/sizeof(Taikonaut[0]))
    {
        index = 0;
    }
}

void test()
{
    // 屏幕分区 
    create_partition();

    WeatherData.temperature = "33";
    WeatherData.weather = "多云";
    WeatherData.wind = "东南风";
    WeatherData.windPower = "2级",
    WeatherData.humidity = "51%";
    WeatherData.aqi = "80";
    WeatherData.area = "浙江省杭州市";
    WeatherData.week = "星期六";
    WeatherData.update_time = time(NULL);
    WeatherData.last_update_time = time(NULL);
    ShowWeather();

    // 显示时间 
    ShowTime(true);
    while(true)
    {
        ShowTaikonaut();
        delay(150); 
    }

    while(true)
    {
        delay(1000);
    }
}

// 按键中断：切换城市（BOOT 键按下触发）
void IRAM_ATTR onCityButton()
{
    cityButtonPressed = true;
}

// 循环切换城市并刷新天气
void HandleCityButton()
{
    if (!cityButtonPressed)
    {
        return;
    }
    cityButtonPressed = false;

    currentCity = (currentCity + 1) % CITY_COUNT;
    Serial.printf("切换到城市: %s (%s)\n", CITIES[currentCity].name, CITIES[currentCity].location);
    UpdateWeather();
    ShowTime(true);
}

void setup() {
    Serial.begin(115200);
    pinMode(LED1, OUTPUT);
    pinMode(LED2, OUTPUT);
    digitalWrite(LED2, LOW);

    // 城市切换按键（BOOT 键 GPIO9），按下接地，低电平触发
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(BUTTON_PIN, onCityButton, FALLING);

    // 初始化tft屏幕240*240 
    tft.init();

//    test();

    // 读取之前的wifi配置 
    if ( !ReadWifiConfig() )
    {
        ConfigWifi();
    }

    // 连接wifi 
    while( !ConnectWifi() )
    {
        ConfigWifi();
    }
    // 连接成功，保存wifi配置 
    SaveWifiConfig();

    // 同步时间 
    SyncSystemTime();

    // 屏幕分区 
    create_partition();

    // 获取天气数据 
    UpdateWeather();

    // 显示时间 
    ShowTime(true);
}

void loop() {
    // put your main code here, to run repeatedly:
    HandleCityButton();

    delay(100);
    ShowTaikonaut();

    // 30分钟更新一次，如果网络上30分钟后还没更新过的，5分钟查询一次 
    uint32_t ltime = time(NULL);
    if ( ltime > (WeatherData.last_update_time+LOCAL_HTTP_MIN*60) )
    {
        if ( ltime > (WeatherData.update_time+NETWOEK_WEATHER_MIN*60) )
        {
            // 重新显示天气信息 
            if ( !UpdateWeather() )
            {
                // wifi 可能出问题了 
                int status = WiFi.status();
                if (
                    status == WL_IDLE_STATUS || 
                    status == WL_NO_SSID_AVAIL || 
                    status == WL_CONNECTION_LOST
                    )
                {
                    // 重连wifi
                    bool bNewWifiConfig = false;
                    while( !ConnectWifi() )
                    {
                        ConfigWifi();
                        bNewWifiConfig = true;
                    }

                    // wifi 配置已改,需要重存储新网络信息,并重绘界面 
                    if (bNewWifiConfig)
                    {
                        SaveWifiConfig();

                        // 重绘  
                        create_partition();
                        UpdateWeather();
                        ShowTime(true);
                    }

                    // 如果是原来的wifi重新连接，那么等一会儿再更新天气信息吧 
                }
                else if (status != WL_CONNECTED)
                {
                    Serial.printf("Wifi status: %u\n", status);
                }
            }
        }
    }
    delay(100);
    ShowTaikonaut();

    // 更新时间 
    ShowTime();
    delay(100);
    ShowTaikonaut();
}
