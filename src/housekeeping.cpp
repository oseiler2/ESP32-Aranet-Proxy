#include <housekeeping.h>
#include <mqtt.h>
#include <ota.h>
#include <wifiManager.h>

// Local logging tag
static const char TAG[] = __FILE__;

namespace housekeeping {
  Ticker cyclicTimer;

  void doHousekeeping() {
    ESP_LOGI(TAG, "Heap: Free:%u, Min:%u, Size:%u, Alloc:%u, StackHWM:%u",
      ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getHeapSize(),
      ESP.getMaxAllocHeap(), uxTaskGetStackHighWaterMark(NULL));
    ESP_LOGI(TAG, "MqttLoop %u bytes left | Taskstate = %d | core = %u",
      uxTaskGetStackHighWaterMark(mqtt::mqttTask), eTaskGetState(mqtt::mqttTask), xTaskGetAffinity(mqtt::mqttTask));
    ESP_LOGI(TAG, "OtaLoop %u bytes left | Taskstate = %d | core = %u",
      uxTaskGetStackHighWaterMark(OTA::otaTask), eTaskGetState(OTA::otaTask), xTaskGetAffinity(OTA::otaTask));
    ESP_LOGI(TAG, "WifiLoop %u bytes left | Taskstate = %d | core = %u",
      uxTaskGetStackHighWaterMark(WifiManager::wifiManagerTask), eTaskGetState(WifiManager::wifiManagerTask), xTaskGetAffinity(WifiManager::wifiManagerTask));
    ESP_LOGI(TAG, "AranetScannerLoop %u bytes left | Taskstate = %d | core = %u",
      uxTaskGetStackHighWaterMark(aranetScannerTask), eTaskGetState(aranetScannerTask), xTaskGetAffinity(aranetScannerTask));
    if (ESP.getMinFreeHeap() <= 2048) {
      ESP_LOGW(TAG,
        "Memory full, counter cleared (heap low water mark = %u Bytes / "
        "free heap = %u bytes)",
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