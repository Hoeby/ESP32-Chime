// Project: ESP32-Doorbell
//
// Setup module
//
#include <ESPAsyncWebServer.h>  // Local WebServer used to server the configuration portal
#include <SPIFFS.h>
#include "ArduinoJson.h"
#include "setup.h"
#include "WebServer.h"
#include "Main.h"
#include "domoticz.h"
#ifndef VERSION
#define VERSION 1.0.0
#endif

// Define private used funcs
bool GetJsonField(String key, DynamicJsonDocument doc, char *variable);
bool GetJsonField(String key, DynamicJsonDocument doc, uint *variable);

extern AsyncWebServer webserver;

// Get Variable info from JSON input CHAR STRINGS
bool GetJsonField(String key, DynamicJsonDocument doc, char *variable) {
    const char *value = doc[key];
    String msg = F("Key ");
    msg += key;
    if (value) {
        strcpy(variable, value);
        msg += F("=");
        msg += String(value);
        msg += F("\n");
        AddLogMessageD(msg);
        return true;
    }
    msg += F(" not in configfile, using default\n");
    AddLogMessageW(msg);
    return false;
}

// Get Variable info from JSON input unsigned INT Values
bool GetJsonField(String key, DynamicJsonDocument doc, uint *variable) {
    const char *value = doc[key];
    String msg = F("Key ");
    msg += key;
    if (value) {
        *variable = atoi(value);
        msg += F("=");
        msg += String(*variable);
        msg += F("\n");
        AddLogMessageD(msg);
        return true;
    }
    msg += F(" not in configfile, using default\n");
    AddLogMessageW(msg);
    return false;
}

// Restore ESP settings From SPIFFS
bool Restore_ESPConfig_from_SPIFFS() {
    // SPIFFS
    AddLogMessageI(F("Restore configuration from SPIFF\n"));
    File file = SPIFFS.open("/ESP_CHIME_CONFIG.json", "r");
    if (!file || file.isDirectory()) {
        AddLogMessageD(F("- empty file or failed to open file\n"));
        return false;
    }
    String fileContent;
    while (file.available()) {
        fileContent += String((char)file.read());
        if (fileContent.length() > 5000) {
            AddLogMessageE(F("- file too large, assume it is corrupt and use defaults\n"));
            fileContent = "";
            file.close();
            SPIFFS.remove("/ESP_CHIME_CONFIG.json");
            break;
        }
    }
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, fileContent);
    if (error) {
        AddLogMessageD(F("Config Parsing failed\n"));
        return false;
    }
    GetJsonField("webloglevel", doc, &webloglevel);
    GetJsonField("esp_name", doc, esp_name);
    GetJsonField("esp_uname", doc, esp_uname);
    GetJsonField("esp_pass", doc, esp_pass);
    GetJsonField("IPsetting", doc, IPsetting);
    GetJsonField("IPaddr", doc, IPaddr);
    GetJsonField("SubNetMask", doc, SubNetMask);
    GetJsonField("GatewayAddr", doc, GatewayAddr);
    GetJsonField("DNSAddr", doc, DNSAddr);
    GetJsonField("SendProtocol", doc, SendProtocol);
    GetJsonField("ServerIP", doc, ServerIP);
    GetJsonField("ServerPort", doc, ServerPort);
    GetJsonField("ServerUser", doc, ServerUser);
    GetJsonField("ServerPass", doc, ServerPass);
    GetJsonField("DomoticzIDX", doc, DomoticzIDX);
    GetJsonField("SendOff", doc, SendOff);
    GetJsonField("RCSWITCH_GPIO", doc, RCSWITCH_GPIO);
    GetJsonField("PHOTOMOS_GPIO", doc, PHOTOMOS_GPIO);
    GetJsonField("RFProtocol", doc, RFProtocol);
    GetJsonField("RFPulse", doc, RFPulse);
    GetJsonField("RFcode", doc, RFcode);
    GetJsonField("MQTTsubscriber", doc, MQTTsubscriber);
    GetJsonField("MQTTtopicin", doc, MQTTtopicin);
    return true;
}

// Save the information from SETUP.HTM to SPIFFS
void Save_NewESPConfig_to_SPIFFS(AsyncWebServerRequest *request) {
    static char json_response[1024];
    char *p = json_response;
    *p++ = '{';
    p += sprintf(p, "\"webloglevel\":\"%s\",", urlDecode(request->arg("webloglevel")).c_str());
    p += sprintf(p, "\"esp_board\":\"%s\",", urlDecode(request->arg("esp_board")).c_str());
    p += sprintf(p, "\"esp_name\":\"%s\",", urlDecode(request->arg("esp_name")).c_str());
    p += sprintf(p, "\"esp_uname\":\"%s\",", urlDecode(request->arg("esp_uname")).c_str());
    p += sprintf(p, "\"esp_pass\":\"%s\",", urlDecode(request->arg("esp_pass")).c_str());
    p += sprintf(p, "\"IPsetting\":\"%s\",", urlDecode(request->arg("IPsetting")).c_str());
    p += sprintf(p, "\"IPaddr\":\"%s\",", urlDecode(request->arg("IPaddr")).c_str());
    p += sprintf(p, "\"SubNetMask\":\"%s\",", urlDecode(request->arg("SubNetMask")).c_str());
    p += sprintf(p, "\"GatewayAddr\":\"%s\",", urlDecode(request->arg("GatewayAddr")).c_str());
    p += sprintf(p, "\"DNSAddr\":\"%s\",", urlDecode(request->arg("DNSAddr")).c_str());
    p += sprintf(p, "\"SendProtocol\":\"%s\",", urlDecode(request->arg("Send_Protocol")).c_str());
    p += sprintf(p, "\"ServerIP\":\"%s\",", urlDecode(request->arg("ServerIP")).c_str());
    p += sprintf(p, "\"ServerPort\":\"%s\",", urlDecode(request->arg("ServerPort")).c_str());
    p += sprintf(p, "\"ServerUser\":\"%s\",", urlDecode(request->arg("ServerUser")).c_str());
    p += sprintf(p, "\"ServerPass\":\"%s\",", urlDecode(request->arg("ServerPass")).c_str());
    p += sprintf(p, "\"DomoticzIDX\":\"%s\",", urlDecode(request->arg("DomoticzIDX")).c_str());
    p += sprintf(p, "\"SendOff\":\"%s\",", urlDecode(request->arg("SendOff")).c_str());
    p += sprintf(p, "\"RCSWITCH_GPIO\":\"%s\",", urlDecode(request->arg("RCSWITCH_GPIO")).c_str());
    p += sprintf(p, "\"PHOTOMOS_GPIO\":\"%s\",", urlDecode(request->arg("PHOTOMOS_GPIO")).c_str());
    p += sprintf(p, "\"RFProtocol\":\"%s\",", urlDecode(request->arg("RFProtocol")).c_str());
    p += sprintf(p, "\"RFPulse\":\"%s\",", urlDecode(request->arg("RFPulse")).c_str());
    p += sprintf(p, "\"RFcode\":\"%s\",", urlDecode(request->arg("RFcode")).c_str());
    p += sprintf(p, "\"MQTTsubscriber\":\"%s\",", urlDecode(request->arg("MQTTsubscriber")).c_str());
    p += sprintf(p, "\"MQTTtopicin\":\"%s\",", urlDecode(request->arg("MQTTtopicin")).c_str());
    p += sprintf(p, "\"dummy\":\"\"");
    *p++ = '}';
    *p++ = 0;
    File file = SPIFFS.open("/ESP_CHIME_CONFIG.json", "w");
    String msg = F("Saving ESPCHIME configuration to SPIFF, ");
    if (!file) {
        msg += F(" failed to open file for writing\n");
        AddLogMessageE(msg);
        return;
    }
    if (file.print(json_response)) {
        msg += F("- config saved\n");
        AddLogMessageI(msg);
    } else {
        msg += F("- config save failed!!!!");
        AddLogMessageE(msg);
    }
    file.close();
}

// Add possible template variables for the webpages
String TranslateTemplateVars(const String &var) {
    if (var == "VERSION") {
        char tmp[40];
        sprintf(tmp, "%s - %s", VERSION, BUILD_MAIN);
        return tmp;
    }
    if (var == "VERSION_MAJOR")
        return VERSION;
    if (var == "webloglevel")
        return String(webloglevel);
    if (var == "esp_name")
        return esp_name;
    if (var == "esp_uname")
        return esp_uname;
    if (var == "esp_pass")
        return esp_pass;
    if (var == "IPsetting")
        return IPsetting;
    if (var == "IPaddr")
        return IPaddr;
    if (var == "SubNetMask")
        return SubNetMask;
    if (var == "GatewayAddr")
        return GatewayAddr;
    if (var == "DNSAddr")
        return DNSAddr;
    if (var == "SendProtocol")
        return SendProtocol;
    if (var == "ServerIP")
        return ServerIP;
    if (var == "ServerPort")
        return ServerPort;
    if (var == "ServerUser")
        return ServerUser;
    if (var == "ServerPass")
        return ServerPass;
    if (var == "DomoticzIDX")
        return DomoticzIDX;
    if (var == "SendOff")
        return SendOff;
    if (var == "RCSWITCH_GPIO")
        return RCSWITCH_GPIO;
    if (var == "PHOTOMOS_GPIO")
        return PHOTOMOS_GPIO;        
    if (var == "RFProtocol")
        return RFProtocol;
    if (var == "RFPulse")
        return RFPulse;
    if (var == "RFcode")
        return RFcode;
    if (var == "MQTTsubscriber")
        return MQTTsubscriber;
    if (var == "MQTTtopicin")
        return MQTTtopicin;
    if (var == "ipaddr")
        return WiFi.localIP().toString();
    if (var == "ipgate")
        return WiFi.gatewayIP().toString();
    if (var == "ipnetm")
        return WiFi.subnetMask().toString();
    if (var == "wifi_ssid")
        return String(WiFi.SSID());
    if (var == "wifi_rssi")
        return String(WiFi.RSSI());
    if (var == "SPIFFS_tot")
        return String(SPIFFS.totalBytes() / 1000);
    if (var == "SPIFFS_used")
        return String(SPIFFS.usedBytes() / 1000);
    if (var == "SPIFFS_free")
        return String((SPIFFS.totalBytes() - SPIFFS.usedBytes()) / 1000);
    return String();
}
