#include <DisplayManager.h>
#include "MatrixDisplayUi.h"
#include <TJpg_Decoder.h>
#include "icons.h"
#include "Globals.h"
#include "PeripheryManager.h"
#include "MQTTManager.h"
#include "GifPlayer.h"
#include <Ticker.h>
#include "timer.h"
#include "Functions.h"
#include "ServerManager.h"
#include "MenuManager.h"
#include "Apps.h"
#include "effects.h"
#include "Overlays.h"
#include "Dictionary.h"
#include <set>
#include "GifPlayer.h"
#include <ArtnetWifi.h>
#include <AwtrixFont.h>
#include <HTTPClient.h>
#include "base64.hpp"
#include "Games/GameManager.h"

unsigned long lastArtnetStatusTime = 0;
const int numberOfChannels = 256 * 3;
// Artnet settings
ArtnetWifi artnet;
const int startUniverse = 0; // CHANGE FOR YOUR SETUP most software this is 1, some software send out artnet first universe as 0.

// Check if we got all universes
const int maxUniverses = numberOfChannels / 512 + ((numberOfChannels % 512) ? 1 : 0);
bool universesReceived[maxUniverses];
bool sendFrame = 1;
int previousDataLength = 0;
#ifdef awtrix2_upgrade
#define MATRIX_PIN D2
#else
#define MATRIX_PIN 32
#endif

#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 8
fs::File gifFile;
GifPlayer gif;

uint16_t gifX, gifY;
CRGB leds[MATRIX_WIDTH * MATRIX_HEIGHT];
CRGB ledsCopy[MATRIX_WIDTH * MATRIX_HEIGHT];
float actualBri;
int16_t cursor_x, cursor_y;
uint32_t textColor;

// NeoMatrix
FastLED_NeoMatrix *matrix = new FastLED_NeoMatrix(leds, 8, 8, 4, 1, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_PROGRESSIVE);
MatrixDisplayUi *ui = new MatrixDisplayUi(matrix);

DisplayManager_ &DisplayManager_::getInstance()
{
  static DisplayManager_ instance;
  return instance;
}

DisplayManager_ &DisplayManager = DisplayManager.getInstance();

void DisplayManager_::setBrightness(int bri)
{
  bool wakeup;
  if (!notifications.empty())
  {
    wakeup = notifications[0].wakeup;
  }

  if (MATRIX_OFF && !wakeup)
  {
    matrix->setBrightness(0);
  }
  else
  {
    matrix->setBrightness(bri);
    actualBri = bri;
  }
}

bool DisplayManager_::setAutoTransition(bool active)
{
  if (ui->AppCount < 2)
  {
    ui->disablesetAutoTransition();
    return false;
  }
  if (active && AUTO_TRANSITION)
  {
    ui->enablesetAutoTransition();
    return true;
  }
  else
  {
    ui->disablesetAutoTransition();
    return false;
  }
}

void DisplayManager_::drawJPG(uint16_t x, uint16_t y, fs::File jpgFile)
{
  TJpgDec.drawFsJpg(x, y, jpgFile);
}

void DisplayManager_::drawJPG(int32_t x, int32_t y, const uint8_t jpeg_data[], uint32_t data_size)
{
  TJpgDec.drawJpg(x, y, jpeg_data, data_size);
}

void DisplayManager_::drawBMP(int16_t x, int16_t y, const uint16_t bitmap[], int16_t w, int16_t h)
{
  matrix->drawRGBBitmap(y, x, bitmap, w, h);
}

void DisplayManager_::applyAllSettings()
{
  ui->setTargetFPS(MATRIX_FPS);
  ui->setTimePerApp(TIME_PER_APP);
  ui->setTimePerTransition(TIME_PER_TRANSITION);

  if (!AUTO_BRIGHTNESS)
    setBrightness(BRIGHTNESS);
  setTextColor(TEXTCOLOR_888);
  setAutoTransition(AUTO_TRANSITION);
}

void DisplayManager_::setAppTime(long duration)
{
  ui->setTimePerApp(duration);
}

void DisplayManager_::resetTextColor()
{
  setTextColor(TEXTCOLOR_888);
}

void DisplayManager_::clearMatrix()
{
  matrix->clear();
  matrix->show();
}

bool jpg_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
  uint16_t bitmapIndex = 0;
  for (uint16_t row = 0; row < h; row++)
  {
    for (uint16_t col = 0; col < w; col++)
    {
      matrix->drawPixel(x + col, y + row, bitmap[bitmapIndex++]);
    }
  }
  return 0;
}

void DisplayManager_::printText(int16_t x, int16_t y, const char *text, bool centered, byte textCase)
{

  if (centered)
  {
    uint16_t textWidth = getTextWidth(text, textCase);
    int16_t textX = ((32 - textWidth) / 2);
    setCursor(textX, y);
  }
  else
  {
    setCursor(x, y);
  }

  if ((UPPERCASE_LETTERS && textCase == 0) || textCase == 1)
  {
    size_t length = strlen(text);
    char upperText[length + 1]; // +1 for the null terminator

    for (size_t i = 0; i < length; ++i)
    {
      upperText[i] = toupper(text[i]);
    }

    upperText[length] = '\0'; // Null terminator
    matrixPrint(upperText);
  }
  else
  {
    matrixPrint(text);
  }
}

void DisplayManager_::HSVtext(int16_t x, int16_t y, const char *text, bool clear, byte textCase)
{
  if (clear)
    matrix->clear();
  static uint8_t hueOffset = 0;
  uint16_t xpos = 0;
  for (uint16_t i = 0; i < strlen(text); i++)
  {
    uint8_t hue = map(i, 0, strlen(text), 0, 360) + hueOffset;
    setTextColor(hsvToRgb(hue, 255, 255));
    const char *myChar = &text[i];

    setCursor(xpos + x, y);
    if ((UPPERCASE_LETTERS && textCase == 0) || textCase == 1)
    {
      matrixPrint((char)toupper(text[i]));
    }
    else
    {
      matrixPrint(text[i]);
    }
    char temp_str[2] = {'\0', '\0'};
    temp_str[0] = text[i];
    xpos += getTextWidth(temp_str, textCase);
  }
  hueOffset++;
  if (clear)
    matrix->show();
}

uint32_t interpolateColor(uint32_t color1, uint32_t color2, float t)
{
  if (t <= 0.0f)
    return color1;
  if (t >= 1.0f)
    return color2;

  uint8_t r1 = (color1 >> 16) & 0xFF; // R-Komponente aus Farbe 1
  uint8_t g1 = (color1 >> 8) & 0xFF;  // G-Komponente aus Farbe 1
  uint8_t b1 = color1 & 0xFF;         // B-Komponente aus Farbe 1

  uint8_t r2 = (color2 >> 16) & 0xFF; // R-Komponente aus Farbe 2
  uint8_t g2 = (color2 >> 8) & 0xFF;  // G-Komponente aus Farbe 2
  uint8_t b2 = color2 & 0xFF;         // B-Komponente aus Farbe 2

  // Interpolation für jede Farbkomponente
  uint8_t r_interp = r1 + (r2 - r1) * t;
  uint8_t g_interp = g1 + (g2 - g1) * t;
  uint8_t b_interp = b1 + (b2 - b1) * t;

  return (r_interp << 16) | (g_interp << 8) | b_interp;
}

void DisplayManager_::GradientText(int16_t x, int16_t y, const char *text, int color1, int color2, bool clear, byte textCase)
{
  if (clear)
    matrix->clear();

  uint16_t xpos = 0;
  uint16_t textLength = strlen(text);

  for (uint16_t i = 0; i < textLength; i++)
  {
    // Bestimme den Interpolationswert basierend auf der aktuellen Position i im Text
    float t = (float)i / (textLength - 1);

    // Bestimme die Farbe für das aktuelle Zeichen basierend auf dem Farbverlauf
    uint32_t TC = interpolateColor(color1, color2, t);
    setTextColor(TC);

    setCursor(xpos + x, y);
    if ((UPPERCASE_LETTERS && textCase == 0) || textCase == 1)
    {
      matrixPrint((char)toupper(text[i]));
    }
    else
    {
      matrixPrint(text[i]);
    }

    char temp_str[2] = {'\0', '\0'};
    temp_str[0] = text[i];
    xpos += getTextWidth(temp_str, textCase);
  }

  if (clear)
    matrix->show();
}

void pushCustomApp(String name, int position)
{
  if (customApps.count(name) == 0)
  {
    int availableCallbackIndex = -1;

    for (int i = 0; i < 20; ++i)
    {
      bool callbackUsed = false;

      for (const auto &appPair : Apps)
      {
        if (appPair.second == customAppCallbacks[i])
        {
          callbackUsed = true;
          break;
        }
      }

      if (!callbackUsed)
      {
        availableCallbackIndex = i;
        break;
      }
    }

    if (availableCallbackIndex == -1)
    {
      if (DEBUG_MODE)
        DEBUG_PRINTLN(F("Error adding custom app -> Maximum number of custom apps reached"));
      return;
    }

    if (position < 0) // Insert at the end of the vector
    {
      Apps.push_back(std::make_pair(name, customAppCallbacks[availableCallbackIndex]));
    }
    else if (position < Apps.size()) // Insert at a specific position
    {
      Apps.insert(Apps.begin() + position, std::make_pair(name, customAppCallbacks[availableCallbackIndex]));
    }
    else // Invalid position, Insert at the end of the vector
    {
      Apps.push_back(std::make_pair(name, customAppCallbacks[availableCallbackIndex]));
    }

    ui->setApps(Apps); // Add Apps
    DisplayManager.getInstance().setAutoTransition(true);
  }
}

bool deleteCustomAppFile(const String &name)
{
  // Create the file name based on the app name
  String fileName = "/CUSTOMAPPS/" + name + ".json";

  // Check if the file exists
  if (!LittleFS.exists(fileName))
  {
    if (DEBUG_MODE)
      DEBUG_PRINTLN("File does not exist: " + fileName + ". No need to delete it");
    return false;
  }

  // Delete the file
  if (LittleFS.remove(fileName))
  {
    if (DEBUG_MODE)
      DEBUG_PRINTLN("File removed successfully: " + fileName);
    return true;
  }
  else
  {
    if (DEBUG_MODE)
      DEBUG_PRINTLN("Failed to remove file: " + fileName);
    return false;
  }
}

void removeCustomAppFromApps(const String &name, bool setApps)
{
  // Remove apps from Apps list
  auto it = Apps.begin();
  while (it != Apps.end())
  {
    if (it->first.startsWith(name))
    {
      it = Apps.erase(it);
    }
    else
    {
      ++it;
    }
  }

  // Remove apps from customApps map
  auto mapIt = customApps.begin();
  while (mapIt != customApps.end())
  {
    if (mapIt->first.startsWith(name))
    {
      mapIt = customApps.erase(mapIt);
    }
    else
    {
      ++mapIt;
    }
  }

  if (setApps)
    ui->setApps(Apps);
  DisplayManager.getInstance().setAutoTransition(true);
  deleteCustomAppFile(name);
  DisplayManager.setAppTime(TIME_PER_APP);
}

bool parseFragmentsText(const JsonArray &fragmentArray, std::vector<uint32_t> &colors, std::vector<String> &fragments, uint32_t standardColor)
{
  colors.clear();
  fragments.clear();

  for (JsonObject fragmentObj : fragmentArray)
  {
    String textFragment = fragmentObj["t"];
    uint32_t color;
    if (fragmentObj.containsKey("c"))
    {
      auto fragColor = fragmentObj["c"];
      color = getColorFromJsonVariant(fragColor, standardColor);
    }
    else
    {
      color = standardColor;
    }

    fragments.push_back(utf8ascii(textFragment));
    colors.push_back(color);
  }
  return true;
}

bool DisplayManager_::parseCustomPage(const String &name, const char *json, bool preventSave)
{
  if ((strcmp(json, "") == 0) || (strcmp(json, "{}") == 0))
  {
    removeCustomAppFromApps(name, true);
    return true;
  }

  DynamicJsonDocument doc(8192);
  DeserializationError error = deserializeJson(doc, json);
  if (error)
  {
    DEBUG_PRINTLN(error.c_str());
    doc.clear();
    return false;
  }

  if (doc.is<JsonObject>())
  {
    JsonObject rootObj = doc.as<JsonObject>();
    return generateCustomPage(name, rootObj, preventSave);
  }
  else if (doc.is<JsonArray>())
  {
    JsonArray customPagesArray = doc.as<JsonArray>();
    int cpIndex = 0;
    for (JsonObject customPageObject : customPagesArray)
    {
      generateCustomPage(name + String(cpIndex), customPageObject, preventSave);
      ++cpIndex;
    }
  }

  doc.clear();
  return true;
}

// Function to subscribe to MQTT topics based on placeholders in text
void subscribeToPlaceholders(String text)
{
  int start = 0;
  while ((start = text.indexOf("{{", start)) != -1)
  {
    int end = text.indexOf("}}", start);
    if (end == -1)
    {
      break;
    }
    String placeholder = text.substring(start + 2, end);
    String topic = placeholder;

    MQTTManager.subscribe(topic.c_str());

    start = end + 2;
  }
}

bool DisplayManager_::generateCustomPage(const String &name, JsonObject doc, bool preventSave)
{
  CustomApp customApp;

  if (customApps.find(name) != customApps.end())
  {
    customApp = customApps[name];
  }

  customApp.progress = doc.containsKey("progress") ? doc["progress"].as<int>() : -1;

  if (doc.containsKey("background"))
  {
    auto background = doc["background"];
    customApp.background = getColorFromJsonVariant(background, 0);
  }
  else
  {
    customApp.background = 0;
  }

  if (doc.containsKey("save") && preventSave == false)
  {
    bool saveApp = doc["save"].as<bool>();
    if (saveApp)
    {
      // Create the directory if it doesn't exist
      if (!LittleFS.exists("/CUSTOMAPPS"))
      {
        LittleFS.mkdir("/CUSTOMAPPS");
      }

      // Open the file for writing (this will overwrite the file if it already exists)
      File file = LittleFS.open("/CUSTOMAPPS/" + name + ".json", "w");
      if (!file)
      {

        return false;
      }

      // Write the new custom app to the file
      serializeJson(doc, file);
      file.close();
    }
  }

  if (doc.containsKey("progressC"))
  {
    auto progressC = doc["progressC"];
    customApp.pColor = getColorFromJsonVariant(progressC, 0x00FF00);
  }
  else
  {
    customApp.pColor = 0x00FF00;
  }

  if (doc.containsKey("progressBC"))
  {
    auto progressBC = doc["progressBC"];
    customApp.pbColor = getColorFromJsonVariant(progressBC, 0xFFFFFF);
  }
  else
  {
    customApp.pbColor = 0xFFFFFF;
  }

  bool autoscale = true;
  if (doc.containsKey("autoscale"))
  {
    autoscale = doc["autoscale"].as<bool>();
  }

  // Handling for "bar" and "line" as they have similar structures
  const char *dataKeys[] = {"bar", "line"};
  int *dataArrays[] = {customApp.barData, customApp.lineData};
  int *dataSizeArrays[] = {&customApp.barSize, &customApp.lineSize};

  for (int i = 0; i < 2; i++)
  {
    const char *key = dataKeys[i];
    int *dataArray = dataArrays[i];
    int *dataSize = dataSizeArrays[i];

    if (doc.containsKey(key))
    {
      if (doc.containsKey("barBC"))
      {
        auto color = doc["barBC"];
        customApp.barBG = getColorFromJsonVariant(color, 0);
      }
      else
      {
        customApp.barBG = 0;
      }
      JsonArray data = doc[key];
      int index = 0;
      int maximum = 0;
      for (JsonVariant v : data)
      {
        if (index >= 16)
        {
          break;
        }
        int d = v.as<int>();
        if (d > maximum)
        {
          maximum = d;
        }
        dataArray[index] = d;
        index++;
      }
      *dataSize = index;

      if (autoscale)
      {
        for (int j = 0; j < *dataSize; j++)
        {
          dataArray[j] = map(dataArray[j], 0, maximum, 0, 8);
        }
      }
    }
    else
    {
      *dataSize = 0;
    }
  }

  if (doc.containsKey("draw"))
  {
    customApp.drawInstructions = doc["draw"].as<String>();
  }
  else
  {
    customApp.drawInstructions = "";
  }

  if (doc.containsKey("effect"))
  {
    customApp.effect = getEffectIndex(doc["effect"].as<String>());
    if (doc.containsKey("effectSettings"))
    {
      updateEffectSettings(customApp.effect, doc["effectSettings"].as<String>());
    }
  }
  customApp.duration = doc.containsKey("duration") ? doc["duration"].as<long>() * 1000 : 0;
  int pos = doc.containsKey("pos") ? doc["pos"].as<uint8_t>() : -1;
  customApp.rainbow = doc.containsKey("rainbow") ? doc["rainbow"] : false;
  customApp.pushIcon = doc.containsKey("pushIcon") ? doc["pushIcon"] : 0;

  customApp.textCase = doc.containsKey("textCase") ? doc["textCase"] : 0;

  if (doc.containsKey("lifetime"))
  {
    customApp.lifetime = doc["lifetime"];
    customApp.lifetimeMode = doc.containsKey("lifetimeMode") ? doc["lifetimeMode"] : 0;
  }
  else
  {
    customApp.lifetime = 0;
  }

  customApp.bounce = doc.containsKey("bounce") ? doc["bounce"].as<bool>() : false;
  customApp.iconOffset = doc.containsKey("iconOffset") ? doc["iconOffset"] : 0;
  customApp.textOffset = doc.containsKey("textOffset") ? doc["textOffset"] : 0;
  customApp.scrollSpeed = doc.containsKey("scrollSpeed") ? doc["scrollSpeed"].as<int>() : -1;
  customApp.topText = doc.containsKey("topText") ? doc["topText"].as<bool>() : false;
  customApp.fade = doc.containsKey("fadeText") ? doc["fadeText"].as<int>() : 0;
  customApp.blink = doc.containsKey("blinkText") ? doc["blinkText"].as<int>() : 0;
  customApp.center = doc.containsKey("center") ? doc["center"].as<bool>() : true;
  customApp.noScrolling = doc.containsKey("noScroll") ? doc["noScroll"] : false;
  customApp.name = name;

  customApp.overlay = doc.containsKey("overlay") ? getOverlay(doc["overlay"].as<String>()) : NONE;

  if (doc.containsKey("icon"))
  {
    String newIconName = doc["icon"].as<String>();

    if (newIconName.length() > 64)
    {
      customApp.jpegDataSize = decode_base64((const unsigned char *)newIconName.c_str(), customApp.jpegDataBuffer);
      customApp.isGif = false;
      customApp.icon.close();
      customApp.iconName = "";
      customApp.iconPosition = 0;
      customApp.currentFrame = 0;
    }
    else if (customApp.iconName != newIconName)
    {
      customApp.jpegDataSize = 0;
      customApp.iconName = newIconName;
      customApp.icon.close();
      customApp.iconPosition = 0;
      customApp.currentFrame = 0;
    }
  }
  else
  {
    customApp.jpegDataSize = 0;
    customApp.icon.close();
    customApp.iconName = "";
    customApp.iconPosition = 0;
    customApp.currentFrame = 0;
  }

  customApp.gradient[0] = -1;
  customApp.gradient[1] = -1;

  if (doc.containsKey("gradient"))
  {
    JsonArray arr = doc["gradient"].as<JsonArray>();
    if (arr.size() == 2)
    {
      auto color1 = arr[0];
      auto color2 = arr[1];

      customApp.gradient[0] = getColorFromJsonVariant(color1, TEXTCOLOR_888);
      customApp.gradient[1] = getColorFromJsonVariant(color2, TEXTCOLOR_888);
    }
  }

  if (doc.containsKey("color"))
  {
    customApp.hasCustomColor = true;
    auto color = doc["color"];
    customApp.color = getColorFromJsonVariant(color, TEXTCOLOR_888);
  }
  else
  {
    customApp.hasCustomColor = false;
    customApp.color = TEXTCOLOR_888;
  }

  customApp.colors.clear();
  customApp.fragments.clear();

  if (doc.containsKey("text") && doc["text"].is<JsonArray>())
  {
    JsonArray textArray = doc["text"].as<JsonArray>();
    parseFragmentsText(textArray, customApp.colors, customApp.fragments, customApp.color);
  }
  else if (doc.containsKey("text"))
  {
    String text = doc["text"].as<String>();
    subscribeToPlaceholders(utf8ascii(text));
    customApp.text = utf8ascii(text);
  }
  else
  {
    customApp.text = "";
  }

  if (currentCustomApp != name)
  {
    customApp.scrollposition = 9 + customApp.textOffset;
  }

  customApp.repeat = doc.containsKey("repeat") ? doc["repeat"].as<int>() : -1;
  if (customApp.noScrolling)
  {
    customApp.repeat = -1;
  }

  customApp.lastUpdate = millis();
  customApp.lifeTimeEnd = false;
  doc.clear();
  pushCustomApp(name, pos - 1);
  customApps[name] = customApp;

  return true;
}

bool DisplayManager_::generateNotification(uint8_t source, const char *json)
{
  // source: 0=MQTT, 1=HTTP
  DynamicJsonDocument doc(6144);
  DeserializationError error = deserializeJson(doc, json);
  if (error)
  {
    DEBUG_PRINTLN(error.c_str());
    doc.clear();
    return false;
  }

  Notification newNotification;

  newNotification.progress = doc.containsKey("progress") ? doc["progress"].as<int>() : -1;

  if (doc.containsKey("progressC"))
  {
    auto progressC = doc["progressC"];
    newNotification.pColor = getColorFromJsonVariant(progressC, 0x00FF00);
  }
  else
  {
    newNotification.pColor = 0x00FF00;
  }

  if (doc.containsKey("progressBC"))
  {
    auto progressBC = doc["progressBC"];
    newNotification.pbColor = getColorFromJsonVariant(progressBC, 0xFFFFFF);
  }
  else
  {
    newNotification.pbColor = 0xFFFFFF;
  }

  if (doc.containsKey("background"))
  {
    auto background = doc["background"];
    newNotification.background = getColorFromJsonVariant(background, 0);
  }
  else
  {
    newNotification.background = 0x000000;
  }

  if (doc.containsKey("draw"))
  {
    newNotification.drawInstructions = doc["draw"].as<String>();
  }
  else
  {
    newNotification.drawInstructions = "";
  }

  if (doc.containsKey("effect"))
  {
    newNotification.effect = getEffectIndex(doc["effect"].as<String>());
    if (doc.containsKey("effectSettings"))
    {
      updateEffectSettings(newNotification.effect, doc["effectSettings"].as<String>());
    }
  }

  newNotification.overlay = doc.containsKey("overlay") ? getOverlay(doc["overlay"].as<String>()) : NONE;
  newNotification.loopSound = doc.containsKey("loopSound") ? doc["loopSound"].as<bool>() : false;
  newNotification.center = doc.containsKey("center") ? doc["center"].as<bool>() : true;
  newNotification.sound = doc.containsKey("sound") ? doc["sound"].as<String>() : "";
  newNotification.rtttl = doc.containsKey("rtttl") ? doc["rtttl"].as<String>() : "";
  newNotification.duration = doc.containsKey("duration") ? doc["duration"].as<long>() * 1000 : TIME_PER_APP;
  newNotification.rainbow = doc.containsKey("rainbow") ? doc["rainbow"].as<bool>() : false;
  newNotification.hold = doc.containsKey("hold") ? doc["hold"].as<bool>() : false;
  newNotification.scrollSpeed = doc.containsKey("scrollSpeed") ? doc["scrollSpeed"].as<int>() : -1;
  newNotification.wakeup = doc.containsKey("wakeup") ? doc["wakeup"].as<bool>() : false;
  newNotification.pushIcon = doc.containsKey("pushIcon") ? doc["pushIcon"] : 0;
  newNotification.iconOffset = doc.containsKey("iconOffset") ? doc["iconOffset"] : 0;
  newNotification.textCase = doc.containsKey("textCase") ? doc["textCase"] : 0;
  newNotification.textOffset = doc.containsKey("textOffset") ? doc["textOffset"] : 0;
  newNotification.topText = doc.containsKey("topText") ? doc["topText"].as<bool>() : false;
  newNotification.noScrolling = doc.containsKey("noScroll") ? doc["noScroll"] : false;
  newNotification.repeat = doc.containsKey("repeat") ? doc["repeat"].as<int>() : -1;
  newNotification.fade = doc.containsKey("fadeText") ? doc["fadeText"].as<int>() : 0;
  newNotification.blink = doc.containsKey("blinkText") ? doc["blinkText"].as<int>() : 0;
  newNotification.barSize = 0;
  newNotification.lineSize = 0;
  if (newNotification.noScrolling)
  {
    newNotification.repeat = -1;
  }
  newNotification.scrollposition = 9 + newNotification.textOffset;
  newNotification.iconWasPushed = false;
  newNotification.iconPosition = 0;
  newNotification.scrollDelay = 0;

  newNotification.gradient[0] = -1;
  newNotification.gradient[1] = -1;
  if (doc.containsKey("gradient"))
  {
    JsonArray arr = doc["gradient"].as<JsonArray>();
    if (arr.size() == 2)
    {
      auto color1 = arr[0];
      auto color2 = arr[1];

      newNotification.gradient[0] = getColorFromJsonVariant(color1, TEXTCOLOR_888);
      newNotification.gradient[1] = getColorFromJsonVariant(color2, TEXTCOLOR_888);
    }
  }

  bool autoscale = true;
  if (doc.containsKey("autoscale"))
  {
    autoscale = doc["autoscale"].as<bool>();
  }

  // Handling for "bar" and "line" as they have similar structures
  const char *dataKeys[] = {"bar", "line"};
  int *dataArrays[] = {newNotification.barData, newNotification.lineData};
  int *dataSizeArrays[] = {&newNotification.barSize, &newNotification.lineSize};

  for (int i = 0; i < 2; i++)
  {
    const char *key = dataKeys[i];
    int *dataArray = dataArrays[i];
    int *dataSize = dataSizeArrays[i];

    if (doc.containsKey(key))
    {

      if (doc.containsKey("barBC"))
      {
        auto color = doc["barBC"];
        newNotification.barBG = getColorFromJsonVariant(color, 0);
      }
      JsonArray data = doc[key];
      int index = 0;
      int maximum = 0;
      for (JsonVariant v : data)
      {
        if (index >= 16)
        {
          break;
        }
        int d = v.as<int>();
        if (d > maximum)
        {
          maximum = d;
        }
        dataArray[index] = d;
        index++;
      }
      *dataSize = index;

      if (autoscale)
      {
        for (int j = 0; j < *dataSize; j++)
        {
          dataArray[j] = map(dataArray[j], 0, maximum, 0, 8);
        }
      }
    }
    else
    {
      *dataSize = 0;
    }
  }

  if (doc.containsKey("color"))
  {
    auto color = doc["color"];
    newNotification.color = getColorFromJsonVariant(color, TEXTCOLOR_888);
  }
  else
  {
    newNotification.color = TEXTCOLOR_888;
  }

  if (doc.containsKey("text") && doc["text"].is<JsonArray>())
  {
    JsonArray textArray = doc["text"].as<JsonArray>();
    parseFragmentsText(textArray, newNotification.colors, newNotification.fragments, newNotification.color);
  }
  else if (doc["text"].is<String>())
  {
    String text = doc["text"];
    newNotification.text = utf8ascii(text);
  }
  else
  {
    newNotification.text = "";
  }

  if (doc.containsKey("icon"))
  {
    String iconValue = doc["icon"].as<String>();

    if (iconValue.length() > 64)
    {
      newNotification.jpegDataSize = decode_base64((const unsigned char *)iconValue.c_str(), newNotification.jpegDataBuffer);
      newNotification.isGif = false;
    }
    else
    {
      newNotification.jpegDataSize = 0;
      if (LittleFS.exists("/ICONS/" + iconValue + ".jpg"))
      {
        newNotification.isGif = false;
        newNotification.icon = LittleFS.open("/ICONS/" + iconValue + ".jpg");
      }
      else if (LittleFS.exists("/ICONS/" + iconValue + ".gif"))
      {
        newNotification.isGif = true;
        newNotification.icon = LittleFS.open("/ICONS/" + iconValue + ".gif");
      }
      else
      {
        fs::File nullPointer;
        newNotification.icon = nullPointer;
        newNotification.isGif = false;
      }
    }
  }
  else
  {
    fs::File nullPointer;
    newNotification.icon = nullPointer;
    newNotification.jpegDataSize = 0;
    newNotification.isGif = false;
  }

  if (doc.containsKey("clients"))
  {
    JsonArray ClientNames = doc["clients"];
    doc.remove("clients");
    String modifiedJson;
    serializeJson(doc, modifiedJson);
    for (JsonVariant c : ClientNames)
    {
      String client = c.as<String>();
      if (source == 0)
      {
        MQTTManager.rawPublish(client.c_str(), "notify", modifiedJson.c_str());
      }
      else
      {
        HTTPClient http;
        http.begin("http://" + client + "/api/notify");
        http.POST(modifiedJson);
        http.end();
      }
    }
  }
  newNotification.startime = millis();
  CURRENT_APP = "Notification";
  MQTTManager.setCurrentApp(CURRENT_APP);

  bool stack = doc.containsKey("stack") ? doc["stack"] : true;

  if (stack)
  {
    notifications.push_back(newNotification);
  }
  else
  {
    if (notifications.empty())
    {
      notifications.push_back(newNotification);
    }
    else
    {
      notifications[0] = newNotification;
    }
  }

  doc.clear();
  return true;
}

void DisplayManager_::loadCustomApps()
{
  File root = LittleFS.open("/CUSTOMAPPS");

  if (!root)
  {
    if (DEBUG_MODE)
      DEBUG_PRINTLN("Failed to open directory");
    return;
  }
  if (!root.isDirectory())
  {
    if (DEBUG_MODE)
      DEBUG_PRINTLN("/CUSTOMAPPS is not a directory");
    return;
  }

  File file = root.openNextFile();

  while (file)
  {
    if (file.isDirectory())
    {
      file = root.openNextFile();
      continue;
    }

    String fileName = file.name();
    String json = "";
    while (file.available())
    {
      json += char(file.read());
    }
    file.close();

    String name = fileName.substring(fileName.lastIndexOf('/') + 1, fileName.lastIndexOf('.')); // remove path and .json extension
    parseCustomPage(name, json.c_str(), true);

    file = root.openNextFile();
  }
}

void DisplayManager_::loadNativeApps()
{
  // Define a helper function to check and update an app
  auto updateApp = [&](const String &name, AppCallback callback, bool show, size_t position)
  {
    auto it = std::find_if(Apps.begin(), Apps.end(), [&](const std::pair<String, AppCallback> &app)
                           { return app.first == name; });
    if (it != Apps.end())
    {
      if (!show)
      {
        Apps.erase(it);
      }
    }
    else
    {
      if (show)
      {
        if (position >= Apps.size())
        {
          Apps.push_back(std::make_pair(name, callback));
        }
        else
        {
          Apps.insert(Apps.begin() + position, std::make_pair(name, callback));
        }
      }
    }
  };

  updateApp("Time", TimeApp, SHOW_TIME, 0);
  updateApp("Date", DateApp, SHOW_DATE, 1);

  if (SENSOR_READING)
  {
    updateApp("Temperature", TempApp, SHOW_TEMP, 2);
    updateApp("Humidity", HumApp, SHOW_HUM, 3);
  }
#ifdef ULANZI
  updateApp("Battery", BatApp, SHOW_BAT, 4);
#endif

  ui->setApps(Apps);
  setAutoTransition(true);
}

void DisplayManager_::setup()
{
  TJpgDec.setCallback(jpg_output);
  TJpgDec.setJpgScale(1);
  random16_set_seed(millis());
  FastLED.addLeds<NEOPIXEL, MATRIX_PIN>(leds, MATRIX_WIDTH * MATRIX_HEIGHT);
  setMatrixLayout(MATRIX_LAYOUT);
  matrix->setRotation(ROTATE_SCREEN ? 90 : 0);
  GAMMA = 1.9;
  if (COLOR_CORRECTION)
  {
    FastLED.setCorrection(COLOR_CORRECTION);
  }
  if (COLOR_TEMPERATURE)
  {
    FastLED.setTemperature(COLOR_TEMPERATURE);
  }
  gif.setMatrix(matrix);
  ui->setAppAnimation(SLIDE_DOWN);

  ui->setTargetFPS(MATRIX_FPS);
  ui->setTimePerApp(TIME_PER_APP);
  ui->setTimePerTransition(TIME_PER_TRANSITION);
  ui->setOverlays(overlays, 3);
  ui->setBackgroundEffect(BACKGROUND_EFFECT);
  setAutoTransition(AUTO_TRANSITION);
  ui->init();
}

void ResetCustomApps()
{
  if (customApps.empty())
  {
    return;
  }

  for (auto it = customApps.begin(); it != customApps.end(); ++it)
  {
    CustomApp &app = it->second;
    if (app.name != currentCustomApp)
    {
      app.iconWasPushed = false;
      app.scrollposition = (app.icon ? 9 : 0) + app.textOffset;
      app.iconPosition = 0;
      app.scrollDelay = 0;
      app.currentRepeat = 0;
      app.icon.close();
      app.currentFrame = 0;
    }
  }
}

void checkLifetime(uint8_t pos)
{
  if (customApps.empty())
  {
    return;
  }

  if (pos >= Apps.size())
  {
    pos = 0;
  }

  String appName = Apps[pos].first;
  auto appIt = customApps.find(appName);

  if (appIt != customApps.end())
  {
    CustomApp &app = appIt->second;

    if (app.lifetime > 0 && (millis() - app.lastUpdate) / 1000 >= app.lifetime)
    {
      if (app.lifetimeMode == 0)
      {
        if (DEBUG_MODE)
          DEBUG_PRINTLN("Removing " + appName + " -> Lifetime over");
        removeCustomAppFromApps(appName, false);

        if (DEBUG_MODE)
          DEBUG_PRINTLN("Set new Apploop");
        ui->setApps(Apps);
      }
      else if (app.lifetimeMode == 1)
      {
        app.lifeTimeEnd = true;
      }
    }
  }
}

uint8_t received_packets = 0;
bool universe1_complete = false;
bool universe2_complete = false;

void DisplayManager_::tick()
{
  if (GAME_ACTIVE)
  {
    GameManager.tick();
    matrix->show();
    memcpy(ledsCopy, leds, sizeof(leds));
  }
  else if (AP_MODE)
  {
    HSVtext(2, 6, "AP MODE", true, 1);
  }
  else if (ARTNET_MODE)
  {
    // handled by the DMXFrame callback
  }
  else if (MOODLIGHT_MODE)
  {
    // handled by the moodlight function
  }
  else
  {
    ui->update();
    if (ui->getUiState()->appState == IN_TRANSITION && !appIsSwitching)
    {
      appIsSwitching = true;
    }
    else if (ui->getUiState()->appState == FIXED && appIsSwitching)
    {

      appIsSwitching = false;
      MQTTManager.setCurrentApp(CURRENT_APP);
      setAppTime(TIME_PER_APP);
      checkLifetime(ui->getnextAppNumber());
      ResetCustomApps();
    }
  }

  if (!AP_MODE)
  {
    uint16_t ArtnetStatus = artnet.read();
    if (ArtnetStatus > 0)
    {
      lastArtnetStatusTime = millis();
      ARTNET_MODE = true;
    }
    else if (millis() - lastArtnetStatusTime > 1000)
    {
      ARTNET_MODE = false;
    }
  }

  if (NEWYEAR)
    DisplayManager.checkNewYear();
}

bool newYearEventTriggered = false;

void DisplayManager_::checkNewYear()
{
  struct tm *timeInfo = timer_localtime();
  if (timeInfo->tm_mon == 0 && timeInfo->tm_mday == 1 && timeInfo->tm_hour == 0 && timeInfo->tm_min == 0 && timeInfo->tm_sec == 0)
  {
    if (!newYearEventTriggered)
    {
      int year = 1900 + timeInfo->tm_year;
      char message[300];
      sprintf(message, "{'stack':false,'text':'%d','duration':20,'effect':'Fireworks','rtttl':'Auld:d=4,o=6,b=125:a5,d.,8d,d,f#,e.,8d,e,8f#,8e,d.,8d,f#,a,2b.,b,a.,8f#,f#,d,e.,8d,e,8f#,8e,d.,8b5,b5,a5,2d,16p'}", year);
      DisplayManager.generateNotification(0, message);
      newYearEventTriggered = true;
    }
  }
  else
  {
    newYearEventTriggered = false;
  }
}

void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t *data)
{
  sendFrame = 1;
  // set brightness of the whole matrix
  if (universe == 10)
  {
    matrix->setBrightness(data[0]);
    matrix->show();
  }

  // Store which universe has got in
  if ((universe - startUniverse) < maxUniverses)
    universesReceived[universe - startUniverse] = 1;

  for (int i = 0; i < maxUniverses; i++)
  {
    if (universesReceived[i] == 0)
    {
      // Serial.println("Broke");
      sendFrame = 0;
      break;
    }
  }

  // read universe and put into the right part of the display buffer
  for (int i = 0; i < length / 3; i++)
  {
    int led = i + (universe - startUniverse) * (previousDataLength / 3);
    if (led < 256)
      matrix->drawPixel(led % matrix->width(), led / matrix->width(), matrix->Color(data[i * 3], data[i * 3 + 1], data[i * 3 + 2]));
  }
  previousDataLength = length;

  if (sendFrame)
  {
    matrix->show();
    // Reset universeReceived to 0
    memset(universesReceived, 0, maxUniverses);
  }
}

void DisplayManager_::startArtnet()
{
  artnet.begin();
  artnet.setArtDmxCallback(onDmxFrame);
}

void DisplayManager_::clear()
{
  matrix->clear();
}

void DisplayManager_::show()
{
  matrix->show();
}

void DisplayManager_::leftButton()
{
  if (!MenuManager.inMenu)
    ui->previousApp();
}

void DisplayManager_::rightButton()
{
  if (!MenuManager.inMenu)
    ui->nextApp();
}

void DisplayManager_::nextApp()
{
  if (!MenuManager.inMenu)
  {
    if (DEBUG_MODE)
      DEBUG_PRINTLN(F("Switching to next app"));
    ui->nextApp();
  }
}

void DisplayManager_::forceNextApp()
{
  ui->switchToApp(ui->getUiState()->currentApp);
  setAppTime(TIME_PER_APP);
  MQTTManager.setCurrentApp(getAppNameAtIndex(ui->getUiState()->currentApp));
}

void DisplayManager_::previousApp()
{
  if (!MenuManager.inMenu)
  {
    if (DEBUG_MODE)
      DEBUG_PRINTLN(F("Switching to previous app"));
    ui->previousApp();
  }
}

void DisplayManager_::selectButton()
{
  if (!MenuManager.inMenu)
  {
    DisplayManager.getInstance().dismissNotify();
  }
}

void DisplayManager_::selectButtonLong()
{
}

void DisplayManager_::dismissNotify()
{
  bool wakeup;
  if (!notifications.empty())
  {
    if (notifications.size() >= 2)
    {
      notifications[1].startime = millis();
    }
    wakeup = notifications[0].wakeup;
    notifications[0].icon.close();
    notifications.erase(notifications.begin());
    PeripheryManager.stopSound();
  }
  if (notifications.empty())
  {
    if (wakeup && MATRIX_OFF)
    {
      DisplayManager.setBrightness(0);
    }
  }
}

bool DisplayManager_::switchToApp(const char *json)
{
  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, json);
  if (error)
  {
    doc.clear();
    return false;
  }

  String name = doc["name"].as<String>();
  bool fast = doc["fast"] | false;
  doc.clear();
  int index = findAppIndexByName(name);
  if (index > -1)
  {
    if (fast)
    {
      ui->switchToApp(index);
      return true;
    }
    else
    {
      ui->transitionToApp(index);
      return true;
    }
  }
  else
  {
    return false;
  }
}

void DisplayManager_::drawProgressBar(int16_t x, int16_t y, int progress, uint32_t pColor, uint32_t pbColor)
{
  int available_length = 32 - x;
  int leds_for_progress = (progress * available_length) / 100;
  drawLine(x, y, x + available_length - 1, y, pbColor);
  if (leds_for_progress > 0)
    drawLine(x, y, x + leds_for_progress - 1, y, pColor);
}

void DisplayManager_::drawMenuIndicator(int cur, int total, uint32_t color)
{
  int menuItemWidth = 1;
  int totalWidth = total * menuItemWidth + (total - 1);
  int leftMargin = (MATRIX_WIDTH - totalWidth) / 2;
  int pixelSpacing = 1;
  for (int i = 0; i < total; i++)
  {
    int x = leftMargin + i * (menuItemWidth + pixelSpacing);
    if (i == cur)
    {
      drawLine(x, 7, x + menuItemWidth - 1, 7, color);
    }
    else
    {
      drawLine(x, 7, x + menuItemWidth - 1, 7, 0x666666);
    }
  }
}

void DisplayManager_::drawBarChart(int16_t x, int16_t y, const int data[], byte dataSize, bool withIcon, uint32_t color, uint32_t barBG)
{
  int availableWidth = withIcon ? (32 - 9) : 32;
  int gap = 1;
  int totalGapsWidth = (dataSize - 1) * gap;
  int barWidth = (availableWidth - totalGapsWidth) / dataSize;
  int startX = withIcon ? 9 : 0;

  for (int i = 0; i < dataSize; i++)
  {
    int x1 = x + startX + i * (barWidth + gap);
    int barHeight = data[i];
    int y1 = (barHeight > 0) ? (8 - barHeight) : 8;

    if (barBG > 0)
    {
      // Draw background bar
      drawFilledRect(x1, y, barWidth, 8, barBG);
    }

    if (barHeight > 0)
    {
      drawFilledRect(x1, y1 + y, barWidth, barHeight, color);
    }
  }
}

void DisplayManager_::drawLineChart(int16_t x, int16_t y, const int data[], byte dataSize, bool withIcon, uint32_t color)
{
  int availableWidth = withIcon ? (32 - 9) : 32;
  int startX = withIcon ? 9 : 0;
  float xStep = static_cast<float>(availableWidth) / static_cast<float>(dataSize - 1);
  int lastX = x + startX;
  int lastY = y + 8 - data[0];
  for (int i = 1; i < dataSize; i++)
  {
    int x1 = x + startX + static_cast<int>(xStep * i);
    int y1 = y + 8 - data[i];
    drawLine(lastX, lastY, x1, y1, color);
    lastX = x1;
    lastY = y1;
  }
}

std::pair<String, AppCallback> getNativeAppByName(const String &appName)
{
  if (appName == "Time")
  {
    return std::make_pair("Time", TimeApp);
  }
  else if (appName == "Date")
  {
    return std::make_pair("Date", DateApp);
  }
  else if (appName == "Temperature")
  {
    return std::make_pair("Temperature", TempApp);
  }
  else if (appName == "Humidity")
  {
    return std::make_pair("Humidity", HumApp);
  }
#ifdef ULANZI
  else if (appName == "Battery")
  {
    return std::make_pair("Battery", BatApp);
  }
#endif
  return std::make_pair("", nullptr);
}

void DisplayManager_::updateAppVector(const char *json)
{
  if (DEBUG_MODE)
    DEBUG_PRINTLN(F("New apps vector received"));
  if (DEBUG_MODE)
    DEBUG_PRINTLN(json);
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, json);
  if (error)
  {
    doc.clear();
    if (DEBUG_MODE)
      DEBUG_PRINTLN(F("Failed to parse json"));
    return;
  }

  JsonArray appArray;
  if (doc.is<JsonObject>())
  {
    JsonArray tempArray = doc.to<JsonArray>();
    tempArray.add(doc.as<JsonObject>());
    appArray = tempArray;
  }
  else if (doc.is<JsonArray>())
  {
    appArray = doc.as<JsonArray>();
  }

  for (JsonObject appObj : appArray)
  {
    String appName = appObj["name"].as<String>();
    bool show = appObj["show"].as<bool>();
    int position = appObj.containsKey("pos") ? appObj["pos"].as<int>() : Apps.size();

    auto appIt = std::find_if(Apps.begin(), Apps.end(), [&appName](const std::pair<String, AppCallback> &app)
                              { return app.first == appName; });

    std::pair<String, AppCallback> nativeApp = getNativeAppByName(appName);

    if (!show)
    {
      if (appIt != Apps.end())
      {
        Apps.erase(appIt);
      }
    }
    else
    {
      if (nativeApp.second != nullptr)
      {
        if (appIt != Apps.end())
        {
          Apps.erase(appIt);
        }
        position = position < 0 ? 0 : position >= Apps.size() ? Apps.size()
                                                              : position;
        Apps.insert(Apps.begin() + position, nativeApp);
      }
      else
      {
        if (appIt != Apps.end() && appObj.containsKey("pos"))
        {
          std::pair<String, AppCallback> app = *appIt;
          Apps.erase(appIt);
          position = position < 0 ? 0 : position >= Apps.size() ? Apps.size()
                                                                : position;
          Apps.insert(Apps.begin() + position, app);
        }
      }
    }
  }

  // Set the updated apps vector in the UI and save settings
  ui->setApps(Apps);
  saveSettings();
  sendAppLoop();
  setAutoTransition(AUTO_TRANSITION);
  doc.clear();
}

String DisplayManager_::getStats()
{
  StaticJsonDocument<1024> doc;
  char buffer[20];

#ifdef awtrix2_upgrade
  doc[F("type")] = 1;
#else
  doc[BatKey] = BATTERY_PERCENT;
  doc[BatRawKey] = BATTERY_RAW;
  doc[F("type")] = 0;
#endif
  doc[LuxKey] = static_cast<int>(CURRENT_LUX);
  doc[LDRRawKey] = LDR_RAW;
  doc[RamKey] = ESP.getFreeHeap() + ESP.getFreePsram();
  doc[BrightnessKey] = BRIGHTNESS;
  if (SENSOR_READING)
  {
    double formattedTemp = roundToDecimalPlaces(CURRENT_TEMP, TEMP_DECIMAL_PLACES);
    doc[TempKey] = formattedTemp;
    doc[HumKey] = static_cast<uint8_t>(CURRENT_HUM);
  }
  doc[UpTimeKey] = PeripheryManager.readUptime();
  doc[SignalStrengthKey] = WiFi.RSSI();
  doc[MessagesKey] = RECEIVED_MESSAGES;
  doc[VersionKey] = VERSION;
  doc[F("indicator1")] = ui->indicator1State;
  doc[F("indicator2")] = ui->indicator2State;
  doc[F("indicator3")] = ui->indicator3State;
  doc[F("app")] = CURRENT_APP;
  doc[F("uid")] = uniqueID;
  doc[F("matrix")] = !MATRIX_OFF;
  doc[IpAddrKey] = WiFi.localIP();
  doc[F("dev_json")] = DEV_JSON;
  String jsonString;
  serializeJson(doc, jsonString);
  return jsonString;
}

void DisplayManager_::setMatrixLayout(int layout)
{
  delete matrix; // Free memory from the current matrix object
  if (DEBUG_MODE)
    DEBUG_PRINTF("Set matrix layout to %i", layout);
  switch (layout)
  {
  case 0:
    matrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG);
    break;
  case 1:
    matrix = new FastLED_NeoMatrix(leds, 8, 8, 4, 1, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_PROGRESSIVE);
    break;
  case 2:
    matrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG);
    break;
  default:
    break;
  }

  delete ui;                        // Free memory from the current ui object
  ui = new MatrixDisplayUi(matrix); // Create a new ui object with the new matrix
}

String DisplayManager_::getAppsAsJson()
{
  DynamicJsonDocument doc(1024);
  JsonObject appsObject = doc.to<JsonObject>();
  for (size_t i = 0; i < Apps.size(); i++)
  {
    appsObject[Apps[i].first] = i;
  }
  String json;
  serializeJson(appsObject, json);
  return json;
}

void DisplayManager_::powerStateParse(const char *json)
{
  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, json);
  if (error)
  {
    setPower((strcmp(json, "true") == 0 || strcmp(json, "1") == 0) ? true : false);
    return;
  }

  if (doc.containsKey("power"))
  {
    bool power = doc["power"].as<bool>();
    setPower(power);
  }
}

void DisplayManager_::showSleepAnimation()
{
  setTextColor(0xFFFFFF);
  int steps[][2] = {{12, 8}, {13, 7}, {14, 6}, {15, 5}, {14, 4}, {13, 3}, {12, 2}, {13, 1}, {14, 0}, {15, -1}, {14, -2}, {13, -3}, {12, -4}, {13, -5}};
  int numSteps = sizeof(steps) / sizeof(steps[0]);
  for (int i = 0; i < numSteps; i++)
  {
    clear();
    printText(steps[i][0], steps[i][1], "Z", false, 1);
    show();
    delay(80);
  }
}

void DisplayManager_::setPower(bool state)
{
  if (state)
  {
    MATRIX_OFF = false;
    setBrightness(BRIGHTNESS);
  }
  else
  {
    MATRIX_OFF = true;
    showSleepAnimation();
    setBrightness(0);
  }
}

void DisplayManager_::setIndicator1Color(uint32_t color)
{
  ui->setIndicator1Color(color);
}

void DisplayManager_::setIndicator1State(bool state)
{
  ui->setIndicator1State(state);
}

void DisplayManager_::setIndicator2Color(uint32_t color)
{
  ui->setIndicator2Color(color);
}

void DisplayManager_::setIndicator2State(bool state)
{
  ui->setIndicator2State(state);
}

void DisplayManager_::setIndicator3Color(uint32_t color)
{
  ui->setIndicator3Color(color);
}

void DisplayManager_::setIndicator3State(bool state)
{
  ui->setIndicator3State(state);
}

bool DisplayManager_::indicatorParser(uint8_t indicator, const char *json)
{

  if (strcmp(json, "") == 0 || strcmp(json, "{}") == 0)
  {
    switch (indicator)
    {
    case 1:
      ui->setIndicator1State(false);
      ui->setIndicator1Fade(0);
      ui->setIndicator1Blink(0);
      MQTTManager.setIndicatorState(1, ui->indicator1State, ui->indicator1Color);
      break;
    case 2:
      ui->setIndicator2State(false);
      ui->setIndicator2Fade(0);
      ui->setIndicator2Blink(0);
      MQTTManager.setIndicatorState(2, ui->indicator2State, ui->indicator2Color);
      break;
    case 3:
      ui->setIndicator3State(false);
      ui->setIndicator3Fade(0);
      ui->setIndicator3Blink(0);
      MQTTManager.setIndicatorState(3, ui->indicator3State, ui->indicator3Color);
      break;
    default:
      break;
    }
    return true;
  }

  DynamicJsonDocument doc(128);
  DeserializationError error = deserializeJson(doc, json);
  if (error)
    return false;

  if (doc.containsKey("color"))
  {
    auto color = doc["color"];

    uint32_t col = getColorFromJsonVariant(color, TEXTCOLOR_888);

    if (col > 0)
    {
      switch (indicator)
      {
      case 1:
        ui->setIndicator1State(true);
        ui->setIndicator1Color(col);
        break;
      case 2:
        ui->setIndicator2State(true);
        ui->setIndicator2Color(col);
        break;
      case 3:
        ui->setIndicator3State(true);
        ui->setIndicator3Color(col);
        break;
      default:
        break;
      }
    }
    else
    {
      switch (indicator)
      {
      case 1:
        ui->setIndicator1State(false);
        break;
      case 2:
        ui->setIndicator2State(false);
        break;
      case 3:
        ui->setIndicator3State(false);
        break;
      default:
        break;
      }
    }
  }

  if (doc.containsKey("blink"))
  {
    switch (indicator)
    {
    case 1:
      ui->setIndicator1Blink(doc["blink"].as<int>());
      break;
    case 2:
      ui->setIndicator2Blink(doc["blink"].as<int>());
      break;
    case 3:
      ui->setIndicator3Blink(doc["blink"].as<int>());
      break;
    default:
      break;
    }
  }
  else
  {
    switch (indicator)
    {
    case 1:
      ui->setIndicator1Blink(0);
      break;
    case 2:
      ui->setIndicator2Blink(0);
      break;
    case 3:
      ui->setIndicator3Blink(0);
      break;
    default:
      break;
    }
  }

  if (doc.containsKey("fade"))
  {
    switch (indicator)
    {
    case 1:
      ui->setIndicator1Fade(doc["fade"].as<int>());
      break;
    case 2:
      ui->setIndicator2Fade(doc["fade"].as<int>());
      break;
    case 3:
      ui->setIndicator3Fade(doc["fade"].as<int>());
      break;
    default:
      break;
    }
  }
  else
  {
    switch (indicator)
    {
    case 1:
      ui->setIndicator1Fade(0);
      break;
    case 2:
      ui->setIndicator2Fade(0);
      break;
    case 3:
      ui->setIndicator3Fade(0);
      break;
    default:
      break;
    }
  }
  doc.clear();
  MQTTManager.setIndicatorState(indicator, ui->indicator1State, ui->indicator1Color);
  return true;
}

float logMap(float x, float in_min, float in_max, float out_min, float out_max, float mid_point_out)
{
  if (x < in_min)
    return out_min;
  if (x > in_max)
    return out_max;
  float scale = (mid_point_out - out_min) / log(in_max - in_min + 1);
  if (x <= (in_max + in_min) / 2.0)
  {
    return scale * log(x - in_min + 1) + out_min;
  }
  else
  {
    float upper_scale = (out_max - mid_point_out) / log(in_max - (in_max + in_min) / 2.0 + 1);
    return upper_scale * log(x - (in_max + in_min) / 2.0 + 1) + mid_point_out;
  }
}

void DisplayManager_::gammaCorrection()
{
  float gamma = logMap(actualBri, 2, 180, 0.535, 2.3, 1.9);
  memcpy(ledsCopy, leds, sizeof(leds));
  for (int i = 0; i < 256; i++)
  {
    leds[i] = applyGamma_video(leds[i], gamma);
  }

  if (MIRROR_DISPLAY)
  {
    for (int y = 0; y < MATRIX_HEIGHT; y++)
    {
      for (int x = 0; x < MATRIX_WIDTH / 2; x++)
      {
        int index1 = y * MATRIX_WIDTH + x;
        int index2 = y * MATRIX_WIDTH + (MATRIX_WIDTH - x - 1);
        std::swap(leds[index1], leds[index2]);
      }
    }
  }
}

void DisplayManager_::sendAppLoop()
{
  MQTTManager.publish("stats/loop", getAppsAsJson().c_str());
}

String CRGBtoHex(CRGB color)
{
  char buf[8];
  snprintf(buf, sizeof(buf), "#%02X%02X%02X", color.r, color.g, color.b);
  return String(buf);
}

String getOverlayName()
{
  switch (GLOBAL_OVERLAY)
  {
  case DRIZZLE:
    return "drizzle";
  case RAIN:
    return "rain";
  case SNOW:
    return "snow";
  case STORM:
    return "storm";
  case THUNDER:
    return "thunder";
  case FROST:
    return "frost";
  case NONE:
    return "clear";
  default:
    Serial.println(F("Invalid effect."));
    return "invalid"; // Oder einen leeren String oder einen Fehlerwert zurückgeben
  }
}

String DisplayManager_::getSettings()
{
  StaticJsonDocument<1024> doc;
  doc["MATP"] = !MATRIX_OFF;
  doc["ABRI"] = AUTO_BRIGHTNESS;
  doc["BRI"] = BRIGHTNESS;
  doc["ATRANS"] = AUTO_TRANSITION;
  doc["TCOL"] = TEXTCOLOR_888;
  doc["TEFF"] = TRANS_EFFECT;
  doc["TSPEED"] = TIME_PER_TRANSITION;
  doc["ATIME"] = TIME_PER_APP / 1000;
  doc["TMODE"] = TIME_MODE;
  doc["CHCOL"] = CALENDAR_HEADER_COLOR;
  doc["CTCOL"] = CALENDAR_TEXT_COLOR;
  doc["CBCOL"] = CALENDAR_BODY_COLOR;
  doc["TFORMAT"] = TIME_FORMAT;
  doc["DFORMAT"] = DATE_FORMAT;
  doc["SOM"] = START_ON_MONDAY;
  doc["CEL"] = IS_CELSIUS;
  doc["BLOCKN"] = BLOCK_NAVIGATION;
  doc["MAT"] = MATRIX_LAYOUT;
  doc["SOUND"] = SOUND_ACTIVE;
  doc["GAMMA"] = GAMMA;
  doc["UPPERCASE"] = UPPERCASE_LETTERS;
  doc["CCORRECTION"] = CRGBtoHex(COLOR_CORRECTION);
  doc["CTEMP"] = CRGBtoHex(COLOR_TEMPERATURE);
  doc["WD"] = SHOW_WEEKDAY;
  doc["TEFF"] = TRANS_EFFECT;
  doc["WDCA"] = WDC_ACTIVE;
  doc["WDCI"] = WDC_INACTIVE;
  doc["TIME_COL"] = TIME_COLOR;
  doc["DATE_COL"] = DATE_COLOR;
  doc["HUM_COL"] = HUM_COLOR;
  doc["TEMP_COL"] = TEMP_COLOR;
  doc["BAT_COL"] = BAT_COLOR;
  doc["SSPEED"] = SCROLL_SPEED;
  doc["TIM"] = SHOW_TIME;
  doc["DAT"] = SHOW_DATE;
  doc["HUM"] = SHOW_HUM;
  doc["TEMP"] = SHOW_TEMP;
  doc["BAT"] = SHOW_BAT;
  doc["VOL"] = SOUND_VOLUME;
  doc["OVERLAY"] = getOverlayName();
  String jsonString;
  return serializeJson(doc, jsonString), jsonString;
}

void DisplayManager_::setNewSettings(const char *json)
{
  if (DEBUG_MODE)
    DEBUG_PRINTLN(F("Got new settings:"));
  if (DEBUG_MODE)
    DEBUG_PRINTLN(json);
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, json);
  if (error)
  {
    if (DEBUG_MODE)
      DEBUG_PRINTLN(F("Error while parsing json"));
    if (DEBUG_MODE)
      DEBUG_PRINTLN(error.c_str());
    return;
  }
  if (doc.containsKey("ATIME"))
  {
    long atime = doc["ATIME"].as<int>();
    TIME_PER_APP = atime * 1000;
  }
  else
  {
    TIME_PER_APP = TIME_PER_APP;
  }

  if (doc.containsKey("OVERLAY"))
  {
    GLOBAL_OVERLAY = getOverlay(doc["OVERLAY"].as<String>());
    if (doc.size() == 1)
      return;
  }

  if (doc.containsKey("GAMEMODE"))
  {
    bool gamemode = doc["GAMEMODE"];
    GameManager.start(gamemode);
    return;
  }

  if (doc.containsKey("GAME"))
  {
    int game = doc["GAME"];
    GameManager.ChooseGame(game);
    return;
  }

  TIME_MODE = doc.containsKey("TMODE") ? doc["TMODE"].as<int>() : TIME_MODE;
  TRANS_EFFECT = doc.containsKey("TEFF") ? doc["TEFF"] : TRANS_EFFECT;
  TIME_PER_TRANSITION = doc.containsKey("TSPEED") ? doc["TSPEED"] : TIME_PER_TRANSITION;
  BRIGHTNESS = doc.containsKey("BRI") ? doc["BRI"] : BRIGHTNESS;
  SCROLL_SPEED = doc.containsKey("SSPEED") ? doc["SSPEED"] : SCROLL_SPEED;
  IS_CELSIUS = doc.containsKey("CEL") ? doc["CEL"] : IS_CELSIUS;
  START_ON_MONDAY = doc.containsKey("SOM") ? doc["SOM"].as<bool>() : START_ON_MONDAY;
  MATRIX_OFF = doc.containsKey("MATP") ? !doc["MATP"].as<bool>() : MATRIX_OFF;
  TIME_FORMAT = doc.containsKey("TFORMAT") ? doc["TFORMAT"].as<String>() : TIME_FORMAT;
  GAMMA = doc.containsKey("GAMMA") ? doc["GAMMA"].as<float>() : GAMMA;
  DATE_FORMAT = doc.containsKey("DFORMAT") ? doc["DFORMAT"].as<String>() : DATE_FORMAT;
  AUTO_BRIGHTNESS = doc.containsKey("ABRI") ? doc["ABRI"].as<bool>() : AUTO_BRIGHTNESS;
  AUTO_TRANSITION = doc.containsKey("ATRANS") ? doc["ATRANS"].as<bool>() : AUTO_TRANSITION;
  UPPERCASE_LETTERS = doc.containsKey("UPPERCASE") ? doc["UPPERCASE"].as<bool>() : UPPERCASE_LETTERS;
  SHOW_WEEKDAY = doc.containsKey("WD") ? doc["WD"].as<bool>() : SHOW_WEEKDAY;
  BLOCK_NAVIGATION = doc.containsKey("BLOCKN") ? doc["BLOCKN"].as<bool>() : BLOCK_NAVIGATION;
  SHOW_TIME = doc.containsKey("TIM") ? doc["TIM"].as<bool>() : SHOW_TIME;
  SHOW_DATE = doc.containsKey("DAT") ? doc["DAT"].as<bool>() : SHOW_DATE;
  SHOW_HUM = doc.containsKey("HUM") ? doc["HUM"].as<bool>() : SHOW_HUM;
  SHOW_TEMP = doc.containsKey("TEMP") ? doc["TEMP"].as<bool>() : SHOW_TEMP;
  SHOW_BAT = doc.containsKey("BAT") ? doc["BAT"].as<bool>() : SHOW_BAT;
  SOUND_ACTIVE = doc.containsKey("SOUND") ? doc["SOUND"].as<bool>() : SOUND_ACTIVE;

  if (doc.containsKey("VOL"))
  {
    SOUND_VOLUME = doc["VOL"];
    PeripheryManager.setVolume(SOUND_VOLUME);
  }

  if (doc.containsKey("CCORRECTION"))
  {
    auto colorValue = doc["CCORRECTION"];
    if (colorValue.is<String>())
    {
      String hexColor = colorValue.as<String>();
      uint32_t rgbColor = strtoul(hexColor.c_str(), NULL, 16);
      uint8_t r = (rgbColor >> 16) & 0xFF;
      uint8_t g = (rgbColor >> 8) & 0xFF;
      uint8_t b = rgbColor & 0xFF;
      COLOR_CORRECTION.setRGB(r, g, b);
    }
    else if (colorValue.is<JsonArray>() && colorValue.size() == 3)
    {
      uint8_t r = colorValue[0];
      uint8_t g = colorValue[1];
      uint8_t b = colorValue[2];
      COLOR_CORRECTION.setRGB(r, g, b);
    }

    if (COLOR_CORRECTION)
    {
      FastLED.setCorrection(COLOR_CORRECTION);
    }
  }
  if (doc.containsKey("CTEMP"))
  {
    auto colorValue = doc["CTEMP"];
    if (colorValue.is<String>())
    {
      String hexColor = colorValue.as<String>();
      uint32_t rgbColor = strtoul(hexColor.c_str(), NULL, 16);
      uint8_t r = (rgbColor >> 16) & 0xFF;
      uint8_t g = (rgbColor >> 8) & 0xFF;
      uint8_t b = rgbColor & 0xFF;
      COLOR_TEMPERATURE.setRGB(r, g, b);
    }
    else if (colorValue.is<JsonArray>() && colorValue.size() == 3)
    {
      uint8_t r = colorValue[0];
      uint8_t g = colorValue[1];
      uint8_t b = colorValue[2];
      COLOR_TEMPERATURE.setRGB(r, g, b);
    }

    if (COLOR_TEMPERATURE)
    {
      FastLED.setTemperature(COLOR_TEMPERATURE);
    }
  }
  if (doc.containsKey("WDCA"))
  {
    auto WDCA = doc["WDCA"];
    WDC_ACTIVE = getColorFromJsonVariant(WDCA, 0xFFFFFF);
  }
  if (doc.containsKey("CHCOL"))
  {
    auto CHCOL = doc["CHCOL"];
    CALENDAR_HEADER_COLOR = getColorFromJsonVariant(CHCOL, 0xFF0000);
  }
  if (doc.containsKey("CTCOL"))
  {
    auto CTCOL = doc["CTCOL"];
    CALENDAR_TEXT_COLOR = getColorFromJsonVariant(CTCOL, 0x000000);
  }
  if (doc.containsKey("CBCOL"))
  {
    auto CBCOL = doc["CBCOL"];
    CALENDAR_BODY_COLOR = getColorFromJsonVariant(CBCOL, 0xFFFFFF);
  }
  if (doc.containsKey("WDCI"))
  {
    auto WDCI = doc["WDCI"];
    WDC_INACTIVE = getColorFromJsonVariant(WDCI, 0x666666);
  }
  if (doc.containsKey("TCOL"))
  {
    auto TCOL = doc["TCOL"];
    uint32_t TempColor = getColorFromJsonVariant(TCOL, 0xFFFFFF);
    setCustomAppColors(TempColor);
    TEXTCOLOR_888 = TempColor;
  }

  if (doc.containsKey("TIME_COL"))
  {
    auto TIME_COL = doc["TIME_COL"];
    TIME_COLOR = getColorFromJsonVariant(TIME_COL, TEXTCOLOR_888);
  }
  if (doc.containsKey("DATE_COL"))
  {
    auto DATE_COL = doc["DATE_COL"];
    DATE_COLOR = getColorFromJsonVariant(DATE_COL, TEXTCOLOR_888);
  }
  if (doc.containsKey("TEMP_COL"))
  {
    auto TEMP_COL = doc["TEMP_COL"];
    TEMP_COLOR = getColorFromJsonVariant(TEMP_COL, TEXTCOLOR_888);
  }
  if (doc.containsKey("HUM_COL"))
  {
    auto HUM_COL = doc["HUM_COL"];
    HUM_COLOR = getColorFromJsonVariant(HUM_COL, TEXTCOLOR_888);
  }
  if (doc.containsKey("BAT_COL"))
  {
    auto BAT_COL = doc["BAT_COL"];
    BAT_COLOR = getColorFromJsonVariant(BAT_COL, TEXTCOLOR_888);
  }
  doc.clear();
  applyAllSettings();
  saveSettings();
  if (DEBUG_MODE)
    DEBUG_PRINTLN("Settings loaded");
}

void DisplayManager_::setCustomAppColors(uint32_t color)
{
  for (auto it = customApps.begin(); it != customApps.end(); ++it)
  {
    CustomApp &app = it->second;
    if (!app.hasCustomColor)
    {
      app.color = color;
    }
  }
}

String DisplayManager_::ledsAsJson()
{
  StaticJsonDocument<JSON_ARRAY_SIZE(MATRIX_WIDTH * MATRIX_HEIGHT)> jsonDoc;
  JsonArray jsonColors = jsonDoc.to<JsonArray>();
  for (int y = 0; y < MATRIX_HEIGHT; y++)
  {
    for (int x = 0; x < MATRIX_WIDTH; x++)
    {
      int index = matrix->XY(x, y);
      int color = (ledsCopy[index].r << 16) | (ledsCopy[index].g << 8) | ledsCopy[index].b;
      jsonColors.add(color);
    }
  }
  String jsonString;
  serializeJson(jsonColors, jsonString);
  return jsonString;
}

String DisplayManager_::getAppsWithIcon()
{
  DynamicJsonDocument jsonDocument(1024);
  JsonArray jsonArray = jsonDocument.to<JsonArray>();
  for (const auto &app : Apps)
  {
    JsonObject appObject = jsonArray.createNestedObject();
    appObject["name"] = app.first;

    CustomApp *customApp = getCustomAppByName(app.first);
    if (customApp != nullptr)
    {
      appObject["icon"] = customApp->iconName;
    }
  }
  String jsonString;
  serializeJson(jsonArray, jsonString);
  return jsonString;
}

CRGB DisplayManager_::getPixelColor(int16_t x, int16_t y)
{
  int index = matrix->XY(x, y);
  return leds[index];
}

void DisplayManager_::reorderApps(const String &jsonString)
{
  StaticJsonDocument<2048> jsonDocument;
  DeserializationError error = deserializeJson(jsonDocument, jsonString);
  if (error)
  {
    return;
  }

  JsonArray jsonArray = jsonDocument.as<JsonArray>();
  std::vector<std::pair<String, AppCallback>> reorderedApps;
  for (const String &appName : jsonArray)
  {
    for (const auto &app : Apps)
    {
      if (app.first == appName)
      {
        reorderedApps.push_back(app);
        break;
      }
    }
  }
  Apps = reorderedApps;
  ui->setApps(Apps);
  ui->forceResetState();
}

void DisplayManager_::processDrawInstructions(int16_t xOffset, int16_t yOffset, String &drawInstructions)
{
  DynamicJsonDocument doc(8192);
  DeserializationError error = deserializeJson(doc, drawInstructions);

  if (error)
  {
    Serial.println("Error parsing JSON draw instructions");
    return;
  }

  if (!doc.is<JsonArray>())
  {
    Serial.println("Invalid JSON draw instructions format");
    return;
  }

  JsonArray instructions = doc.as<JsonArray>();
  for (JsonObject instruction : instructions)
  {
    for (auto kvp : instruction)
    {
      String command = kvp.key().c_str();

      JsonArray params = kvp.value().as<JsonArray>();
      if (command == "dp")
      {
        int x = params[0].as<int>();
        int y = params[1].as<int>();
        auto color1 = params[2];
        uint32_t color = getColorFromJsonVariant(color1, TEXTCOLOR_888);
        matrix->drawPixel(x + xOffset, y + yOffset, color);
      }
      else if (command == "dl")
      {
        int x0 = params[0].as<int>();
        int y0 = params[1].as<int>();
        int x1 = params[2].as<int>();
        int y1 = params[3].as<int>();
        auto color2 = params[4];
        uint32_t color = getColorFromJsonVariant(color2, TEXTCOLOR_888);
        drawLine(x0 + xOffset, y0 + yOffset, x1 + xOffset, y1 + yOffset, color);
      }
      else if (command == "dr")
      {
        int x = params[0].as<int>();
        int y = params[1].as<int>();
        int w = params[2].as<int>();
        int h = params[3].as<int>();
        auto color3 = params[4];
        uint32_t color = getColorFromJsonVariant(color3, TEXTCOLOR_888);
        drawRect(x + xOffset, y + yOffset, w, h, color);
      }
      else if (command == "df")
      {
        int x = params[0].as<int>();
        int y = params[1].as<int>();
        int w = params[2].as<int>();
        int h = params[3].as<int>();
        auto color4 = params[4];
        uint32_t color = getColorFromJsonVariant(color4, TEXTCOLOR_888);
        drawFilledRect(x + xOffset, y + yOffset, w, h, color);
      }
      else if (command == "dc")
      {
        int x = params[0].as<int>();
        int y = params[1].as<int>();
        int r = params[2].as<int>();
        auto color5 = params[3];
        uint32_t color = getColorFromJsonVariant(color5, TEXTCOLOR_888);
        drawCircle(x + xOffset, y + yOffset, r, color);
      }
      else if (command == "dfc")
      {
        double x = params[0].as<double>();
        double y = params[1].as<double>();
        double r = params[2].as<double>();
        auto color6 = params[3];
        uint32_t color = getColorFromJsonVariant(color6, TEXTCOLOR_888);
        fillCircle(x + xOffset, y + yOffset, r, color);
      }
      else if (command == "dt")
      {
        int x = params[0].as<int>();
        int y = params[1].as<int>();
        String text = params[2].as<String>();
        auto color7 = params[3];
        uint32_t color = getColorFromJsonVariant(color7, TEXTCOLOR_888);
        setTextColor(color);
        setCursor(x + xOffset, y + yOffset + 5);
        matrixPrint(utf8ascii(text).c_str());
      }
      else if (command == "db")
      {
        int x = params[0].as<int>();
        int y = params[1].as<int>();
        int width = params[2].as<int>();
        int height = params[3].as<int>();
        std::vector<uint32_t> bitmap(width * height);
        JsonArray colorArray = params[4].as<JsonArray>();
        size_t i = 0;
        for (const auto &color : colorArray)
        {
          bitmap[i] = color.as<uint32_t>();
          i++;
        }
        size_t bitmapIndex = 0;
        for (int row = 0; row < height; ++row)
        {
          for (int col = 0; col < width; ++col)
          {
            matrix->drawPixel(x + col + xOffset, y + row + yOffset, bitmap[bitmapIndex++]);
          }
        }
      }
    }
  }
  doc.clear();
}

bool DisplayManager_::moodlight(const char *json)
{
  if (strcmp(json, "") == 0)
  {
    MOODLIGHT_MODE = false;
    return true;
  }

  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, json);
  if (error)
    return false;

  int brightness = doc["brightness"] | BRIGHTNESS;
  matrix->setBrightness(brightness);

  if (doc.containsKey("kelvin"))
  {
    int kelvin = doc["kelvin"];
    CRGB color;
    color = kelvinToRGB(kelvin);
    color.nscale8(brightness);
    for (int i = 0; i < 256; i++)
    {
      leds[i] = color;
    }
  }
  else if (doc.containsKey("color"))
  {
    auto c = doc["color"];
    uint32_t color888 = getColorFromJsonVariant(c, TEXTCOLOR_888);
    drawFilledRect(0, 0, 32, 8, color888);
  }
  else
  {
    doc.clear();
    return true;
  }

  MOODLIGHT_MODE = true;
  doc.clear();
  matrix->show();
  return true;
}

CRGB *DisplayManager_::getLeds()
{
  return leds;
}

String DisplayManager_::getEffectNames()
{
  StaticJsonDocument<1024> doc;
  JsonArray array = doc.to<JsonArray>();
  for (int i = 0; i < numOfEffects; i++)
  {
    array.add(effects[i].name);
  }
  String result;
  serializeJson(array, result);
  doc.clear();
  return result;
}

String DisplayManager_::getTransitionNames()
{
  char effectOptions[100];
  strcpy_P(effectOptions, HAeffectOptions);
  StaticJsonDocument<1024> doc;
  char *effect = strtok(effectOptions, ";");
  while (effect != NULL)
  {
    doc.add(effect);
    effect = strtok(NULL, ";");
  }
  String json;
  serializeJson(doc, json);
  doc.clear();
  return json;
}

// ### DRAWING FUNCTIONS ###

void DisplayManager_::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color)
{
  for (int16_t i = x; i < x + w; i++)
  {
    matrix->drawPixel(i, y, color);
    matrix->drawPixel(i, y + h - 1, color);
  }
  for (int16_t i = y; i < y + h; i++)
  {
    matrix->drawPixel(x, i, color);
    matrix->drawPixel(x + w - 1, i, color);
  }
}

void DisplayManager_::drawFastVLine(int16_t x, int16_t y, int16_t h, uint32_t color)
{
  drawLine(x, y, x, y + h - 1, color);
}

void DisplayManager_::drawFilledRect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color)
{
  for (int16_t i = x; i < x + w; i++)
  {
    drawFastVLine(i, y, h, color);
  }
}

void DisplayManager_::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint32_t color)
{
  int16_t dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int16_t dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int16_t err = dx + dy, e2;

  while (true)
  {
    matrix->drawPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1)
      break;
    e2 = 2 * err;
    if (e2 >= dy)
    {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx)
    {
      err += dx;
      y0 += sy;
    }
  }
}

void DisplayManager_::drawRGBBitmap(int16_t x, int16_t y, const uint32_t *bitmap, int16_t w, int16_t h)
{
  for (int16_t i = 0; i < w; i++)
  {
    for (int16_t j = 0; j < h; j++)
    {
      uint32_t pixelColor = bitmap[j * w + i];
      matrix->drawPixel(x + i, y + j, pixelColor);
    }
  }
}

void DisplayManager_::drawPixel(int16_t x0, int16_t y0, uint32_t color)
{
  matrix->drawPixel(x0, y0, color);
}

void DisplayManager_::drawCircle(int16_t x0, int16_t y0, int16_t r, uint32_t color)
{
  int16_t x = r;
  int16_t y = 0;
  int16_t p = 1 - r;

  if (r == 0)
  {
    matrix->drawPixel(x0, y0, color);
    return;
  }

  matrix->drawPixel(x0 + r, y0, color);
  matrix->drawPixel(x0 - r, y0, color);
  matrix->drawPixel(x0, y0 + r, color);
  matrix->drawPixel(x0, y0 - r, color);
  matrix->drawPixel(x0 + x, y0 - y, color);
  while (x > y)
  {
    y++;

    if (p <= 0)
      p = p + 2 * y + 1;
    else
    {
      x--;
      p = p + 2 * y - 2 * x + 1;
    }

    if (x < y)
      break;

    matrix->drawPixel(x0 + x, y0 - y, color);
    matrix->drawPixel(x0 - x, y0 - y, color);
    matrix->drawPixel(x0 + x, y0 + y, color);
    matrix->drawPixel(x0 - x, y0 + y, color);

    if (x != y)
    {
      matrix->drawPixel(x0 + y, y0 - x, color);
      matrix->drawPixel(x0 - y, y0 - x, color);
      matrix->drawPixel(x0 + y, y0 + x, color);
      matrix->drawPixel(x0 - y, y0 + x, color);
    }
  }
}

void DisplayManager_::fillCircle(int16_t x0, int16_t y0, int16_t r, uint32_t color)
{
  matrix->drawPixel(x0, y0, color);
  drawLine(x0 - r, y0, x0 + r, y0, color);
  int16_t x = r;
  int16_t y = 0;
  int16_t p = 1 - r;
  while (x > y)
  {
    y++;

    if (p <= 0)
      p = p + 2 * y + 1;
    else
    {
      x--;
      p = p + 2 * y - 2 * x + 1;
    }

    if (x < y)
      break;

    drawLine(x0 - x, y0 - y, x0 + x, y0 - y, color);
    drawLine(x0 - x, y0 + y, x0 + x, y0 + y, color);

    if (x != y)
    {
      drawLine(x0 - y, y0 - x, x0 + y, y0 - x, color);
      drawLine(x0 - y, y0 + x, x0 + y, y0 + x, color);
    }
  }
}

void DisplayManager_::matrixPrint(char c)
{
  if (c == '\n')
  {
    cursor_y += AwtrixFont.yAdvance;
    cursor_x = 0;
    return;
  }
  else if (c == '\r')
  {
    // Handle carriage return, if needed
    return;
  }

  c -= (uint8_t)pgm_read_byte(&AwtrixFont.first);
  GFXglyph *glyph = &AwtrixFont.glyph[c];
  uint8_t *bitmap = AwtrixFont.bitmap;
  uint16_t bo = glyph->bitmapOffset;
  uint8_t w = glyph->width,
          h = glyph->height;
  int8_t xo = glyph->xOffset,
         yo = glyph->yOffset;

  uint8_t xx, yy, bits = 0, bit = 0;
  for (yy = 0; yy < h; yy++)
  {
    for (xx = 0; xx < w; xx++)
    {
      if (!(bit++ & 7))
      {
        bits = pgm_read_byte(&bitmap[bo++]);
      }
      if (bits & 0x80)
      {
        matrix->drawPixel(cursor_x + xo + xx, cursor_y + yo + yy, textColor);
      }
      bits <<= 1;
    }
  }

  cursor_x += glyph->xAdvance;
}

void DisplayManager_::matrixPrint(const char *str)
{
  while (*str)
  {
    char c = *str++;
    if (c == '\n')
    {
      cursor_y += AwtrixFont.yAdvance;
    }
    else if (c >= AwtrixFont.first && c <= AwtrixFont.last)
    {
      GFXglyph *glyph = &AwtrixFont.glyph[c - AwtrixFont.first];
      matrixPrint(c);
    }
  }
}

void DisplayManager_::matrixPrint(double number, uint8_t digits)
{
  size_t n = 0;
  String output;

  if (isnan(number))
  {
    output = "nan";
  }
  else if (isinf(number))
  {
    output = "inf";
  }
  else if (number > 4294967040.0)
  {
    output = "ovf";
  }
  else if (number < -4294967040.0)
  {
    output = "ovf";
  }
  else
  {

    if (number < 0.0)
    {
      output += '-';
      number = -number;
    }
    double rounding = 0.5;
    for (uint8_t i = 0; i < digits; ++i)
    {
      rounding /= 10.0;
    }
    number += rounding;

    unsigned long int_part = (unsigned long)number;
    output += String(int_part);

    if (digits > 0)
    {
      output += '.';
    }

    double remainder = number - (double)int_part;
    while (digits-- > 0)
    {
      remainder *= 10.0;
      int toPrint = int(remainder);
      output += String(toPrint);
      remainder -= toPrint;
    }
  }

  matrixPrint(output);
}

void DisplayManager_::matrixPrint(String str)
{
  matrixPrint(str.c_str());
}

void DisplayManager_::matrixPrint(char *str)
{
  matrixPrint(static_cast<const char *>(str));
}

void DisplayManager_::matrixPrint(char str[], size_t length)
{
  size_t Tlength = strlen(str);
  char temp[Tlength + 1]; // +1 für Nullterminator
  strncpy(temp, str, Tlength);
  temp[Tlength] = '\0'; // Nullterminator hinzufügen
  matrixPrint(temp);
}

void DisplayManager_::setCursor(int16_t x, int16_t y)
{
  cursor_x = x;
  cursor_y = y;
}

void DisplayManager_::setTextColor(uint32_t color)
{
  textColor = color;
}