// Project: ESP32-Doorbell          V2.0 (jan-2021)
//
// Main ESP32-Doorbell module
//
#include <WiFi.h>
#include <SPIFFS.h>
#include "Main.h"
#include "WebServer.h"
#include "domoticz.h"
#include "setup.h"
#include <RCSwitch.h>

const char BUILD_MAIN[] = __DATE__ " " __TIME__;
//**************************************************************************************************************************************************
//**                                                           Setting Wifi credentials                                                           **
//**************************************************************************************************************************************************
uint webloglevel = 3;                         // loglevel to show in WebConsole. 0-5

char esp_board[20] = "none";                  // ESP type selection
char esp_name[20] = "ESP Chime";              // Wifi SSID waarop ESP32-chime zich moet aanmelden.
char esp_uname[10] = "admin";                 // Username voor weblogin.
char esp_pass[20] = "admin";                  // Bijbehorend wachtwoord voor SSID & Weblogin, moet min 8 characters zijn voor WifiManager

//**************************************************************************************************************************************************
//**                                                              IP settings device                                                              **
//**************************************************************************************************************************************************
char IPsetting[6] = "DHCP";                   // DHCP/Fixed
char IPaddr[16] = "192.168.0.0";              // IP address
char SubNetMask[16] = "255.255.255.0";        // subnet mask
char GatewayAddr[16] = "192.168.0.1";         // Gateway address
char DNSAddr[16] = "";                        // DNS address
//**************************************************************************************************************************************************
//**                                                          Setting Server credentials                                                         **
//**************************************************************************************************************************************************
char SendProtocol[5] = "none";                // Define protocol to use
char ServerIP[16] = "192.168.0.0";            // Domoticz Server IP adres.
char ServerPort[5] = "8080";                  // Domoticz Server poort adres.
char ServerUser[16] = "";                     // MQTT username
char ServerPass[16] = "";                     // MQTT password
char DomoticzIDX[5] = "999";                  // Domoticz IDX nummer welke geschakeld moet worden.
char SendOff[4] = "yes";                      // Send OFF command to Domoticz
char RFProtocol[3] = "1";                     // Send RF protocol
char RFPulse[5] = "500";                      // Send RF pulse
char RFcode[33] = "";                         // Sedn RF code max 32 bits
char MQTTsubscriber[20] = "ESP32Chime/Input"; // MQTT MQTTsubscriber name
char MQTTtopicin[20] = "domoticz/in";         // MQTT Topic name

int RCSWITCH_GPIO_Wroom = 12;                 // Set the rcswitch GPIO pin, ESP WROOM
int PHOTOMOS_GPIO_Wroom = 13;                 // Set the photomos GPIO pin, ESP WROOM
int RCSWITCH_GPIO_M5_pico = 21;               // Set the rcswitch GPIO pin, ESP M5stamp-pico
int PHOTOMOS_GPIO_M5_pico = 22;               // Set the photomos GPIO pin, ESP M5stamp-pico

//**************************************************************************************************************************************************
//**                                                                END SETTINGS END                                                              **
//**************************************************************************************************************************************************

// Define private used funcs
void start_ssdp_service();

AsyncWebServer webserver(80);

DNSServer dns;
ssdpAWS mySSDP(&webserver);

// Define RCSwitch
RCSwitch mySwitch = RCSwitch();

unsigned long MQTT_lasttime;  // MQTT check lasttime
bool WifiOK = false;          // WiFi status
bool mqtt_initdone = false;   // MQTT status
bool reboot = false;          // Pending reboot status
long rebootdelay = 0;         // used to calculate the delay

void setup() {
    //EEPROM.begin(200);
    Serial.begin(115200);
    Serial.setDebugOutput(true);

    // Initialize SPIFFS
    if (!SPIFFS.begin(true))
        ESP_LOGE(TAG, "An Error has occurred while mounting SPIFFS");

    // restore previous ESP saved settings
    Restore_ESPConfig_from_SPIFFS();

    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    AsyncWiFiManager wifiManager(&webserver, &dns);
    //reset saved settings
    //  wifiManager.resetSettings();
    // Previous line doesn't always work so this is another option to erase the EEPROM and all saved settings
    //  pio run --target erase

    // Set hardcoded IP Settings when Fixed IP is defined
    if (strcmp(IPsetting, "Fixed") == 0) {
        AddLogMessageI(F("==>Set Static IP\n"));
        IPAddress ip;
        IPAddress nm;
        IPAddress gw;
        IPAddress dn;
        ip.fromString(IPaddr);
        nm.fromString(SubNetMask);
        gw.fromString(GatewayAddr);
        if (!(strcmp(DNSAddr, "") == 0)) {
            dn.fromString(DNSAddr);
        }
        else {
            AddLogMessageI(F("DNS server not provided. Using Gateway IP as DNS.\n"));
            dn = gw;
        }
        wifiManager.setSTAStaticIPConfig(ip, gw, nm, dn);
    }
    // Try connecting to previous saved WiFI settings or else start as AP
    wifiManager.autoConnect(esp_name, esp_pass);
    //if you get here you have connected to the WiFi
    WifiOK = true;

    // Get Network time
    const char *NTPpool = "nl.pool.ntp.org";
    const char *defaultTimezone = "CET-1CEST,M3.5.0/2,M10.5.0/3";
    configTzTime(defaultTimezone, NTPpool);  //sets TZ and starts NTP sync
    // wait max 5 secs till time is synced
    AddLogMessageI("Wifi connected " + WiFi.SSID() + "  IP:" + WiFi.localIP().toString() + "  GW:" + WiFi.gatewayIP().toString() + "  NM:" + WiFi.subnetMask().toString() + "  DNS:" + WiFi.dnsIP().toString() + "  RSSI:" + String(WiFi.RSSI()) + "\n");
    struct tm timeinfo;
    for (uint i = 0; i < 10; i++) {
        if (getLocalTime(&timeinfo, 500)) {
            AddLogMessageI(F("Time synced.\n"));
            break;
        }
        if (i == 9)
            AddLogMessageW(F("Time not synced yet.. Continuing startup.\n"));
    }

    // Set outputs
    if (strcmp(esp_board, "ESP_Wroom") == 0) {
       mySwitch.enableTransmit(RCSWITCH_GPIO_Wroom);
       pinMode(PHOTOMOS_GPIO_Wroom, OUTPUT);
    } else if (strcmp(esp_board, "M5stamp_pico") == 1) {
       mySwitch.enableTransmit(RCSWITCH_GPIO_M5_pico);
       pinMode(PHOTOMOS_GPIO_M5_pico, OUTPUT);
    } else {
       mySwitch.enableTransmit(21);
       pinMode(22, OUTPUT);
    }

    //mySwitch.enableTransmit(RCSWITCH_GPIO_NUM);
    int iRFProtocol = atoi(RFProtocol);
    int iRFPulse = atoi(RFPulse); 
    mySwitch.setProtocol(iRFProtocol);
    mySwitch.setPulseLength(iRFPulse);
           
    // start mqtt
    if (!strcmp(SendProtocol, "mqtt")) {
        Mqtt_begin();
        mqtt_initdone = true;
    }
    MQTT_lasttime = millis();

    // Init WebServer
    WebServerInit(&webserver);

    // Make ESP-CHIME "known" in the network under it's ESP_NAME
    start_ssdp_service();
    AddLogMessageI(F("ESP Chime initialized\n"));
}

void loop() {
    // Don't do anything when shutting down and wait 1 second before rebooting
    if (reboot) {
        if (rebootdelay + 1000 > millis()) {
            Serial.flush();
            SPIFFS.end();
            delay(500);
            ESP.restart();
            delay(2000);
        }
        return;
    }

    if (!WiFi.isConnected()) {
        // Add a WiFI log record when WiFI goes down
        if (WifiOK) {
            WifiOK = false;
            AddLogMessageE(F("WiFi connection lost!\n"));
        }
    } else {
        // Add a WiFI log record when WiFI is restored
        if (!WifiOK) {
            WifiOK = true;
            AddLogMessageW(F("WiFi connection restored\n"));
        }
        // Process MQTT when selected
        if (mqtt_initdone && (millis() > MQTT_lasttime + 500)) {
            Mqtt_Loop();
            MQTT_lasttime = millis();
        }
        // Send any logmessages to the browser
        SendNextLogMessage();
    }

    // short pause
    delay(10);
}

void start_ssdp_service() {
    //initialize mDNS service
    //Define SSDP and model name
    const char *SSDP_Name = esp_name;
    const char *modelName = esp_board;
    const char *nVersion = BUILD_MAIN;
    const char *SerialNumber = "";
    const char *Manufacturer = "ESP32Chime";
    const char *ManufacturerURL = "https://github.com/Hoeby";
    mySSDP.begin(SSDP_Name, SerialNumber, modelName, nVersion, Manufacturer, ManufacturerURL);
    AddLogMessageI(F("SSDP started\n"));
}

void RFsend(const char *State){
    delay(10);
    //mySwitch.send(State);
    mySwitch.send("00110000101111001101010110010011");
    AddLogMessageI("RF Code send: (Protocol: " + String(RFProtocol) + ", Pulse: " + String(RFPulse) + ", Code: " + String(State) + ")\n");
    delay(1000);
}