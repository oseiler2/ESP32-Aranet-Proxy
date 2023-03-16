#include "globals.h"
#include <wifiManager.h>
#include <config.h>

#include <ESPAsync_WiFiManager.h>
#include <ESPAsync_WiFiManager-Impl.h>
#include <base64.h>
#include <configManager.h>
#include <lcd.h>

#ifndef AP_PW
#define AP_PW ""
#endif

// Local logging tag
static const char TAG[] = __FILE__;

namespace WifiManager {
#define HTTP_PORT 80

  volatile static boolean safeConfigFlag = false;

  String getMac() {
    uint8_t rawMac[6];
    for (uint8_t i = 0;i < 6;i++) {
      rawMac[i] = (uint8_t)(ESP.getEfuseMac() >> (6 - i - 1) * 8 & 0x000000ffUL);
    }
    String encoded = base64::encode(rawMac, 6);
    encoded.replace("+", "-");
    encoded.replace("/", "_");
    return encoded;
  }

  String getSSID() {
    return ("Aranet-proxy-" + getMac());
  }

  void saveConfigCallback() {
    ESP_LOGD(TAG, "saveConfigCallback");
    safeConfigFlag = true;
  }

  ESPAsync_WMParameter* deviceIdParam;
  ESPAsync_WMParameter* mqttTopicParam;
  ESPAsync_WMParameter* mqttUsernameParam;
  ESPAsync_WMParameter* mqttPasswordParam;
  ESPAsync_WMParameter* mqttHostParam;
  ESPAsync_WMParameter* mqttPortParam;
  ESPAsync_WMParameter* mqttUseTlsParam;
  ESPAsync_WMParameter* mqttInsecureParam;
  ESPAsync_WMParameter* ssd1306RowsParam;
  ESPAsync_WMParameter* scanIntervalParam;

  void setupWifiManager(ESPAsync_WiFiManager* wifiManager) {
    safeConfigFlag = false;
    wifiManager->setConfigPortalTimeout(300);

    WiFi_AP_IPConfig  portalIPconfig;
    portalIPconfig._ap_static_gw = IPAddress(192, 168, 100, 1);
    portalIPconfig._ap_static_ip = IPAddress(192, 168, 100, 1);
    portalIPconfig._ap_static_sn = IPAddress(255, 255, 255, 0);
    wifiManager->setAPStaticIPConfig(portalIPconfig);

    char deviceId[6];
    char mqttTopic[MQTT_TOPIC_ID_LEN + 1];
    char mqttUsername[MQTT_USERNAME_LEN + 1];
    char mqttPassword[MQTT_PASSWORD_LEN + 1];
    char mqttHost[MQTT_HOSTNAME_LEN + 1];
    char mqttPort[6];
    char mqttUseTls[6];
    char mqttInsecure[6];
    char ssd1306Rows[3];
    char scanInterval[6];

    sprintf(deviceId, "%u", config.deviceId);
    sprintf(mqttTopic, "%s", config.mqttTopic);
    sprintf(mqttUsername, "%s", config.mqttUsername);
    sprintf(mqttPassword, "%s", config.mqttPassword);
    sprintf(mqttHost, "%s", config.mqttHost);
    sprintf(mqttPort, "%u", config.mqttServerPort);
    sprintf(mqttUseTls, "%s", config.mqttUseTls ? "true" : "false");
    sprintf(mqttInsecure, "%s", config.mqttInsecure ? "true" : "false");
    sprintf(ssd1306Rows, "%u", config.ssd1306Rows);
    sprintf(scanInterval, "%u", config.scanInterval);

    ESP_LOGD(TAG, "deviceId: %s", deviceId);
    ESP_LOGD(TAG, "mqttTopic: %s", mqttTopic);
    ESP_LOGD(TAG, "mqttUsername: %s", mqttUsername);
    ESP_LOGD(TAG, "mqttPassword: %s", mqttPassword);
    ESP_LOGD(TAG, "mqttHost: %s", mqttHost);
    ESP_LOGD(TAG, "mqttPort: %s", mqttPort);
    ESP_LOGD(TAG, "mqttUseTls: %s", mqttUseTls);
    ESP_LOGD(TAG, "mqttInsecure: %s", mqttInsecure);
    ESP_LOGD(TAG, "ssd1306Rows: %s", ssd1306Rows);
    ESP_LOGD(TAG, "scanInterval: %s", scanInterval);

    deviceIdParam = new ESPAsync_WMParameter("deviceId", "Device ID", deviceId, 6, "config.deviceId");
    mqttTopicParam = new ESPAsync_WMParameter("mqttTopic", "MQTT topic", mqttTopic, MQTT_TOPIC_ID_LEN + 1, config.mqttTopic);
    mqttUsernameParam = new ESPAsync_WMParameter("mqttUsername", "MQTT username", mqttUsername, MQTT_USERNAME_LEN + 1, config.mqttUsername);
    mqttPasswordParam = new ESPAsync_WMParameter("mqttPassword", "MQTT password", mqttPassword, MQTT_PASSWORD_LEN + 1, config.mqttPassword);
    mqttHostParam = new ESPAsync_WMParameter("mqttHost", "MQTT host", mqttHost, MQTT_HOSTNAME_LEN + 1, config.mqttHost);
    mqttPortParam = new ESPAsync_WMParameter("mqttServerPort", "MQTT port", mqttPort, 6, "config.mqttServerPort");
    mqttUseTlsParam = new ESPAsync_WMParameter("mqttUseTls", "MQTT use TLS", mqttUseTls, 6, "config.mqttUseTls");
    mqttInsecureParam = new ESPAsync_WMParameter("mqttInsecure", "MQTT Ignore certificate errors", mqttInsecure, 6, "config.mqttInsecure");
    ssd1306RowsParam = new ESPAsync_WMParameter("ssd1306Rows", "SSD1306 rows", ssd1306Rows, 4, "config.ssd1306Rows");
    scanIntervalParam = new ESPAsync_WMParameter("scanInterval", "Scan interval (sec)", scanInterval, 6, "config.scaninterval");

    wifiManager->addParameter(deviceIdParam);
    wifiManager->addParameter(mqttTopicParam);
    wifiManager->addParameter(mqttUsernameParam);
    wifiManager->addParameter(mqttPasswordParam);
    wifiManager->addParameter(mqttHostParam);
    wifiManager->addParameter(mqttPortParam);
    wifiManager->addParameter(mqttUseTlsParam);
    wifiManager->addParameter(mqttInsecureParam);
    wifiManager->addParameter(ssd1306RowsParam);
    wifiManager->addParameter(scanIntervalParam);
    wifiManager->setSaveConfigCallback(saveConfigCallback);

    ESP_LOGD(TAG, "SSID: %s", getSSID().c_str());
  }

  void updateConfiguration(ESPAsync_WiFiManager* wifiManager) {
    if (safeConfigFlag) {
      ESP_LOGD(TAG, "deviceId: %s", deviceIdParam->getValue());
      ESP_LOGD(TAG, "mqttTopic: %s", mqttTopicParam->getValue());
      ESP_LOGD(TAG, "mqttUsername: %s", mqttUsernameParam->getValue());
      ESP_LOGD(TAG, "mqttPassword: %s", mqttPasswordParam->getValue());
      ESP_LOGD(TAG, "mqttHost: %s", mqttHostParam->getValue());
      ESP_LOGD(TAG, "mqttPort: %s", mqttPortParam->getValue());
      ESP_LOGD(TAG, "mqttUseTls: %s", mqttUseTlsParam->getValue());
      ESP_LOGD(TAG, "mqttInsecure: %s", mqttInsecureParam->getValue());
      ESP_LOGD(TAG, "ssd1306Rows: %s", ssd1306RowsParam->getValue());
      ESP_LOGD(TAG, "scanIntervalParam: %s", scanIntervalParam->getValue());

      config.deviceId = (uint16_t)atoi(deviceIdParam->getValue());
      strncpy(config.mqttTopic, mqttTopicParam->getValue(), MQTT_TOPIC_ID_LEN);
      strncpy(config.mqttUsername, mqttUsernameParam->getValue(), MQTT_USERNAME_LEN);
      strncpy(config.mqttPassword, mqttPasswordParam->getValue(), MQTT_PASSWORD_LEN);
      strncpy(config.mqttHost, mqttHostParam->getValue(), MQTT_HOSTNAME_LEN);
      config.mqttServerPort = (uint16_t)atoi(mqttPortParam->getValue());
      config.mqttUseTls = strcmp("true", mqttUseTlsParam->getValue()) == 0;
      config.mqttInsecure = strcmp("true", mqttInsecureParam->getValue()) == 0;
      config.ssd1306Rows = (uint8_t)atoi(ssd1306RowsParam->getValue());
      config.scanInterval = (uint16_t)atoi(scanIntervalParam->getValue());

      saveConfiguration(config);
      delay(1000);
      esp_restart();
    }
    delete deviceIdParam;
    delete mqttTopicParam;
    delete mqttUsernameParam;
    delete mqttPasswordParam;
    delete mqttHostParam;
    delete mqttPortParam;
    delete mqttUseTlsParam;
    delete mqttInsecureParam;
    delete ssd1306RowsParam;
    delete scanIntervalParam;
  }

  void setupWifi(setPriorityMessageCallback_t setPriorityMessageCallback, clearPriorityMessageCallback_t clearPriorityMessageCallback) {
    if (WiFi.status() != WL_CONNECTED) {
      ESP_LOGD(TAG, "Could not connect to Wifi using known settings");
      AsyncWebServer webServer(HTTP_PORT);
      DNSServer dnsServer;
      ESPAsync_WiFiManager* wifiManager;
      wifiManager = new ESPAsync_WiFiManager(&webServer, &dnsServer, "Aranet Proxy");
      setupWifiManager(wifiManager);
      setPriorityMessageCallback(getSSID().c_str());
      wifiManager->autoConnect(getSSID().c_str(), AP_PW);
      updateConfiguration(wifiManager);
      delete wifiManager;
      clearPriorityMessageCallback();
    }
    ESP_LOGD(TAG, "setupWifi end");
  }

  void startConfigPortal(updateMessageCallback_t updateMessageCallback, setPriorityMessageCallback_t setPriorityMessageCallback, clearPriorityMessageCallback_t clearPriorityMessageCallback) {
    setPriorityMessageCallback(getSSID().c_str());
    AsyncWebServer webServer(HTTP_PORT);
    DNSServer dnsServer;
    ESPAsync_WiFiManager* wifiManager;
    wifiManager = new ESPAsync_WiFiManager(&webServer, &dnsServer, "Aranet Proxy");
    setupWifiManager(wifiManager);
    wifiManager->startConfigPortal(getSSID().c_str(), AP_PW);
    updateConfiguration(wifiManager);
    delete wifiManager;
    clearPriorityMessageCallback();
    ESP_LOGD(TAG, "startConfigPortal end");
  }

  void resetSettings() {
    //    ESPAsync_WiFiManager::resetSettings();
    WiFi.disconnect(true, true);
    WiFi.begin("0", "0");
  }

}