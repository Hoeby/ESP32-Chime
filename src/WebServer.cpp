// Project: ESP32-Doorbell
//
// WebServer module
// Partly based on: // https://gist.github.com/me-no-dev/d34fba51a8f059ac559bf62002e61aa3
//
#include <WiFi.h>
#include <ESPAsyncWebServer.h>  // Local WebServer used to server the configuration portal
#include <Update.h>
#include <SPIFFS.h>
#include "ArduinoJson.h"

#include "Main.h"
#include "WebServer.h"
#include "setup.h"
#include "domoticz.h"

// Define private used funcs
String makePage(String title, String contents);

AsyncWebSocket ws("/ws");
uint LogId = 999;
String LogMessage[50];                 // amount of messages to log
uint LogMessageIndexI = 0;
uint LogMessageIndexO = 0;
bool LogMessageSuccess = false;
unsigned long LogMessage_Send_time;    // time a logmessage was send.
uint LogMessage_Send_tries = 0;        // number of tries.
bool LogMessage_NewLine = true;        // remember wether the last line had a \n

String tfile;                          // target SPIFFS file
File hfile;                            // target SPIFFS file handle
bool EspConfig = false;                // target SPIFFS file config?
bool ChimeConfig = false;              // target SPIFFS file config?
bool tbin = false;                     // target bin file?

char CustomRfProtocol[3] = "";         // Send RF protocol
char CustomRfPulse[5] = "";            // Send RF pulse
char CustomRfCode[33] = "";            // Send RF code max 32 bits

bool _webAuth(AsyncWebServerRequest *request) {
    if (!request->authenticate(esp_uname, esp_pass, NULL, false)) {
        request->requestAuthentication(NULL, false);  // force basic auth
        return false;
    }
    return true;
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        // close previous session first
        if (LogId != 999)
            client->close(LogId);

        LogId = client->id();
        LogMessageSuccess = true;
        // set Output to current last input message as we add 1 when confirmed
        LogMessageIndexO = LogMessageIndexI + 1;
        // find first none empty message record
        for (uint i = LogMessageIndexI; i < 50; i++) {
            if (LogMessage[LogMessageIndexO] != "")
                break;
            LogMessageIndexO++;
            if (LogMessageIndexO > 49)
                LogMessageIndexO = 0;
        }
        log_d("Websocket client connection received %i start sending messages: %i", LogId, LogMessageIndexO);
    } else if (type == WS_EVT_DISCONNECT) {
        log_d("Client disconnected");
        LogId = 999;
    } else if (type == WS_EVT_ERROR) {
        //error was received from the other end
        //Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t *)arg), (char *)data);
    } else if (type == WS_EVT_PONG) {
        //pong message was received (in response to a ping request maybe)
        //Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char *)data : "");
    } else if (type == WS_EVT_DATA) {
        //data packet
        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        if (info->final && info->index == 0 && info->len == len) {
            //the whole message is in a single frame and we got all of it's data
            //Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", info->len);
            if (info->opcode == WS_TEXT) {
                data[len] = 0;
                //Serial.printf("%s\n", (char *)data);
                if (strcmp((char *)data, "ok") == 0) {
                    // log message confirmed received so reset the array entry and set  indeex to next
                    //log_d("Ok msg:%i", LogMessageIndexO);
                    LogMessageSuccess = true;
                    LogMessageIndexO++;
                }
            }
        }
    }
}

// Define all specific webpages to service by the ESP
void WebServerInit(AsyncWebServer *webserver) {
    // WebSockets
    ws.onEvent(onWsEvent);
    webserver->addHandler(&ws);

    // Define all weboptions with TEMPLATE variables or specific functions
    webserver->on("/", HTTP_GET, ESPShowPagewithTemplate);
    webserver->on("/index.html", HTTP_GET, ESPShowPagewithTemplate);
    webserver->on("/info", HTTP_GET, ESPShowPagewithTemplate);
    webserver->on("/logger", HTTP_GET, ESPShowPagewithTemplate);
    webserver->on("/setup", HTTP_GET, ESPShowPagewithTemplate);
    webserver->on("/chimesetup", HTTP_GET, ESPShowPagewithTemplate);
    webserver->on("/configupdrequest", HTTP_GET, ESPShowPagewithTemplate);
    webserver->on("/wiki", HTTP_GET, ESPShowPagewithTemplate);
    webserver->on("/wificlear", HTTP_GET, ESPShowPagewithTemplate);
    webserver->on("/reboot", HTTP_GET, ESPShowPagewithTemplate);
    webserver->on("/logout", HTTP_GET, ESPShowPagewithTemplate);

    // special pages/functions
    webserver->on("/message", HTTP_GET, Web_messageReceived);        // Receive command via HTTP
    webserver->on("/savesettings", HTTP_GET, ESPSaveSettings);       // Save ESP settings to Spiffs
    webserver->on("/logclean", HTTP_GET, LogClean);                  // remove Spiffs logs and clean message array
    webserver->on("/logdump", HTTP_GET, LogDump);                    // dump spiffs logs
    webserver->on("/configdump", HTTP_GET, ConfigDump);              // Dump ESPCHIME Saved configuration
    webserver->on("/ConfigFileUploads", HTTP_POST, [](AsyncWebServerRequest *request) { request->send(200); }, ConfigFileUploads);

    // CHIME links
    webserver->on("/ring", HTTP_GET, RingChime);
    webserver->on("/customring", HTTP_GET, CustomRingChime);

    // serving other static information from SPIFFS
    webserver->serveStatic("/", SPIFFS, "/www/");
    //    .setAuthentication(esp_uname, esp_pass);

    // process invalid requests
    webserver->onNotFound(WrongPage);
    // Start Webserver
    webserver->begin();

    AddLogMessageI(F("Webserver started\n"));
}

// ----------------------------------------------------------------------------------------------------------------
// ------  General Webfunctions -----------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------------

void WrongPage(AsyncWebServerRequest *request) {
    //Serial.print(request->url());
    //Serial.println(F(" -> invalid request"));
    String s = F("<h1>");
    s += esp_name;
    s += F("</h1><p>Wrong request</p>");
    s += F("<p><a href=\"/\">Home</a></p>");
    request->send(500, "text/html", makePage(esp_name, s));
}

void ESPShowPagewithTemplate(AsyncWebServerRequest *request) {
    if (!_webAuth(request))
        return;
    String page = request->url();
    String msg;
    msg = F("Webrequest: \"");
    msg += page;
    msg += "\"";

    if (page == "/" || page == "/index.html") {
        page = "/www/index.htm";
    } else if (page == "/wificlear") {
        String msg = F("Webrequest: \"ESP wifi clearing\"\n");
        AddLogMessageI(msg);
        String s = F("<script>setTimeout(function() {window.parent.location.href= \"/\";}, 3000);</script>");
        s += F("<b>ESP will clear wifi credentials and reboot now.<br>");
        s += F("<b>Search for wifi-ssid \"ESP-Doorbell\" and run the wifi-manager, to connect again.<br>");
        request->send(200, "text/html", makePage(esp_name, s));
        WiFi.disconnect(true,true);
        reboot = true;
        rebootdelay = millis();
    } else if (page == "/reboot") {
        String msg = F("Webrequest: \"Reboot ESP\"\n");
        AddLogMessageI(msg);
        reboot = true;
        rebootdelay = millis();
        String s = F("<script>setTimeout(function() {window.parent.location.href= \"/\";}, 3000);</script>");
        s += F("<b>ESP will reboot now.<br>");
        request->send(200, "text/html", makePage(esp_name, s));
    } else if (page == "/logout") {
        String s = F("Logout done.");
        request->send(401, "text/html", makePage(esp_name, s));
        msg += F("Logout done\n");
        AddLogMessageD(msg);
        return;
    } else {
        page = "/www" + page + ".htm";
    }

    msg += F(" -> ");
    msg += page;
    if (SPIFFS.exists(page)) {
        request->send(SPIFFS, page.c_str(), String(), false, TranslateTemplateVars);
    } else {
        msg += F(" file missing so show bin upload!");
        String s = F("<h1>");
        s += esp_name;
        s += F("<br>File missing in SPIFFS:");
        s += page;
        s += F(".</h1><br><h2>Please upload the correct version of spiffs.bin or this file.</h2>");
        s += F("<form method='POST' action='/ConfigFileUploads' enctype='multipart/form-data'>");
        s += F("<input type='file' name='update'><input type='submit' value='Update'></form>");
        request->send(200, "text/html", makePage(esp_name, s));
    }
    msg += F("\n");
    AddLogMessageI(msg);
}

void Web_messageReceived(AsyncWebServerRequest *request) {
    String s = "";
    String webcmd = urlDecode(request->arg("command"));
    if (webcmd != "")
        s += process_messageReceived(webcmd);
    else {
        String msg = F("Webrequest: \"/message\" -> \"?command=\" not provided or empty.\n");
        AddLogMessageW(msg);
        s = F("\"?command=\" not provided or empty.");
    }
    request->send(200, "text/html", s);
}

void ESPSaveSettings(AsyncWebServerRequest *request) {
    String msg = F("Webrequest: \"save ESP Settings \" \n");
    AddLogMessageI(msg);
    // reboot to load new CONFIG
    Save_NewESPConfig_to_SPIFFS(request);
    reboot = true;
    rebootdelay = millis();  // give 2 extra seconds to save config
    String s = F("<script>setTimeout(function() {window.parent.location.href= \"/\";s}, 3000);</script>");
    s += F("<b>Config saved.<br>");
    s += F("<b>ESP will reboot now.<br>");
    request->send(200, "text/html", makePage(esp_name, s));
}

void ConfigDump(AsyncWebServerRequest *request) {
    if (!_webAuth(request))
        return;
    String msg = F("Webrequest: \"/configdump\" \n");
    AddLogMessageI(msg);
    request->send(SPIFFS, "/ESP_CHIME_CONFIG.json", String(), true);
}

void LogClean(AsyncWebServerRequest *request) {
    String msg = F("Webrequest: \"/LogClean\" ->");
    // get WebForm data back in a JSON format for easy updating of settings
    String s = F("<script>setTimeout(function() {window.parent.location.href= \"/\";s}, 3000);</script>");
    // Clean message array
    for (uint i = 0; i < 50; i++) {
        LogMessage[i] = "";
    }
    LogMessageIndexI = 0;
    LogMessageIndexO = 999;
    LogMessage_NewLine = true;
    AddLogMessageI("Start clean logs\n");
    s += "Delete Logfiles: ";
    if (SPIFFS.exists("/chimeprev.log"))
        if (SPIFFS.remove("/chimeprev.log"))
            msg += F("chimeprev.log removed");
        else
            msg += F("chimeprev.log remove failed");
    else
        msg += F("chimeprev.log not there");
    msg += F(", ");
    if (SPIFFS.exists("/chime.log"))
        if (SPIFFS.remove("/chime.log"))
            msg += F("chime.log removed");
        else
            msg += F("chime.log remove failed");
    else
        msg += F("chime.log not there");
    msg += F("\n");
    AddLogMessageI(msg);
    s += msg;
    request->send(200, "text/html", makePage(esp_name, s));
}

void RingChime(AsyncWebServerRequest *request) {
    String msg = F("Ring request received");
    msg += F("\n");
    AddLogMessageI(msg);
    String s = F("<h1>");
    s += esp_name;
    s += F("</h1><p>Ring request received</p>");
    request->send(200, "text/html", makePage(esp_name, s));

    HTTP_Received("On");
    digitalWrite(atoi(PHOTOMOS_GPIO), HIGH);
    RFsend(RFcode);
    digitalWrite(atoi(PHOTOMOS_GPIO), LOW);
    if (!strcmp(SendOff, "yes")) {
        HTTP_Received("Off");
    } else {
        AddLogMessageI("OFF command to domoticz IDX is not send \n");
        AddLogMessageI("Ringing chime: OFF \n");
    }
    AddLogMessageI("\n");
}

void CustomRingChime(AsyncWebServerRequest *request) {
    //string example: http://192.168.xxx.xxx/customring?protocol=2&pulse=712&code=00110000101111001101010110010011
    bool CustomRfComplete = false;
    bool CustomProtocol = false;
    bool CustomPulse = false;
    bool CustomCode = false;
    String msg = F("Custom ring request received");
    msg += F("\n");
    AddLogMessageI(msg);
    
    if (request->hasParam("protocol")) {
        AsyncWebParameter* p = request->getParam("protocol");
        const char* payload = (p->value().c_str());
        strncpy(CustomRfProtocol,payload,3);
        CustomProtocol = true;
    }
    if (request->hasParam("pulse")) {
        AsyncWebParameter* p = request->getParam("pulse");
        const char* payload = (p->value().c_str());
        strncpy(CustomRfPulse,payload,5);
        CustomPulse = true;
    }
    if (request->hasParam("code")) {
        AsyncWebParameter* p = request->getParam("code");
        const char* payload = (p->value().c_str());
        strncpy(CustomRfCode,payload,33);
        CustomCode = true;
    }

    if (CustomProtocol && CustomPulse && CustomCode) {
        String s = F("<h1>");
        s += esp_name;
        s += F("</h1><p>Custom Ring RF message received. <br>Protocol: ");
        s += String(CustomRfProtocol);
        s += F(",<br>Pulse: ");
        s += String(CustomRfPulse); 
        s += F(",<br>Code: ");
        s += String(CustomRfCode);
        s += F("</p>");        
        request->send(200, "text/html", makePage(esp_name, s));
        CustomRFsend(RCSWITCH_GPIO, PHOTOMOS_GPIO, CustomRfProtocol, CustomRfPulse, CustomRfCode);
    
        HTTP_Received("On");
        if (!strcmp(SendOff, "yes")) {
            HTTP_Received("Off");
        } else {
            AddLogMessageI("OFF command to domoticz IDX is not send \n");
            AddLogMessageI("Custom ringing chime: Off \n");
        }
    } else {
        String s = F("<h1>");
        s += esp_name;
        s += F("</h1><p>Custom Ring RF message received. <br>Protocol:");
        if (CustomProtocol) {
            s += String(CustomRfProtocol);
        } else {
            s += F("no valid protocol data.");
            AddLogMessageI("No valid protocol data in custom ring. \n");
        }
        s += F(",<br>Pulse: ");
        if (CustomPulse) {
            s += String(CustomRfPulse); 
        } else {
            s += F("no valid pulse data.");
            AddLogMessageI("No valid pulse data in custom ring. \n");
        }
        s += F(",<br>Code: ");
        if (CustomCode) {
            s += String(CustomRfCode);
        } else {
            s += F("no valid code data.");
            AddLogMessageI("No valid code data in custom ring. \n");
        }
        s += F("</p>");        
        request->send(200, "text/html", makePage(esp_name, s));
    }
}

void LogDump(AsyncWebServerRequest *request) {
    if (!_webAuth(request))
        return;
    String msg = F("Webrequest: \"/logdump\" \n");
    // First dump the previous chime.log
    File ofile = SPIFFS.open("/espchime.log", "w");
    File file = SPIFFS.open("/chimeprev.log", "r");
    if (file || !file.isDirectory()) {
        if (file.size() > CHIME_LOGSIZE + 500) {
            msg += F("---ERROR: chimeprev.log larger than CHIME_LOGSIZE, so assume it is corrupt.");
            msg += file.size();
            msg += "\n";
            file.close();
        } else {
            while (file.available()) {
                ofile.write((char)file.read());
            }
        }
        file.close();
    }
    Serial.println("Start chime.log");
    Serial.flush();
    file = SPIFFS.open("/chime.log", "r");
    if (file || !file.isDirectory()) {
        Serial.printf("Size chime.log %i\n", file.size());
        if (file.size() > CHIME_LOGSIZE + 500) {
            msg += F("---ERROR: chime.log larger than CHIME_LOGSIZE, so assume it is corrupt.");
            msg += file.size();
            msg += "\n";
            file.close();
        } else {
            while (file.available()) {
                ofile.write((char)file.read());
            }
        }
        file.close();
        ofile.close();
    }
    Serial.println("Done");
    // Then dump the current chime.log
    AddLogMessageI(msg);
    request->send(SPIFFS, "/espchime.log", String(), true);
}

void ConfigFileUploads(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
    String msg = F("Webrequest: \"/configfileuploads\" ");
    if (!_webAuth(request))
        return;
    if (!index) {
        msg += F("Load file start: ");
        if (filename.indexOf(".bin") > 0) {
            tbin = true;
            // if filename includes spiffs, update the spiffs partition
            msg += F(" BIN: ");
            msg += filename;
            int cmd = (filename.indexOf("spiffs") > -1) ? U_SPIFFS : U_FLASH;

            if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
                Update.printError(Serial);
            }
        } else {
            if (filename.indexOf(".htm") > 0) {
                tfile = "/www/" + filename;
                msg += F(" HTM: ");
            } else if (filename.indexOf(".css") > 0) {
                tfile = "/www/css/" + filename;
                msg += F(" CSS: ");
            } else if (filename.indexOf(".js") > 0 && filename.indexOf(".json") < 0) {
                tfile = "/www/js/" + filename;
                msg += F(" JS: ");
            } else if (filename == "favicon.ico") {
                tfile = "/www/favicon.ico";
                msg += F("favicon.ico: ");
            }
            msg += filename;
            msg += " -> ";
            msg += tfile;
            msg += " :";
            // Write received data to config file
            hfile = SPIFFS.open(tfile, "w");
            if (!hfile) {
                msg += "failed to open file for writing\n";
                AddLogMessageI(msg);
                return;
            }
        }
    }
    if (tbin) {
        // Write the BIN datablocks
        if (Update.write(data, len) != len) {
            Update.printError(Serial);
        }
    } else {
        // Write the SPIFFS FILE datablocks
        for (size_t i = 0; i < len; i++) {
            hfile.write(data[i]);
        }
    }
    if (final) {
        String s = F("<script>setTimeout(function() {window.parent.location.href= ");
        if (tbin || EspConfig) {
            if (tbin) {
                if (!Update.end(true)) {
                    Update.printError(Serial);
                }
            }
            if (EspConfig)
                hfile.close();

            s += F("\"/\";}, 3000);</script>");
            s += (tbin ? F("New bin is loaded.<br>") : F("New config is loaded.<br>"));
            s += F("The ESPCHIME will now reboot to activate the update.");
            msg += F("Finished updating file");
            msg += F(" -> will reboot now.");
        } else {
            hfile.close();
            s += F("\"/\";}, 1000);</script>");
            s += tfile;
            s += F(" Updated.<br>");
            msg += F("Finished updating file:");
            msg += tfile;
        }
        msg += "\n";
        AddLogMessageI(msg);
        // reboot only when BIN or CHIME CONFIG is loaded
        if (tbin || EspConfig) {
            // reboot to load new BIN
            reboot = true;
            rebootdelay = millis();
            request->send(200, "text/html", makePage(esp_name, s));
        }
        // Don't send a response to allow for multiple uploads
        //request->send(200, "text/html", makePage(esp_name, s));
        // Restore settings from SPIFFS when new file is loaded.
    }
}

// ----------------------------------------------------------------------------------------------------------------
// ------ Helper functions ---------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------------
// Add log message to queue

void AddLogMessage(String msg, String Module, String Function, String Severity, int Line) {
    if (msg == "")
        return;
    // Get current local time
    char logprefix[20];
    if (LogMessage_NewLine) {
        struct tm timeinfo;
        strcpy(logprefix,("[" + Severity + "]").c_str());
        if (getLocalTime(&timeinfo, 100)) {
            char logtime[18];
            strftime(logtime, 20, " %m/%d %H:%M:%S", &timeinfo);
            strcat(logprefix,logtime);
        }
    }
    /*
        #define ARDUHAL_LOG_LEVEL_NONE       (0)
        #define ARDUHAL_LOG_LEVEL_ERROR      (1)
        #define ARDUHAL_LOG_LEVEL_WARN       (2)
        #define ARDUHAL_LOG_LEVEL_INFO       (3)
        #define ARDUHAL_LOG_LEVEL_DEBUG      (4)
        #define ARDUHAL_LOG_LEVEL_VERBOSE    (5)
    */
    if ((Severity == "E" && CORE_DEBUG_LEVEL > 0) ||
        (Severity == "W" && CORE_DEBUG_LEVEL > 1) ||
        (Severity == "I" && CORE_DEBUG_LEVEL > 2) ||
        (Severity == "D" && CORE_DEBUG_LEVEL > 3) ||
        (Severity == "V" && CORE_DEBUG_LEVEL > 4)) {
        Module.replace("src\\", "");  // initial directory src to make it look like the standard log_i() function
        // print the actual line to Serial
        if (LogMessage_NewLine)
            //Serial.printf("[%s][%s:%i] %s(): %s", Severity.c_str(), Module.c_str(), Line, Function.c_str(), msg.c_str());
            Serial.printf("%s [%s:%i] %s(): %s", logprefix, Module.c_str(), Line, Function.c_str(), msg.c_str());
        else
            Serial.print(msg);
    }

    // Set bool to know whether we need to add the Time to the next line
    LogMessage_NewLine = false;
    if (msg.indexOf("\n") > 0)
        LogMessage_NewLine = true;
    // Also log to SPIFFS
    File hlogfile = SPIFFS.open("/chime.log", FILE_APPEND);
    if (!hlogfile) {
        log_e("Error opening chime.log");
    } else {
        if (hlogfile.print(String(logprefix) + ' ' + msg)) {
            //Serial.println("File was written");
        } else {
            log_e("chime.log write failed");
        }
        // cycle logfile when greater than define size
        if (hlogfile.size() > CHIME_LOGSIZE) {
            hlogfile.close();
            SPIFFS.remove("/chimeprev.log");
            SPIFFS.rename("/chime.log", "/chimeprev.log");
            AddLogMessageI("Start new chime.log.\n");
        }
        hlogfile.close();
    }
    // Add message to queue when level is requested
    if ((Severity == "E" && webloglevel > 0) ||
        (Severity == "W" && webloglevel > 1) ||
        (Severity == "I" && webloglevel > 2) ||
        (Severity == "D" && webloglevel > 3) ||
        (Severity == "V" && webloglevel > 4)) {
        //Serial.printf("Queued: %s : %i %s", Severity.c_str(), webloglevel, msg.c_str());
        LogMessage[LogMessageIndexI] = String(logprefix) + ' ' + msg;
        // Set pointer to next message row
        LogMessageIndexI++;
        if (LogMessageIndexI > 49)
            LogMessageIndexI = 0;
    }
    //else
    //Serial.printf("NOT Queued: %s : %i %s", Severity.c_str(), webloglevel, msg.c_str());
}

// Send next message from queue
void SendNextLogMessage() {
    // Check if session is active
    if (LogId == 999)
        return;
    // Check if Last message is confirmed
    if (LogMessageSuccess) {
        //Serial.printf("-s %i / %i\n", LogMessageIndexI, LogMessageIndexO);
        if (LogMessageIndexO > 49)
            LogMessageIndexO = 0;

        //Serial.printf("check for send next message:%i",LogMessageIndexO);
        if (LogMessageIndexO == LogMessageIndexI)
            return;

        String tmp = LogMessage[LogMessageIndexO];
        tmp.replace("\n", "");  // remove ending Newline
        //log_d("send next message:%i - %s", LogMessageIndexO, tmp.c_str());
        LogMessageSuccess = false;
        ws.text(LogId, LogMessage[LogMessageIndexO]);
        LogMessage_Send_time = millis();
        LogMessage_Send_tries = 1;
    } else {
        // Close socket when 5 tries failed
        if (LogMessage_Send_tries >= 5) {
            log_e("Websocket client connection closed after 5 retries: %i\n", LogId);
            ws.close(LogId);
            LogId = 999;
        }
        // retry 4 times in case no confirm was received, else assume de client sn't there anymore.
        else if (LogMessage_Send_time > millis() + 1000) {
            ws.text(LogId, LogMessage[LogMessageIndexO]);
            LogMessage_Send_time = millis();
            LogMessage_Send_tries++;
        }
    }
}

String makePage(String title, String contents) {
    String s = F("\n<!DOCTYPE html><html><head>");

    s += F("<meta name=\"viewport\" content=\"width=device-width,user-scalable=0\">");
    s += F("<title>");
    s += title;
    s += F("</title></head><body>\n");
    s += contents;
    s += F("\n</body></html>\n");
    return s;
}

String urlDecode(String input) {
    String s = input;

    s.replace("%20", " ");
    s.replace("+", " ");
    s.replace("%21", "!");
    s.replace("%22", "\"");
    s.replace("%23", "#");
    s.replace("%24", "$");
    s.replace("%25", "%");
    s.replace("%26", "&");
    s.replace("%27", "\'");
    s.replace("%28", "(");
    s.replace("%29", ")");
    s.replace("%30", "*");
    s.replace("%31", "+");
    s.replace("%2C", ",");
    s.replace("%2E", ".");
    s.replace("%2F", "/");
    s.replace("%2C", ",");
    s.replace("%3A", ":");
    s.replace("%3A", ";");
    s.replace("%3C", "<");
    s.replace("%3D", "=");
    s.replace("%3E", ">");
    s.replace("%3F", "?");
    s.replace("%40", "@");
    s.replace("%5B", "[");
    s.replace("%5C", "\\");
    s.replace("%5D", "]");
    s.replace("%5E", "^");
    s.replace("%5F", "-");
    s.replace("%60", "`");
    return s;
}
