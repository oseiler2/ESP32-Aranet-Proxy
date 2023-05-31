#include <globals.h>
#include <wifiManager.h>
#include <config.h>

#include <ESPAsync_WiFiManager.h>
#include <ESPAsync_WiFiManager-Impl.h>
#include <base64.h>
#include <configManager.h>

#include <configParameter.h>

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

  template <typename T>
  std::pair<ConfigParameterBase*, ESPAsync_WMParameter*>* toParameterPair(ConfigParameter<T>* configParameter) {
    char defaultValue[configParameter->getMaxStrLen()];
    configParameter->print(defaultValue);
    ESPAsync_WMParameter* espAsyncParam = new ESPAsync_WMParameter(configParameter->getId(), configParameter->getLabel(), defaultValue, configParameter->getMaxStrLen());
    ESP_LOGD(TAG, "%s: %s", configParameter->getId(), defaultValue);
    return new std::pair<ConfigParameterBase*, ESPAsync_WMParameter*>(configParameter, espAsyncParam);
  }

  std::vector<std::pair<ConfigParameterBase*, ESPAsync_WMParameter*>*> configParameterVector;

  void setupWifiManager(ESPAsync_WiFiManager* wifiManager) {
    safeConfigFlag = false;
    wifiManager->setConfigPortalTimeout(300);

    WiFi_AP_IPConfig  portalIPconfig;
    portalIPconfig._ap_static_gw = IPAddress(192, 168, 100, 1);
    portalIPconfig._ap_static_ip = IPAddress(192, 168, 100, 1);
    portalIPconfig._ap_static_sn = IPAddress(255, 255, 255, 0);
    wifiManager->setAPStaticIPConfig(portalIPconfig);

    configParameterVector.push_back(toParameterPair(new Uint16ConfigParameter("deviceId", "Device ID", &(config.*deviceIdPtr))));
    configParameterVector.push_back(toParameterPair(new CharArrayConfigParameter("mqttTopic", "MQTT topic", &(config.*mqttTopicPtr), MQTT_TOPIC_ID_LEN + 1)));
    configParameterVector.push_back(toParameterPair(new CharArrayConfigParameter("mqttUsername", "MQTT username", &(config.*mqttUsernamePtr), MQTT_USERNAME_LEN + 1)));
    configParameterVector.push_back(toParameterPair(new CharArrayConfigParameter("mqttPassword", "MQTT password", &(config.*mqttPasswordPtr), MQTT_PASSWORD_LEN + 1)));
    configParameterVector.push_back(toParameterPair(new CharArrayConfigParameter("mqttHost", "MQTT host", &(config.*mqttHostPtr), MQTT_HOSTNAME_LEN + 1)));
    configParameterVector.push_back(toParameterPair(new Uint16ConfigParameter("mqttServerPort", "MQTT port", &(config.*mqttServerPortPtr))));
    configParameterVector.push_back(toParameterPair(new BooleanConfigParameter("mqttUseTls", "MQTT use TLS", &(config.*mqttUseTlsPtr))));
    configParameterVector.push_back(toParameterPair(new BooleanConfigParameter("mqttInsecure", "MQTT Ignore certificate errors", &(config.*mqttInsecurePtr))));
    configParameterVector.push_back(toParameterPair(new Uint8ConfigParameter("ssd1306Rows", "SSD1306 rows", &(config.*ssd1306RowsPtr))));
    configParameterVector.push_back(toParameterPair(new Uint16ConfigParameter("scanInterval", "Scan interval (sec)", &(config.*scanIntervalPtr))));

    for (std::pair<ConfigParameterBase*, ESPAsync_WMParameter*>* configParameterPair : configParameterVector) {
      wifiManager->addParameter(configParameterPair->second);
    }
    wifiManager->setSaveConfigCallback(saveConfigCallback);

    ESP_LOGD(TAG, "SSID: %s", getSSID().c_str());
  }

  void updateConfiguration(ESPAsync_WiFiManager* wifiManager) {
    if (safeConfigFlag) {
      char msg[128];
      for (std::pair<ConfigParameterBase*, ESPAsync_WMParameter*>* configParameterPair : configParameterVector) {
        configParameterPair->first->print(msg);
        ESP_LOGD(TAG, "%s: %s", configParameterPair->first->getId(), msg);

        configParameterPair->first->save(configParameterPair->second->getValue());
      }
      saveConfiguration(config);
      delay(1000);
      esp_restart();
    }
    for (std::pair<ConfigParameterBase*, ESPAsync_WMParameter*>* configParameterPair : configParameterVector) {
      free(configParameterPair->first);
      free(configParameterPair->second);
      free(configParameterPair);
    }
    configParameterVector.clear();
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