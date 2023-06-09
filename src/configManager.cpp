
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

#define DEFAULT_DEVICE_ID                  0
#define DEFAULT_MQTT_TOPIC     "aranetproxy"
#define DEFAULT_MQTT_HOST        "127.0.0.1"
#define DEFAULT_MQTT_PORT               1883
#define DEFAULT_MQTT_USERNAME   "aranetproxy"
#define DEFAULT_MQTT_PASSWORD   "aranetproxy"
#define DEFAULT_MQTT_USE_TLS           false
#define DEFAULT_MQTT_INSECURE          false
#define DEFAULT_SSD1306_ROWS              64
#define DEFAULT_SCAN_INTERVAL            600

std::vector<ConfigParameterBase<Config>*> configParameterVector;

void setupConfigManager() {
  if (!LittleFS.begin(true)) {
    ESP_LOGW(TAG, "LittleFS failed! Already tried formatting.");
    if (!LittleFS.begin()) {
      delay(100);
      ESP_LOGW(TAG, "LittleFS failed second time!");
    }
  }
  //  configParameterVector.clear();
  configParameterVector.push_back(new Uint16ConfigParameter<Config>("deviceId", "Device ID", &Config::deviceId, DEFAULT_DEVICE_ID));
  configParameterVector.push_back(new CharArrayConfigParameter<Config>("mqttTopic", "MQTT topic", (char Config::*) & Config::mqttTopic, DEFAULT_MQTT_TOPIC, MQTT_TOPIC_ID_LEN));
  configParameterVector.push_back(new CharArrayConfigParameter<Config>("mqttUsername", "MQTT username", (char Config::*) & Config::mqttUsername, DEFAULT_MQTT_USERNAME, MQTT_USERNAME_LEN));
  configParameterVector.push_back(new CharArrayConfigParameter<Config>("mqttPassword", "MQTT password", (char Config::*) & Config::mqttPassword, DEFAULT_MQTT_PASSWORD, MQTT_PASSWORD_LEN));
  configParameterVector.push_back(new CharArrayConfigParameter<Config>("mqttHost", "MQTT host", (char Config::*) & Config::mqttHost, DEFAULT_MQTT_HOST, MQTT_HOSTNAME_LEN));
  configParameterVector.push_back(new Uint16ConfigParameter<Config>("mqttServerPort", "MQTT port", &Config::mqttServerPort, DEFAULT_MQTT_PORT));
  configParameterVector.push_back(new BooleanConfigParameter<Config>("mqttUseTls", "MQTT use TLS", &Config::mqttUseTls, DEFAULT_MQTT_USE_TLS));
  configParameterVector.push_back(new BooleanConfigParameter<Config>("mqttInsecure", "MQTT Ignore certificate errors", &Config::mqttInsecure, DEFAULT_MQTT_INSECURE));
  configParameterVector.push_back(new Uint8ConfigParameter<Config>("ssd1306Rows", "SSD1306 rows", &Config::ssd1306Rows, DEFAULT_SSD1306_ROWS, 32, 64, true));
  configParameterVector.push_back(new Uint16ConfigParameter<Config>("scanInterval", "Interval (secs) between BT scans", &Config::scanInterval, DEFAULT_SCAN_INTERVAL, 60, 65535, false));
}

std::vector<ConfigParameterBase<Config>*> getConfigParameters() {
  return configParameterVector;
}

void getDefaultConfiguration(Config& _config) {
  for (ConfigParameterBase<Config>* configParameter : configParameterVector) {
    configParameter->setToDefault(_config);
  }
}

void logConfiguration(const Config _config) {
  for (ConfigParameterBase<Config>* configParameter : configParameterVector) {
    ESP_LOGD(TAG, "%s: %s", configParameter->getId(), configParameter->toString(_config).c_str());
  }
}

boolean loadConfiguration(Config& _config) {
  File file = LittleFS.open(CONFIG_FILENAME, FILE_READ);
  if (!file) {
    ESP_LOGW(TAG, "Could not open config file");
    return false;
  }

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/v6/assistant to compute the capacity.
  DynamicJsonDocument* doc = new DynamicJsonDocument(CONFIG_SIZE);

  DeserializationError error = deserializeJson(*doc, file);
  if (error) {
    ESP_LOGW(TAG, "Failed to parse config file: %s", error.f_str());
    file.close();
    return false;
  }

  for (ConfigParameterBase<Config>* configParameter : configParameterVector) {
    configParameter->fromJson(_config, doc, true);
  }

  file.close();
  return true;
}

boolean saveConfiguration(const Config _config) {
  ESP_LOGD(TAG, "###################### saveConfiguration");
  logConfiguration(_config);
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

  DynamicJsonDocument* doc = new DynamicJsonDocument(CONFIG_SIZE);
  for (ConfigParameterBase<Config>* configParameter : configParameterVector) {
    configParameter->toJson(_config, doc);
  }

  // Serialize JSON to file
  if (serializeJson(*doc, file) == 0) {
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
