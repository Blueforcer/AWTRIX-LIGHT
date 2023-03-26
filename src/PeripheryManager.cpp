#include <PeripheryManager.h>
#include <melody_player.h>
#include <melody_factory.h>
#include "Globals.h"
#include "DisplayManager.h"
#include "MQTTManager.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <LightDependentResistor.h>
#include <MenuManager.h>

#define SOUND_OFF false
#define BATTERY_PIN 34
#define BUZZER_PIN 15
#define LDR_PIN 35
#define BUTTON_UP_PIN 26
#define BUTTON_DOWN_PIN 14
#define BUTTON_SELECT_PIN 27

Adafruit_SHT31 sht31;
EasyButton button_left(BUTTON_UP_PIN);
EasyButton button_right(BUTTON_DOWN_PIN);
EasyButton button_select(BUTTON_SELECT_PIN);
MelodyPlayer player(BUZZER_PIN, LOW);

#define USED_PHOTOCELL LightDependentResistor::GL5516

LightDependentResistor photocell(LDR_PIN,
                                 10000,
                                 USED_PHOTOCELL,
                                 10,
                                 10);

int readIndex = 0;
int sampleIndex = 0;
unsigned long previousMillis_BatTempHum = 0;
unsigned long previousMillis_LDR = 0;
const unsigned long interval_BatTempHum = 10000;
const unsigned long interval_LDR = 100;
int total = 0;

const int LDRReadings = 10;
int TotalLDRReadings[LDRReadings];
float sampleSum = 0.0;
float sampleAverage = 0.0;
float brightnessPercent = 0.0;

unsigned long startTime;

// The getter for the instantiated singleton instance
PeripheryManager_ &PeripheryManager_::getInstance()
{
    static PeripheryManager_ instance;
    return instance;
}

// Initialize the global shared instance
PeripheryManager_ &PeripheryManager = PeripheryManager.getInstance();

void left_button_pressed()
{
    DisplayManager.leftButton();
    MenuManager.leftButton();
    MQTTManager.getInstance().clickButton("leftButton","click");
}

void left_button_pressed_long()
{
    MQTTManager.getInstance().clickButton("leftButton","long_click");
}

void left_button_tripple()
{
    MQTTManager.getInstance().clickButton("leftButton","double_click");
}

void right_button_pressed()
{
    DisplayManager.rightButton();
    MenuManager.rightButton();
    MQTTManager.getInstance().clickButton("rightButton","click");
}

void right_button_pressed_long()
{
    MQTTManager.getInstance().clickButton("rightButton","long_click");
}

void right_button_tripple()
{
    MQTTManager.getInstance().clickButton("rightButton","double_click");
}

void select_button_pressed()
{
    DisplayManager.selectButton();
    MenuManager.selectButton();
    MQTTManager.getInstance().clickButton("selectButton","click");
}

void select_button_pressed_long()
{
    DisplayManager.selectButtonLong();
    MenuManager.selectButtonLong();
    MQTTManager.getInstance().clickButton("selectButton","long_click");
}

void select_button_tripple()
{
    if (MATRIX_OFF)
    {
        DisplayManager.MatrixState(true);
    }
    else
    {
        DisplayManager.MatrixState(false);
    }
    MQTTManager.getInstance().clickButton("selectButton","double_click");
}

void PeripheryManager_::playBootSound()
{
    if (SOUND_OFF)
        return;
    const int nNotes = 6;
    String notes[nNotes] = {"E5", "C5", "G4", "E4", "G4", "C5"};
    const int timeUnit = 150;
    // create a melody
    Melody melody = MelodyFactory.load("Nice Melody", timeUnit, notes, nNotes);
    player.playAsync(melody);
}

void PeripheryManager_::stopSound()
{
    player.stop();
}

void PeripheryManager_::playFromFile(String file)
{
    if (SOUND_OFF)
        return;
    Melody melody = MelodyFactory.loadRtttlFile(file);
    player.playAsync(melody);
}

bool PeripheryManager_::isPlaying()
{
    return player.isPlaying();
}

void fistStart()
{

    uint16_t ADCVALUE = analogRead(BATTERY_PIN);

    BATTERY_PERCENT = min((int)map(ADCVALUE, 510, 660, 0, 100), 100);
    sht31.readBoth(&CURRENT_TEMP, &CURRENT_HUM);

    uint16_t LDRVALUE = analogRead(LDR_PIN);
    brightnessPercent = LDRVALUE / 4095.0 * 100.0;
    int brightness = map(brightnessPercent, 0, 100, 10, 120);
    DisplayManager.setBrightness(brightness);
}

void PeripheryManager_::setup()
{
    pinMode(LDR_PIN, INPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    button_left.begin();
    button_right.begin();
    button_select.begin();
    button_left.onPressed(left_button_pressed);
    button_left.onPressedFor(1000, left_button_pressed_long);
    button_left.onSequence(2, 300, left_button_tripple);
    button_right.onPressed(right_button_pressed);
    button_right.onPressedFor(1000, right_button_pressed_long);
    button_right.onSequence(2, 300, right_button_tripple);
    button_select.onPressed(select_button_pressed);
    button_select.onPressedFor(1000, select_button_pressed_long);
    button_select.onSequence(2, 300, select_button_tripple);
    Wire.begin(21, 22);
    sht31.begin(0x44);
    photocell.setPhotocellPositionOnGround(false);
    fistStart();
}

void PeripheryManager_::tick()
{
    button_left.read();
    button_right.read();
    button_select.read();


    unsigned long currentMillis_BatTempHum = millis();
    if (currentMillis_BatTempHum - previousMillis_BatTempHum >= interval_BatTempHum)
    {
        previousMillis_BatTempHum = currentMillis_BatTempHum;
        uint16_t ADCVALUE = analogRead(BATTERY_PIN);
        BATTERY_PERCENT = min((int)map(ADCVALUE, 510, 665, 0, 100), 100);
        CURRENT_LUX = (roundf(photocell.getSmoothedLux() * 1000) / 1000);
        sht31.readBoth(&CURRENT_TEMP, &CURRENT_HUM);
        CURRENT_TEMP -= 9.0;
        checkAlarms();
        MQTTManager.sendStats();
        uint32_t freeHeap = esp_get_free_heap_size(); 
        float freeHeapKB = freeHeap / 1024.0;   
        Serial.print(ESP.getFreeHeap() / 1024);
        Serial.println(" KB");
    }


    unsigned long currentMillis_LDR = millis();
    if (currentMillis_LDR - previousMillis_LDR >= interval_LDR && AUTO_BRIGHTNESS)
    {
        previousMillis_LDR = currentMillis_LDR;
        TotalLDRReadings[sampleIndex] = analogRead(LDR_PIN);
        sampleIndex = (sampleIndex + 1) % LDRReadings;
        sampleSum = 0.0;
        for (int i = 0; i < LDRReadings; i++)
        {
            sampleSum += TotalLDRReadings[i];
        }
        sampleAverage = sampleSum / (float)LDRReadings;

        brightnessPercent = sampleAverage / 4095.0 * 100.0;
        int brightness = map(brightnessPercent, 0, 100, 10, 120);
        BRIGHTNESS = map(brightnessPercent, 0, 100, 0, 255);
        DisplayManager.setBrightness(brightness);
    }
}

void readUptime()
{
    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - startTime;
    int hours = (elapsedTime / 1000) / 3600;
    int minutes = ((elapsedTime / 1000) % 3600) / 60;
    int seconds = (elapsedTime / 1000) % 60;
    char timeString[10];
    sprintf(timeString, "%02d:%02d:%02d", hours, minutes, seconds);
}

const int MIN_ALARM_INTERVAL = 60; // 1 Minute
time_t lastAlarmTime = 0;

void PeripheryManager_::checkAlarms()
{
    File file = LittleFS.open("/alarms.json", "r");
    if (!file)
    {
        return;
    }

    DynamicJsonDocument doc(file.size() * 1.33);
    DeserializationError error = deserializeJson(doc, file);
    if (error)
    {
        Serial.println("Failed to read Alarm file");
        return;
    }
    JsonArray alarms = doc["alarms"];
    file.close();

    time_t now1 = time(nullptr);
    struct tm *timeInfo;
    timeInfo = localtime(&now1);
    int currentHour = timeInfo->tm_hour;
    int currentMinute = timeInfo->tm_min;
    int currentDay = timeInfo->tm_wday - 1;

    for (JsonObject alarm : alarms)
    {
        int alarmHour = alarm["hour"];
        int alarmMinute = alarm["minute"];
        String alarmDays = alarm["days"];

        if (currentHour == alarmHour && currentMinute == alarmMinute && alarmDays.indexOf(String(currentDay)) != -1)
        {
            if (difftime(now1, lastAlarmTime) < MIN_ALARM_INTERVAL)
            {
                return;
            }

            ALARM_ACTIVE = true;
            lastAlarmTime = now1;

            if (alarm.containsKey("sound"))
            {
                ALARM_SOUND = alarm["sound"].as<String>();
            }
            else
            {
                ALARM_SOUND = "";
            }

            if (alarm.containsKey("snooze"))
            {
                SNOOZE_TIME = alarm["snooze"].as<uint8_t>();
            }
            else
            {
                SNOOZE_TIME = 0;
            }
        }
    }
}
