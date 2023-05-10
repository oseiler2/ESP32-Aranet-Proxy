#include <Arduino.h>
#include <config.h>
#include <configManager.h>

#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Local logging tag
static const char TAG[] = __FILE__;

Config config;

// Allocate a temporary JsonDocument
// Don't forget to change the capacity to match your requirements.
// Use arduinojson.org/v6/assistant to compute the capacity.
/*
{
  "deviceId": 65535,
  "mqttTopic": "123456789112345678921",
  "mqttUsername": "123456789112345678921",
  "mqttPassword": "123456789112345678921",
  "mqttHost": "1234567891123456789212345678931",
  "mqttUseTls": false,
  "mqttInsecure": false,
  "mqttServerPort": 65535,
  "ssd1306Rows": 64,
  "scanInterval": 65535,
}
*/

void setupConfigManager() {
  if (!LittleFS.begin(true)) {
    ESP_LOGW(TAG, "LittleFS failed! Already tried formatting.");
    if (!LittleFS.begin()) {
      delay(100);
      ESP_LOGW(TAG, "LittleFS failed second time!");
    }
  }
}

#define DEFAULT_MQTT_TOPIC      "aranetproxy"
#define DEFAULT_MQTT_HOST        "127.0.0.1"
#define DEFAULT_MQTT_PORT               1883
#define DEFAULT_MQTT_USERNAME   "aranetproxy"
#define DEFAULT_MQTT_PASSWORD   "aranetproxy"
#define DEFAULT_MQTT_USE_TLS           false
#define DEFAULT_MQTT_INSECURE          false
#define DEFAULT_DEVICE_ID                  0
#define DEFAULT_SSD1306_ROWS              64
#define DEFAULT_SCAN_INTERVAL            600

void getDefaultConfiguration(Config& config) {
  config.deviceId = DEFAULT_DEVICE_ID;
  strlcpy(config.mqttTopic, DEFAULT_MQTT_TOPIC, sizeof(DEFAULT_MQTT_TOPIC));
  strlcpy(config.mqttUsername, DEFAULT_MQTT_USERNAME, sizeof(DEFAULT_MQTT_USERNAME));
  strlcpy(config.mqttPassword, DEFAULT_MQTT_PASSWORD, sizeof(DEFAULT_MQTT_PASSWORD));
  strlcpy(config.mqttHost, DEFAULT_MQTT_HOST, sizeof(DEFAULT_MQTT_HOST));
  config.mqttUseTls = DEFAULT_MQTT_USE_TLS;
  config.mqttInsecure = DEFAULT_MQTT_INSECURE;
  config.mqttServerPort = DEFAULT_MQTT_PORT;
  config.ssd1306Rows = DEFAULT_SSD1306_ROWS;
  config.scanInterval = DEFAULT_SCAN_INTERVAL;
}

void logConfiguration(const Config& config) {
  ESP_LOGD(TAG, "deviceId: %u", config.deviceId);
  ESP_LOGD(TAG, "mqttTopic: %s", config.mqttTopic);
  ESP_LOGD(TAG, "mqttUsername: %s", config.mqttUsername);
  ESP_LOGD(TAG, "mqttPassword: %s", config.mqttPassword);
  ESP_LOGD(TAG, "mqttHost: %s", config.mqttHost);
  ESP_LOGD(TAG, "mqttUseTls: %s", config.mqttUseTls ? "true" : "false");
  ESP_LOGD(TAG, "mqttInsecure: %s", config.mqttInsecure ? "true" : "false");
  ESP_LOGD(TAG, "mqttPort: %u", config.mqttServerPort);
  ESP_LOGD(TAG, "ssd1306Rows: %u", config.ssd1306Rows);
  ESP_LOGD(TAG, "scanInterval: %u", config.scanInterval);
}

boolean loadConfiguration(Config& config) {
  File file = LittleFS.open(CONFIG_FILENAME, FILE_READ);
  if (!file) {
    ESP_LOGW(TAG, "Could not open config file");
    return false;
  }

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/v6/assistant to compute the capacity.
  DynamicJsonDocument doc(CONFIG_SIZE);

  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    ESP_LOGW(TAG, "Failed to parse config file: %s", error.f_str());
    file.close();
    return false;
  }

  // Copy values from the JsonDocument to the Config
  config.deviceId = doc["deviceId"] | DEFAULT_DEVICE_ID;
  strlcpy(config.mqttTopic,
    doc["mqttTopic"] | DEFAULT_MQTT_TOPIC,
    sizeof(config.mqttTopic));
  strlcpy(config.mqttUsername,
    doc["mqttUsername"] | DEFAULT_MQTT_USERNAME,
    sizeof(config.mqttUsername));
  strlcpy(config.mqttPassword,
    doc["mqttPassword"] | DEFAULT_MQTT_PASSWORD,
    sizeof(config.mqttPassword));
  strlcpy(config.mqttHost,
    doc["mqttHost"] | DEFAULT_MQTT_HOST,
    sizeof(config.mqttHost));
  config.mqttServerPort = doc["mqttServerPort"] | DEFAULT_MQTT_PORT;
  config.mqttUseTls = doc["mqttUseTls"] | DEFAULT_MQTT_USE_TLS;
  config.mqttInsecure = doc["mqttInsecure"] | DEFAULT_MQTT_INSECURE;
  config.ssd1306Rows = doc["ssd1306Rows"] | DEFAULT_SSD1306_ROWS;
  config.scanInterval = doc["scanInterval"] | DEFAULT_SCAN_INTERVAL;

  file.close();
  return true;
}

boolean saveConfiguration(const Config& config) {
  ESP_LOGD(TAG, "###################### saveConfiguration");
  logConfiguration(config);
  // Delete existing file, otherwise the configuration is appended to the file
  if (LittleFS.exists(CONFIG_FILENAME)) {
    LittleFS.remove(CONFIG_FILENAME);
  }

  // Open file for writing
  File file = LittleFS.open(CONFIG_FILENAME, FILE_WRITE);
  if (!file) {
    ESP_LOGW(TAG, "Could not create config file for writing");
    return false;
  }

  DynamicJsonDocument doc(CONFIG_SIZE);

  // Set the values in the document
  doc["deviceId"] = config.deviceId;
  doc["mqttTopic"] = config.mqttTopic;
  doc["mqttUsername"] = config.mqttUsername;
  doc["mqttPassword"] = config.mqttPassword;
  doc["mqttHost"] = config.mqttHost;
  doc["mqttServerPort"] = config.mqttServerPort;
  doc["mqttUseTls"] = config.mqttUseTls;
  doc["mqttInsecure"] = config.mqttInsecure;
  doc["ssd1306Rows"] = config.ssd1306Rows;
  doc["scanInterval"] = config.scanInterval;

  // Serialize JSON to file
  if (serializeJson(doc, file) == 0) {
    ESP_LOGW(TAG, "Failed to write to file");
    file.close();
    return false;
  }

  // Close the file
  file.close();
  ESP_LOGD(TAG, "Stored configuration successfully");
  return true;
}

// Prints the content of a file to the Serial
void printFile() {
  // Open file for reading
  File file = LittleFS.open(CONFIG_FILENAME, FILE_READ);
  if (!file) {
    ESP_LOGW(TAG, "Could not open config file");
    return;
  }

  // Extract each characters by one by one
  while (file.available()) {
    Serial.print((char)file.read());
  }
  Serial.println();

  // Close the file
  file.close();
}
