#include <Arduino.h>

bool Domoticz_JSON_Switch(const char *Idx, const char* State);
bool Domoticz_MQTT_Switch(const char *Idx, const char* State);

void Mqtt_begin();
bool Mqtt_Connect();
bool Mqtt_Loop();
void Mqtt_messageReceived(String &topic, String &payload);

void HTTP_Received(const char *State);

String process_messageReceived(String payload);