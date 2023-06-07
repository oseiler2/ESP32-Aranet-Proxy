#include <housekeeping.h>
#include <mqtt.h>
#include <ota.h>
#include <wifiManager.h>

// Local logging tag
static const char TAG[] = __FILE__;

namespace housekeeping {
  Ticker cyclicTimer;

  void doHousekeeping() {
    ESP_LOGI(TAG, "Heap: Free:%d, Min:%d, Size:%d, Alloc:%d, StackHWM:%d",
      ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getHeapSize(),
      ESP.getMaxAllocHeap(), uxTaskGetStackHighWaterMark(NULL));
    ESP_LOGI(TAG, "MqttLoop %d bytes left | Taskstate = %d | core = %u",
      uxTaskGetStackHighWaterMark(mqtt::mqttTask), eTaskGetState(mqtt::mqttTask), xTaskGetAffinity(mqtt::mqttTask));
    ESP_LOGI(TAG, "OtaLoop %d bytes left | Taskstate = %d | core = %u",
      uxTaskGetStackHighWaterMark(OTA::otaTask), eTaskGetState(OTA::otaTask), xTaskGetAffinity(OTA::otaTask));
    ESP_LOGI(TAG, "WifiLoop %d bytes left | Taskstate = %d | core = %u",
      uxTaskGetStackHighWaterMark(WifiManager::wifiManagerTask), eTaskGetState(WifiManager::wifiManagerTask), xTaskGetAffinity(WifiManager::wifiManagerTask));
    ESP_LOGI(TAG, "AranetScannerLoop %d bytes left | Taskstate = %d | core = %u",
      uxTaskGetStackHighWaterMark(aranetScannerTask), eTaskGetState(aranetScannerTask), xTaskGetAffinity(aranetScannerTask));
    if (ESP.getMinFreeHeap() <= 2048) {
      ESP_LOGW(TAG,
        "Memory full, counter cleared (heap low water mark = %d Bytes / "
        "free heap = %d bytes)",
        ESP.getMinFreeHeap(), ESP.getFreeHeap());
      Serial.flush();
      esp_restart();
    }
  }

  uint32_t getFreeRAM() {
#ifndef BOARD_HAS_PSRAM
    return ESP.getFreeHeap();
#else
    return ESP.getFreePsram();
#endif
  }


}