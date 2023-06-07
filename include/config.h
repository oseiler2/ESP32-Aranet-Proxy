#ifndef _CONFIG_H
#define _CONFIG_H

#include <logging.h>

#define OTA_URL               "https://otahost/co2monitor/firmware.json"
#define OTA_APP               "aranetproxy"
//#define OTA_POLL

#if CONFIG_IDF_TARGET_ESP32

#define LED_PIN                2
#define BTN_1                  0
#define SDA_PIN              SDA
#define SCL_PIN              SCL

#elif CONFIG_IDF_TARGET_ESP32S3

#define LED_PIN                2
#define BTN_1                  0
#define SDA_PIN               14
#define SCL_PIN               21

#endif

#define I2C_CLK 100000UL

static const char* CONFIG_FILENAME = "/config.json";
static const char* MQTT_ROOT_CA_FILENAME = "/mqtt_root_ca.pem";
static const char* MQTT_CLIENT_CERT_FILENAME = "/mqtt_client_cert.pem";
static const char* MQTT_CLIENT_KEY_FILENAME = "/mqtt_client_key.pem";
static const char* TEMP_MQTT_ROOT_CA_FILENAME = "/temp_mqtt_root_ca.pem";
static const char* ROOT_CA_FILENAME = "/root_ca.pem";

#define MQTT_QUEUE_LENGTH      25

#define PWM_CHANNEL_LEDS        0

// ----------------------------  Config struct ------------------------------------- 
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

#endif
