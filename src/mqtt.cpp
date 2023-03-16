#include <mqtt.h>
#include <Arduino.h>
#include <config.h>

#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <i2c.h>
#include <configManager.h>
#include <wifiManager.h>
#include <ota.h>

#include <LittleFS.h>

// Local logging tag
static const char TAG[] = __FILE__;

namespace mqtt {

  struct MqttMessage {
    uint8_t cmd;
    DynamicJsonDocument* payload;
    char* statusMessage;
  };

  const uint8_t X_CMD_PUBLISH_SENSORS = bit(0);
  const uint8_t X_CMD_PUBLISH_CONFIGURATION = bit(1);
  const uint8_t X_CMD_PUBLISH_STATUS_MSG = bit(2);

  TaskHandle_t mqttTask;
  QueueHandle_t mqttQueue;

  WiFiClient* wifiClient;
  PubSubClient* mqtt_client;

  uint32_t lastReconnectAttempt = 0;
  uint16_t connectionAttempts = 0;

  StaticJsonDocument<CONFIG_SIZE> doc;

  char* cloneStr(const char* original) {
    char* copy = (char*)malloc(strlen(original) + 1);
    strcpy(copy, original);
    return copy;
  }

  void publishMeasurement(const char* name, DynamicJsonDocument* _payload) {
    if (!WiFi.isConnected() || !mqtt_client->connected()) return;
    MqttMessage msg;
    msg.cmd = X_CMD_PUBLISH_SENSORS;
    msg.payload = _payload;
    msg.statusMessage = cloneStr(name);
    if (mqttQueue) xQueueSendToBack(mqttQueue, (void*)&msg, pdMS_TO_TICKS(100));
  }

  boolean publishMeasurementInternal(MqttMessage queueMsg) {
    char topic[256];
    char msg[256];

    sprintf(topic, "%s/%u/up/sensors/%s", config.mqttTopic, config.deviceId, queueMsg.statusMessage);
    free(queueMsg.statusMessage);
    // Serialize JSON to file
    if (serializeJson(*queueMsg.payload, msg) == 0) {
      ESP_LOGW(TAG, "Failed to serialise payload");
      delete queueMsg.payload;
      return true; // pretend to have been successful to prevent queue from clogging up
    }
    delete queueMsg.payload;
    if (strncmp(msg, "null", 4) == 0) {
      ESP_LOGD(TAG, "Nothing to publish");
      return true; // pretend to have been successful to prevent queue from clogging up
    }
    ESP_LOGD(TAG, "Publishing sensor values: %s:%s", topic, msg);
    if (!mqtt_client->publish(topic, msg)) {
      ESP_LOGE(TAG, "publish sensors failed!");
      return false;
    }
    return true;
  }

  void publishConfiguration() {
    MqttMessage msg;
    msg.cmd = X_CMD_PUBLISH_CONFIGURATION;
    msg.statusMessage = nullptr;
    if (mqttQueue) xQueueSendToBack(mqttQueue, (void*)&msg, pdMS_TO_TICKS(100));
  }

  void setMqttCerts(WiFiClientSecure* wifiClient, const char* mqttRootCertFilename, const char* mqttClientKeyFilename, const char* mqttClientCertFilename) {
    File root_ca_file = LittleFS.open(mqttRootCertFilename, "r");
    if (root_ca_file) {
      ESP_LOGD(TAG, "Loading MQTT root ca from FS (%s)", mqttRootCertFilename);
      wifiClient->loadCACert(root_ca_file, root_ca_file.size());
      root_ca_file.close();
    }
    File client_key_file = LittleFS.open(mqttClientKeyFilename, "r");
    if (client_key_file) {
      ESP_LOGD(TAG, "Loading MQTT client key from FS (%s)", mqttClientKeyFilename);
      wifiClient->loadPrivateKey(client_key_file, client_key_file.size());
      client_key_file.close();
    }
    File client_cert_file = LittleFS.open(mqttClientCertFilename, "r");
    if (client_cert_file) {
      ESP_LOGD(TAG, "Loading MQTT client cert from FS (%s)", mqttClientCertFilename);
      wifiClient->loadCertificate(client_cert_file, client_cert_file.size());
      client_cert_file.close();
    }
  }

  boolean testMqttConfig(WiFiClient* wifiClient, Config testConfig) {
    char buf[128];
    boolean mqttTestSuccess;
    PubSubClient* testMqttClient = new PubSubClient(*wifiClient);
    testMqttClient->setServer(testConfig.mqttHost, testConfig.mqttServerPort);
    sprintf(buf, "Aranet-proxy-%u-%s", testConfig.deviceId, WifiManager::getMac().c_str());
    mqttTestSuccess = testMqttClient->connect(buf, testConfig.mqttUsername, testConfig.mqttPassword);
    if (mqttTestSuccess) {
      ESP_LOGD(TAG, "Test MQTT connected");
      sprintf(buf, "%s/%u/up/status", testConfig.mqttTopic, testConfig.deviceId);
      mqttTestSuccess = testMqttClient->publish(buf, "{\"test\":true}");
      if (!mqttTestSuccess) ESP_LOGE(TAG, "connecting using new mqtt settings failed!");
    }
    delete testMqttClient;
    return mqttTestSuccess;
  }

  boolean publishConfigurationInternal() {
    char buf[256];
    char msg[CONFIG_SIZE];
    doc.clear();
    doc["appVersion"] = APP_VERSION;
    doc["mqttHost"] = config.mqttHost;
    doc["mqttServerPort"] = config.mqttServerPort;
    doc["mqttUsername"] = config.mqttUsername;
    doc["mqttTopic"] = config.mqttTopic;
    doc["mqttUseTls"] = config.mqttUseTls;
    doc["mqttInsecure"] = config.mqttInsecure;
    sprintf(buf, "%s", WifiManager::getMac().c_str());
    doc["mac"] = buf;
    sprintf(buf, "%s", WiFi.localIP().toString().c_str());
    doc["ip"] = buf;
    doc["ssd1306Rows"] = config.ssd1306Rows;
    doc["scanInterval"] = config.scanInterval;
    if (serializeJson(doc, msg) == 0) {
      ESP_LOGW(TAG, "Failed to serialise payload");
      return true; // pretend to have been successful to prevent queue from clogging up
    }
    sprintf(buf, "%s/%u/up/config", config.mqttTopic, config.deviceId);
    ESP_LOGI(TAG, "Publishing configuration: %s:%s", buf, msg);
    if (!mqtt_client->publish(buf, msg)) {
      ESP_LOGE(TAG, "publish configuration failed!");
      return false;
    }
    return true;
  }

  void publishStatusMsg(const char* statusMessage) {
    MqttMessage msg;
    msg.cmd = X_CMD_PUBLISH_STATUS_MSG;
    msg.statusMessage = cloneStr(statusMessage);
    if (mqttQueue) xQueueSendToBack(mqttQueue, (void*)&msg, pdMS_TO_TICKS(100));
  }

  boolean publishStatusMsgInternal(char* statusMessage) {
    if (strlen(statusMessage) > 200) {
      free(statusMessage);
      return true;// pretend to have been successful to prevent queue from clogging up
    }
    char topic[256];
    sprintf(topic, "%s/%u/up/status", config.mqttTopic, config.deviceId);
    char msg[256];
    doc.clear();
    doc["msg"] = statusMessage;
    free(statusMessage);
    if (serializeJson(doc, msg) == 0) {
      ESP_LOGW(TAG, "Failed to serialise payload");
      return true;// pretend to have been successful to prevent queue from clogging up
    }
    if (!mqtt_client->publish(topic, msg)) {
      ESP_LOGE(TAG, "publish status msg failed!");
      return false;
    }
    return true;
  }

  // Helper to write a file to fs
  bool writeFile(const char* name, unsigned char* contents) {
    File f;
    if (!(f = LittleFS.open(name, FILE_WRITE))) {
      return false;
    }
    int len = strlen((char*)contents);
    if (f.write(contents, len) != len) {
      f.close();
      return false;
    }
    f.close();
    return true;
  }

  void callback(char* topic, byte* payload, unsigned int length) {
    char buf[256];
    char msg[length + 1];
    strncpy(msg, (char*)payload, length);
    msg[length] = 0x00;
    ESP_LOGI(TAG, "Message arrived [%s] %s", topic, msg);

    sprintf(buf, "%s/%u/down/", config.mqttTopic, config.deviceId);
    int16_t cmdIdx = -1;
    if (strncmp(topic, buf, strlen(buf)) == 0) {
      ESP_LOGI(TAG, "Device specific downlink message arrived [%s]", topic);
      cmdIdx = strlen(buf);
    }
    sprintf(buf, "%s/down/", config.mqttTopic);
    if (strncmp(topic, buf, strlen(buf)) == 0) {
      ESP_LOGI(TAG, "Device agnostic downlink message arrived [%s]", topic);
      cmdIdx = strlen(buf);
    }
    if (cmdIdx < 0) return;
    strncpy(buf, topic + cmdIdx, strlen(topic) - cmdIdx + 1);
    ESP_LOGI(TAG, "Received command [%s]", buf);

    if (strncmp(buf, "getConfig", strlen(buf)) == 0) {
      publishConfiguration();
    } else if (strncmp(buf, "setConfig", strlen(buf)) == 0) {
      doc.clear();
      DeserializationError error = deserializeJson(doc, msg);
      if (error) {
        ESP_LOGW(TAG, "Failed to parse message: %s", error.f_str());
        return;
      }
      bool rebootRequired = false;
      if (doc.containsKey("ssd1306Rows")) { config.ssd1306Rows = doc["ssd1306Rows"].as<uint8_t>(); rebootRequired = true; }
      if (doc.containsKey("scanInterval")) { config.scanInterval = doc["scanInterval"].as<uint16_t>(); }

      Config mqttConfig = config;
      bool mqttConfigUpdated = false;
      bool mqttTestSuccess = true;
      if (doc.containsKey("mqttHost")) { strlcpy(mqttConfig.mqttHost, doc["mqttHost"], sizeof(config.mqttHost)); mqttConfigUpdated = true; }
      if (doc.containsKey("mqttServerPort")) { mqttConfig.mqttServerPort = doc["mqttServerPort"].as<uint16_t>(); mqttConfigUpdated = true; }
      if (doc.containsKey("mqttUsername")) { strlcpy(mqttConfig.mqttUsername, doc["mqttUsername"], sizeof(config.mqttUsername)); mqttConfigUpdated = true; }
      if (doc.containsKey("mqttPassword")) { strlcpy(mqttConfig.mqttPassword, doc["mqttPassword"], sizeof(config.mqttPassword)); mqttConfigUpdated = true; }
      if (doc.containsKey("mqttTopic")) { strlcpy(mqttConfig.mqttTopic, doc["mqttTopic"], sizeof(config.mqttTopic)); mqttConfigUpdated = true; }
      if (doc.containsKey("mqttUseTls")) { mqttConfig.mqttUseTls = doc["mqttUseTls"].as<boolean>(); mqttConfigUpdated = true; }
      if (doc.containsKey("mqttInsecure")) { mqttConfig.mqttInsecure = doc["mqttInsecure"].as<boolean>(); mqttConfigUpdated = true; }

      if (mqttConfigUpdated) {
        WiFiClient* testWifiClient;
        if (mqttConfig.mqttUseTls) {
          testWifiClient = new WiFiClientSecure();
          if (mqttConfig.mqttInsecure) {
            ((WiFiClientSecure*)testWifiClient)->setInsecure();
          }
          setMqttCerts((WiFiClientSecure*)testWifiClient, MQTT_ROOT_CA_FILENAME, MQTT_CLIENT_KEY_FILENAME, MQTT_CLIENT_CERT_FILENAME);
        } else {
          testWifiClient = new WiFiClient();
        }
        mqttTestSuccess = testMqttConfig(testWifiClient, config);
        delete testWifiClient;
        if (mqttTestSuccess) {
          config = mqttConfig;
          rebootRequired = true;
        }
      }
      if (saveConfiguration(config) && rebootRequired) {
        publishStatusMsgInternal(cloneStr("configuration updated - rebooting shortly"));
        delay(2000);
        esp_restart();
      }
    } else if (strncmp(buf, "installMqttRootCa", strlen(buf)) == 0) {
      ESP_LOGD(TAG, "installMqttRootCa");
      if (!writeFile(TEMP_MQTT_ROOT_CA_FILENAME, (unsigned char*)&msg[0])) {
        ESP_LOGW(TAG, "Error writing mqtt root ca");
        publishStatusMsgInternal(cloneStr("Error writing cert to FS"));
        return;
      }
      bool mqttTestSuccess = config.mqttInsecure || !config.mqttUseTls; // no need to test if not using tls, or not checking certs
      if (config.mqttUseTls && !config.mqttInsecure) {
        ESP_LOGD(TAG, "test connection using new ca");
        // test connection using cert
        WiFiClientSecure* testWifiClient = new WiFiClientSecure();
        setMqttCerts(testWifiClient, TEMP_MQTT_ROOT_CA_FILENAME, MQTT_CLIENT_KEY_FILENAME, MQTT_CLIENT_CERT_FILENAME);
        mqttTestSuccess = testMqttConfig(testWifiClient, config);
        delete testWifiClient;
      }
      ESP_LOGD(TAG, "mqttTestSuccess %u", mqttTestSuccess);
      if (mqttTestSuccess) {
        if (LittleFS.exists(MQTT_ROOT_CA_FILENAME) && !LittleFS.remove(MQTT_ROOT_CA_FILENAME)) {
          ESP_LOGE(TAG, "Failed to remove original CA file");
          publishStatusMsgInternal(cloneStr("Could not remove original CA - giving up"));
          return;  // leave old file in place and give up.
        }
        if (!LittleFS.rename(TEMP_MQTT_ROOT_CA_FILENAME, MQTT_ROOT_CA_FILENAME)) {
          publishStatusMsgInternal(cloneStr("Could not replace original CA with new CA - PANIC - giving up"));
          ESP_LOGE(TAG, "Failed to move temporary CA file");
          config.mqttInsecure = true;
          saveConfiguration(config);
          delay(2000);
          esp_restart();
          return;
        }
        ESP_LOGI(TAG, "installed and tested new CA, rebooting shortly");
        publishStatusMsgInternal(cloneStr("installed and tested new CA - rebooting shortly"));
        delay(2000);
        esp_restart();
      } else {
        ESP_LOGE(TAG, "publish connect msg failed!");
        publishStatusMsgInternal(cloneStr("Connecting using the new CA failed - reverting"));
        if (!LittleFS.remove(TEMP_MQTT_ROOT_CA_FILENAME)) ESP_LOGW(TAG, "Failed to remove temporary CA file");
      }
    } else if (strncmp(buf, "installRootCa", strlen(buf)) == 0) {
      ESP_LOGD(TAG, "installRootCa");
      if (!writeFile(ROOT_CA_FILENAME, (unsigned char*)&msg[0])) {
        ESP_LOGW(TAG, "Error writing root ca");
        publishStatusMsgInternal(cloneStr("Error writing cert to FS"));
      }
    } else if (strncmp(buf, "resetWifi", strlen(buf)) == 0) {
      WifiManager::resetSettings();
    } else if (strncmp(buf, "ota", strlen(buf)) == 0) {
      OTA::checkForUpdate();
    } else if (strncmp(buf, "forceota", strlen(buf)) == 0) {
      OTA::forceUpdate(msg);
    } else if (strncmp(buf, "reboot", strlen(buf)) == 0) {
      esp_restart();
    }
  }

  void reconnect() {
    if (!WiFi.isConnected() || mqtt_client->connected()) return;
    if (millis() - lastReconnectAttempt < 60000) return;
    char topic[256];
    sprintf(topic, "Aranet-proxy-%u-%s", config.deviceId, WifiManager::getMac().c_str());
    lastReconnectAttempt = millis();
    ESP_LOGD(TAG, "Attempting MQTT connection...");
    connectionAttempts++;
    if (mqtt_client->connect(topic, config.mqttUsername, config.mqttPassword)) {
      ESP_LOGD(TAG, "MQTT connected");
      sprintf(topic, "%s/%u/down/#", config.mqttTopic, config.deviceId);
      mqtt_client->subscribe(topic);
      sprintf(topic, "%s/down/#", config.mqttTopic);
      mqtt_client->subscribe(topic);
      sprintf(topic, "%s/%u/up/status", config.mqttTopic, config.deviceId);
      char msg[256];
      doc.clear();
      doc["online"] = true;
      doc["connectionAttempts"] = connectionAttempts;
      if (serializeJson(doc, msg) == 0) {
        ESP_LOGW(TAG, "Failed to serialise payload");
        return;
      }
      if (mqtt_client->publish(topic, msg))
        connectionAttempts = 0;
      else
        ESP_LOGE(TAG, "publish connect msg failed!");
    } else {
      ESP_LOGW(TAG, "MQTT connection failed, rc=%i", mqtt_client->state());
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  void setupMqtt() {
    mqttQueue = xQueueCreate(MQTT_QUEUE_LENGTH, sizeof(struct MqttMessage));
    if (mqttQueue == NULL) {
      ESP_LOGE(TAG, "Queue creation failed!");
    }

    if (config.mqttUseTls) {
      wifiClient = new WiFiClientSecure();
      if (config.mqttInsecure) {
        ((WiFiClientSecure*)wifiClient)->setInsecure();
      }
      setMqttCerts((WiFiClientSecure*)wifiClient, MQTT_ROOT_CA_FILENAME, MQTT_CLIENT_KEY_FILENAME, MQTT_CLIENT_CERT_FILENAME);
    } else {
      wifiClient = new WiFiClient();
    }

    mqtt_client = new PubSubClient(*wifiClient);
    mqtt_client->setServer(config.mqttHost, config.mqttServerPort);
    mqtt_client->setCallback(callback);
    if (!mqtt_client->setBufferSize(MQTT_BUFFER_SIZE)) ESP_LOGE(TAG, "mqtt_client->setBufferSize failed!");
  }

  void mqttLoop(void* pvParameters) {
    _ASSERT((uint32_t)pvParameters == 1);
    lastReconnectAttempt = millis() - 60000;
    BaseType_t notified;
    MqttMessage msg;
    while (1) {
      //      notified = xQueueReceive(mqttQueue, &msg, pdMS_TO_TICKS(100));
      notified = xQueuePeek(mqttQueue, &msg, pdMS_TO_TICKS(100));
      if (notified == pdPASS) {
        if (!takeRadioMutex(MQTT_MUTEX_DEF_WAIT)) {
          ESP_LOGI(TAG, "Failed taking mutex - skipping publish op");
        } else {
          if (msg.cmd == X_CMD_PUBLISH_CONFIGURATION) {
            if (publishConfigurationInternal()) {
              xQueueReceive(mqttQueue, &msg, pdMS_TO_TICKS(100));
            }
          } else if (msg.cmd == X_CMD_PUBLISH_SENSORS) {
            if (publishMeasurementInternal(msg)) {
              xQueueReceive(mqttQueue, &msg, pdMS_TO_TICKS(100));
            }
          } else if (msg.cmd == X_CMD_PUBLISH_STATUS_MSG) {
            if (publishStatusMsgInternal(msg.statusMessage)) {
              xQueueReceive(mqttQueue, &msg, pdMS_TO_TICKS(100));
            }
          }
          giveRadioMutex();
        }
      }
      if (!mqtt_client->connected()) {
        reconnect();
      }
      mqtt_client->loop();
      vTaskDelay(pdMS_TO_TICKS(50));
    }
    vTaskDelete(NULL);
  }

}