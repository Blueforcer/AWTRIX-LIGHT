#ifndef esp_fs_webserver_H
#define esp_fs_webserver_H

#include <Arduino.h>
#include <memory>
#include <typeinfo>
#include <base64.h>
// #include <FS.h>
#include <LittleFS.h>

#define INCLUDE_EDIT_HTM
#ifdef INCLUDE_EDIT_HTM
#include "edit_htm.h"
#endif

#define INCLUDE_SETUP_HTM
#ifdef INCLUDE_SETUP_HTM
#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>
#include "setup_htm.h"
#endif

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h> // from Arduino core, OTA update via webbrowser
using WebServerClass = ESP8266WebServer;
using UpdateServerClass = ESP8266HTTPUpdateServer;
#elif defined(ESP32)
#include <esp_wifi.h>
#include <WebServer.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <HTTPUpdateServer.h> // from Arduino core, OTA update via webbrowser
using WebServerClass = WebServer;
using UpdateServerClass = HTTPUpdateServer;
#endif
#include <DNSServer.h>

#ifndef DEBUG_ESP_PORT
#define DBG_OUTPUT_PORT Serial
#define DEBUG_MODE_WS false
#else
#define DBG_OUTPUT_PORT DEBUG_ESP_PORT
#endif

#if DEBUG_MODE_WS
#define DebugPrint(x) DBG_OUTPUT_PORT.print(x)
#define DebugPrintln(x) DBG_OUTPUT_PORT.println(x)
#define DebugPrintf(fmt, ...) DBG_OUTPUT_PORT.printf(fmt, ##__VA_ARGS__)
#define DebugPrintf_P(fmt, ...) DBG_OUTPUT_PORT.printf_P(fmt, ##__VA_ARGS__)
#else
#define DebugPrint(x)
#define DebugPrintln(x)
#define DebugPrintf(x, ...)
#define DebugPrintf_P(x, ...)
#endif

enum
{
    MSG_OK,
    CUSTOM,
    NOT_FOUND,
    BAD_REQUEST,
    ERROR
};
#define TEXT_PLAIN "text/plain"
#define FS_INIT_ERROR "FS INIT ERROR"
#define FILE_NOT_FOUND "FileNotFound"

class FSWebServer
{

    // using CallbackF = std::function<void(void)>;

public:
    WebServerClass *webserver;

    FSWebServer(fs::FS &fs, WebServerClass &server);

    bool begin( int port = 80,const char *path = nullptr);

    void run();
    void onNotFound(WebServerClass::THandlerFunction fn);

    void addHandler(const Uri &uri, HTTPMethod method, WebServerClass::THandlerFunction fn);
    void addHandler(const Uri &uri, WebServerClass::THandlerFunction handler);

    void setCaptiveWebage(const char *url);

    IPAddress setAPmode(const char *ssid, const char *psk);

    IPAddress startWiFi(uint32_t timeout, const char *apSSID, const char *apPsw);

    WebServerClass *getRequest();

    void setAuth(const String &user, const String &pass)
    {
        authUser = user;
        authPass = pass;
    }

#ifdef INCLUDE_SETUP_HTM

#define MIN_F -3.4028235E+38
#define MAX_F 3.4028235E+38

    inline bool clearOptions()
    {
        File file = m_filesystem->open("/DoNotTouch.json", "r");
        if (file)
        {
            file.close();
            m_filesystem->remove("/DoNotTouch.json");
            return true;
        }
        return false;
    }

    inline void addOptionBox(const char *boxTitle)
    {
        addOption("param-box", boxTitle, false);
    }

    inline void addHTML(const char *html, const char *id)
    {
        String elementId = "raw-html-";
        elementId += id;
        String trimmed = html;
        removeWhiteSpaces(trimmed);
        addOption(elementId.c_str(), trimmed.c_str(), false);
    }

    inline void addCSS(const char *css)
    {
        String trimmed = css;
        removeWhiteSpaces(trimmed);
        addOption("raw-css", trimmed.c_str(), false);
    }

    inline void addJavascript(const char *script)
    {
        // String trimmed = script;
        //  removeWhiteSpaces(trimmed);
        addOption("raw-javascript", script, true, MIN_F, MAX_F, 1.0, true);
    }

    void addDropdownList(const char *label, const char **array, size_t size);

    // Only for backward-compatibility
    template <typename T>
    inline void addOption(fs::FS &fs, const char *label, T val, bool hidden = false)
    {
        addOption(label, val, hidden);
    }

    // Add custom option to config webpage (float values)
    template <typename T>
    inline void addOption(const char *label, T val, double d_min, double d_max, double step)
    {
        addOption(label, val, false, d_min, d_max, step);
    }

    // Add custom option to config webpage (type of parameter will be deduced from variable itself)
    template <typename T>
    inline void addOption(const char *label, T val, bool hidden = false,
                          double d_min = MIN_F, double d_max = MAX_F, double step = 1.0, bool replace = false)
    {
        File file = m_filesystem->open("/DoNotTouch.json", "r");
        int sz = file.size() * 1.33;
        int docSize = max(sz, 2048);
        DynamicJsonDocument doc((size_t)docSize);
        if (file)
        {
            // If file is present, load actual configuration
            DeserializationError error = deserializeJson(doc, file);
            if (error)
            {
                DebugPrintln(F("Failed to deserialize file, may be corrupted"));
                DebugPrintln(error.c_str());
                file.close();
                return;
            }
            file.close();
        }
        else
        {
            DebugPrintln(F("File not found, will be created new configuration file"));
        }

        numOptions++;

        String key = label;
        if (hidden)
            key += "-hidden";

        // Unique key name
        if (key.equals("param-box"))
        {
            key = key + "-" + val;
        }
        if (key.equals("raw-javascript"))
        {
            key += numOptions;
        }

        // If key is present in json and replace is false, we don't need to create it.
        if (doc.containsKey(key.c_str()) && !replace)
            return;

        // if min, max, step != from default, treat this as object in order to set other properties
        if (d_min != MIN_F || d_max != MAX_F || step != 1.0)
        {
            JsonObject obj = doc.createNestedObject(key);
            obj["value"] = static_cast<T>(val);
            obj["min"] = d_min;
            obj["max"] = d_max;
            obj["step"] = step;
        }
        else
        {
            doc[key] = static_cast<T>(val);
        }

        file = m_filesystem->open("/DoNotTouch.json", "w");
        if (serializeJsonPretty(doc, file) == 0)
        {
            DebugPrintln(F("Failed to write to file"));
        }
        file.close();
    }

    // Get current value for a specific custom option (true on success)
    template <typename T>
    bool getOptionValue(const char *label, T &var)
    {
        File file = m_filesystem->open("/DoNotTouch.json", "r");
        DynamicJsonDocument doc(file.size() * 1.33);
        if (file)
        {
            DeserializationError error = deserializeJson(doc, file);
            if (error)
            {
                DebugPrintln(F("Failed to deserialize file, may be corrupted"));
                DebugPrintln(error.c_str());
                file.close();
                return false;
            }
            file.close();
        }
        else
            return false;

        if (doc[label]["value"])
            var = doc[label]["value"].as<T>();
        else if (doc[label]["selected"])
            var = doc[label]["selected"].as<T>();
        else
            var = doc[label].as<T>();
        return true;
    }

    template <typename T>
    bool saveOptionValue(const char *label, T val)
    {
        // Öffne die Datei im Lesemodus, um den Inhalt des Dokuments beizubehalten
        File file = m_filesystem->open("/DoNotTouch.json", "r");
        DynamicJsonDocument doc(file.size() * 1.33);

        if (file)
        {
            // Deserialisiere das JSON-Dokument aus der Datei
            DeserializationError error = deserializeJson(doc, file);
            if (error)
            {
                DebugPrintln(F("Failed to deserialize file, may be corrupted"));
                DebugPrintln(error.c_str());
                file.close();
                return false;
            }
            file.close();
        }

        // Füge den Schlüssel mit dem neuen Wert hinzu, falls er nicht vorhanden ist
        if (!doc.containsKey(label))
        {
            doc[label] = val;
        }
        else
        {
            // Ändere den Wert mit dem Schlüssel "label"
            JsonVariant option = doc[label];
            option.set(val);
        }

        // Öffne die Datei im Schreibmodus und speichere das geänderte Dokument
        file = m_filesystem->open("/DoNotTouch.json", "w");
        if (!file)
            return false;
        serializeJsonPretty(doc, file);
        file.close();

        return true;
    }

#endif

private:
    int failedAttempts = 0;
    unsigned long previousMillis = 0;
    unsigned long interval = 10000;
    char m_basePath[16];
    UpdateServerClass m_httpUpdater;
    String authUser;
    String authPass;
    DNSServer m_dnsServer;
    fs::FS *m_filesystem;
    File m_uploadFile;
    bool m_fsOK = false;
    bool m_apmode = false;
    char *m_apWebpage = (char *)"/setup";
    uint32_t m_timeout = 10000;

    // Default handler for all URIs not defined above, use it to read files from filesystem
    bool checkDir(char *dirname, uint8_t levels);
    void doWifiConnection();
    void doRestart();
    void replyOK();
    void getIpAddress();
    void handleRequest();
#ifdef INCLUDE_SETUP_HTM
    void removeWhiteSpaces(String &str);
    void handleSetup();
    uint8_t numOptions = 0;
#endif
    void handleIndex();
    bool handleFileRead(const String &path);
    void handleFileUpload();
    void replyToCLient(int msg_type, const char *msg);
    void checkForUnsupportedPath(String &filename, String &error);
    void setCrossOrigin();
    void handleScanNetworks();
    const char *getContentType(const char *filename);
    bool captivePortal();
    WebServerClass::THandlerFunction authMiddleware(WebServerClass::THandlerFunction fn);
    // edit page, in usefull in some situation, but if you need to provide only a web interface, you can disable
#ifdef INCLUDE_EDIT_HTM
    void handleGetEdit();
    void handleFileCreate();
    void handleFileDelete();
    void handleStatus();
    void handleFileList();
#endif
};

#endif
