#ifndef _USER_CONDIG_
#define _USER_CONDIG_

// wifi 账号密码最大长度  
#define WIFI_STR_LEN    255

// 热点名
#define AP_SSID         "ESP32C3"
#define AP_PASSWORD     ""

// 城市列表（和风天气 LocationID，按键循环切换）
typedef struct {
    const char* name;      // 显示名称
    const char* location;  // 和风天气 LocationID
} city_info;

#define CITY_COUNT 5
city_info CITIES[CITY_COUNT] = {
    {"南京栖霞", "101190112"},
    {"南京江宁", "101190104"},
    {"合肥瑶海", "101220107"},
    {"安庆潜山", "101220604"},
    {"安庆桐城", "101220609"},
};

// 城市切换按键：板载 BOOT 键 GPIO9（免接线），可改为 IO00 / IO01
#define BUTTON_PIN      9

// 时间同步服务器（阿里云 NTP，国内更快更稳；备选 ntp1~ntp7.aliyun.com）
#define NTP_HOST        "ntp.aliyun.com"

// 时区 
#define TIME_ZONE        8
#define LOCAL_HTTP_MIN   2     // 本地http间隔2分钟  
#define NETWOEK_WEATHER_MIN    35   // 网络上天气间隔时间35分 

// 屏幕区域 
#define  TFT_WIDTH      240 
#define  TFT_HEIGHT     240 
#define  SUPERIOR       60   // 上边高度 
#define  SUPERIOR_LEFT  155  // 上面左边区域宽度 
#define  SUPERIOR_RIGHT 72   // 上面右边区域宽度 
#define  MIDIUM         135  // 中间高度 
#define  INFERIOR       45   // 下面高度 
#define  INFERIOR_LEFT  125  // 下面高度 
#define  INFERIOR_RIGHT 115   // 下面高度 
#define  LINE_WIDTH     3    // 线宽度 

// 等宽字体 
#define  MSYHL25        25 
#define  NI7SEG45       45 

// 板载小灯 
#define  LED1           12  
#define  LED2           13

// 配置wifi页面 
String FrontHtml = "<!DOCTYPE html><head><meta charset='UTF-8'>"\
    "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
    "<title>config wifi</title>"
    "<style type='text/css'>body{background-color:rgb(225,250,250);font-size:18px;font-weight:600;}"
    ".formbox{position:relative;height:200px;width:350px;top:150px;margin:0 auto;background:#fff;border:5px solid #ccc;}"
    ".lineblock{display:block;}"\
    ".middle{max-width:400px; text-align:center;}"\
    ".inputbox{display:inline-block;margin:10px 20px;height:60px;text-align:left;}"\
    ".ssidinput{margin:15px 0px;}"\
    ".submitbtn{margin:-30px auto;}"\
    ".connectBtn{width:50%;height:40px;border-radius:5px;background:#4ad0f1;font-size:20px;font-weight:600;}</style>"\
    "<script>function SelectWifi(){var select=document.getElementById('SelectSSID');var tsw=document.getElementById('tsw');if (select.options[select.selectedIndex].value=='Other'){tsw.style.display='inline-block';}else{tsw.style.display='none';}}</script>"\
    "</head><body onload='SelectWifi();'><div class='formbox'>"\
    "<form name='input' action='/' method='POST'><div class='lineblock middle inputbox'>"\
    "<div class=''>选择Wifi: <select id='SelectSSID' name='SelectSSID' onchange='SelectWifi();'>";

String AfterHtml = "<option value='Other'>隐藏的网络</option></select></div>"\
    "<div id='tsw' class='ssidinput'>SSID: <input id='wifiname' type='text' name='ssid'>"\
    "</div></div><div class='lineblock middle inputbox'>密码: <input type='text' name='password'>"\
    "</div><div class='lineblock middle submitbtn'><input class='connectBtn' type='submit' value='连接'>"\
    "</div></form></div></body></html>";

#endif // _USER_CONDIG_ 