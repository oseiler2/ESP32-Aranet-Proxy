#ifndef _CONFIG_MANAGER_H
#define _CONFIG_MANAGER_H

#include <Arduino.h>
#include <config.h>

#define CONFIG_SIZE 512

#define MQTT_USERNAME_LEN 20
#define MQTT_PASSWORD_LEN 20
#define MQTT_HOSTNAME_LEN 30
#define MQTT_TOPIC_ID_LEN 30
#define SSID_LEN 32
#define WIFI_PASSWORD_LEN 64

struct Config {
  uint16_t deviceId;
  char mqttTopic[MQTT_TOPIC_ID_LEN + 1];
  char mqttUsername[MQTT_USERNAME_LEN + 1];
  char mqttPassword[MQTT_PASSWORD_LEN + 1];
  char mqttHost[MQTT_HOSTNAME_LEN + 1];
  bool mqttUseTls;
  bool mqttInsecure;
  uint16_t mqttServerPort;
  uint8_t ssd1306Rows;
  uint16_t scanInterval;
};

static uint16_t Config::* deviceIdPtr = &Config::deviceId;
static char Config::* mqttTopicPtr = (char Config::*) & Config::mqttTopic;
static char Config::* mqttUsernamePtr = (char Config::*) & Config::mqttUsername;
static char Config::* mqttPasswordPtr = (char Config::*) & Config::mqttPassword;
static char Config::* mqttHostPtr = (char Config::*) & Config::mqttHost;
static bool Config::* mqttUseTlsPtr = &Config::mqttUseTls;
static bool Config::* mqttInsecurePtr = &Config::mqttInsecure;
static uint16_t Config::* mqttServerPortPtr = &Config::mqttServerPort;
static uint8_t Config::* ssd1306RowsPtr = &Config::ssd1306Rows;
static uint16_t Config::* scanIntervalPtr = &Config::scanInterval;

void setupConfigManager();
void getDefaultConfiguration(Config& config);
boolean loadConfiguration(Config& config);
boolean saveConfiguration(const Config& config);
void logConfiguration(const Config& config);
void printFile();

extern Config config;

#endif