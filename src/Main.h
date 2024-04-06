#include <Arduino.h>

#include <ssdpAWS.h>              // SSDP for ESPAsyncWebServer
#include <DNSServer.h>            // Local DNS Server used for redirecting all requests to the configuration portal
#include <ESPAsyncWebServer.h>    // Local WebServer used to server the configuration portal
#include <ESPAsyncWiFiManager.h>  // https://github.com/alanswx/ESPAsyncWiFiManager

//**************************************************************************************************************************************************
//**                                                              Setting Chime type                                                             **
//**************************************************************************************************************************************************
//-------------------------------------------------------
// Global variables defined in Main.cpp
//-------------------------------------------------------
extern uint webloglevel;
extern char esp_name[20];
extern char esp_uname[10];
extern char esp_pass[20];
extern char IPsetting[6];
extern char IPaddr[16];
extern char SubNetMask[16];
extern char GatewayAddr[16];
extern char DNSAddr[16];
extern char SendProtocol[5];
extern char ServerIP[16];
extern char ServerPort[5];
extern char ServerUser[16];
extern char ServerPass[16];
extern char DomoticzIDX[5];
extern char SendOff[4];
extern char RFProtocol[3];
extern char RFPulse[5];
extern char RFcode[33];
extern char MQTTsubscriber[20];
extern char MQTTtopicin[20];
extern const char BUILD_MAIN[];
extern bool reboot;
extern long rebootdelay;
extern bool mqtt_initdone;

extern char RCSWITCH_GPIO[3];
extern char PHOTOMOS_GPIO[3];

void RFsend(const char *State);

//-------------------------------------------------------
// Global Functions
