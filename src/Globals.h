#ifndef GLOBALS_H
#define GLOBALS_H
#include <Arduino.h>
#include <FastLED.h>
#include "effects.h"
 
#define DEBUG

#ifdef DEBUG
#define DEBUG_PRINTLN(x)    \
  {                         \
    Serial.print("[");      \
    Serial.print(millis()); \
    Serial.print("] [");    \
    Serial.print(__func__); \
    Serial.print("]: ");    \
    Serial.println(x);      \
  }
#define DEBUG_PRINTF(format, ...)                                                        \
  {                                                                                      \
    String formattedMessage = "[" + String(millis()) + "] [" + String(__func__) + "]: "; \
    Serial.print(formattedMessage);                                                      \
    Serial.printf(format, ##__VA_ARGS__);                                                \
    Serial.println();                                                                    \
  }
#else
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTF(format, ...)
#endif

void formatSettings();
extern const char *uniqueID;
extern const char *VERSION;
extern IPAddress local_IP;
extern IPAddress gateway;
extern IPAddress subnet;
extern IPAddress primaryDNS;
extern IPAddress secondaryDNS;
extern String MQTT_HOST;
extern uint16_t MQTT_PORT;
extern String MQTT_USER;
extern String MQTT_PASS;
extern String MQTT_PREFIX;
extern bool IO_BROKER;
extern bool NET_STATIC;
extern bool SHOW_TIME;
extern bool SHOW_DATE;
extern bool SHOW_WEATHER;
extern bool SHOW_BAT;
extern String HA_PREFIX;
extern bool SHOW_TEMP;
extern bool SHOW_HUM;
extern bool SHOW_SECONDS;
extern bool SHOW_WEEKDAY;
extern int8_t TRANS_EFFECT;
extern String NET_IP;
extern String NET_GW;
extern String NET_SN;
extern String NET_PDNS;
extern String NET_SDNS;
extern long TIME_PER_APP;
extern uint8_t MATRIX_FPS;
extern int TIME_PER_TRANSITION;
extern String NTP_SERVER;
extern String NTP_TZ;
extern bool HA_DISCOVERY;
extern bool UPPERCASE_LETTERS;
extern bool DFPLAYER_ACTIVE;
extern float CURRENT_TEMP;
extern float CURRENT_HUM;
extern float CURRENT_LUX;
extern uint16_t LDR_RAW;
extern String CURRENT_APP;
extern int BACKGROUND_EFFECT;
extern uint8_t BATTERY_PERCENT;
extern uint16_t BATTERY_RAW;
extern float TEMP_OFFSET;
extern float HUM_OFFSET;
extern int BRIGHTNESS;
extern int BRIGHTNESS_PERCENT;
extern String TEXTCOLOR;
extern String TIME_FORMAT;
extern bool AUTO_BRIGHTNESS;
extern bool AP_MODE;
extern bool MATRIX_OFF;
extern uint32_t TEXTCOLOR_888;
extern bool MIRROR_DISPLAY;
extern bool AUTO_TRANSITION;
extern String TIME_FORMAT;
extern String DATE_FORMAT;
extern bool START_ON_MONDAY;
extern bool IS_CELSIUS;
  
#define TEMP_SENSOR_TYPE_NONE 0
#define TEMP_SENSOR_TYPE_BME280 1
#define TEMP_SENSOR_TYPE_HTU21DF 2
#define TEMP_SENSOR_TYPE_BMP280 3
#define TEMP_SENSOR_TYPE_SHT31 4
extern uint8_t TEMP_SENSOR_TYPE;

extern bool SOUND_ACTIVE;
extern String BOOT_SOUND;
extern int TEMP_DECIMAL_PLACES;
extern uint8_t SOUND_VOLUME;
extern int MATRIX_LAYOUT;
extern bool UPDATE_AVAILABLE;
extern long RECEIVED_MESSAGES;
extern uint16_t MIN_BATTERY;
extern uint16_t MAX_BATTERY;
extern CRGB COLOR_CORRECTION;
extern CRGB COLOR_TEMPERATURE;
extern uint32_t WDC_ACTIVE;
extern uint32_t WDC_INACTIVE;
extern float GAMMA;
void loadSettings();
void saveSettings();
extern bool BLOCK_NAVIGATION;
extern bool UPDATE_CHECK;
extern bool SENSOR_READING;
extern bool SENSORS_STABLE;
extern bool ROTATE_SCREEN;
extern long STATS_INTERVAL;
extern uint8_t TIME_MODE;
extern uint8_t SCROLL_SPEED;
extern uint32_t CALENDAR_HEADER_COLOR;
extern uint32_t CALENDAR_TEXT_COLOR;
extern uint32_t CALENDAR_BODY_COLOR;
extern uint32_t TIME_COLOR;
extern uint32_t DATE_COLOR;
extern uint32_t BAT_COLOR;
extern uint32_t TEMP_COLOR;
extern uint32_t HUM_COLOR;
extern bool ARTNET_MODE;
extern bool MOODLIGHT_MODE;
extern double movementFactor;
extern uint8_t MIN_BRIGHTNESS;
extern uint8_t MAX_BRIGHTNESS;
extern float LDR_GAMMA; 
extern float LDR_FACTOR;
extern bool DEBUG_MODE;
extern String AUTH_USER;
extern String AUTH_PASS;
extern String BUTTON_CALLBACK;
extern bool NEWYEAR;
extern bool SWAP_BUTTONS;
extern bool LDR_ON_GROUND;
extern bool GAME_ACTIVE;
extern uint32_t AP_TIMEOUT;
extern OverlayEffect GLOBAL_OVERLAY;
extern String HOSTNAME;
extern int WEB_PORT;
extern bool BUZ_VOL;
extern String DEV_JSON;
#endif // Globals_H
