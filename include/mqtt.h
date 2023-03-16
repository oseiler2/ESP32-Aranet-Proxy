#ifndef _MQTT_H
#define _MQTT_H

#include <globals.h>
#include <ArduinoJson.h>

#define MQTT_MUTEX_DEF_WAIT pdMS_TO_TICKS(15000)

// If you issue really large certs (e.g. long CN, extra options) this value may need to be
// increased, but 1600 is plenty for a typical CN and standard option openSSL issued cert.
#define MQTT_CERT_SIZE 8192

// Use larger of cert or config for MQTT buffer size.
#define MQTT_BUFFER_SIZE MQTT_CERT_SIZE > CONFIG_SIZE ? MQTT_CERT_SIZE : CONFIG_SIZE

extern boolean takeRadioMutex(TickType_t blockTime);
extern void giveRadioMutex();

namespace mqtt {

  void setupMqtt();

  void publishMeasurement(const char* name, DynamicJsonDocument* payload);
  void publishConfiguration();
  void publishStatusMsg(const char* statusMessage);

  void mqttLoop(void* pvParameters);

  extern TaskHandle_t mqttTask;
}

#endif