#include <Arduino.h>
#include <i2c.h>
#include <Wire.h>
#include <config.h>

// Local logging tag
static const char TAG[] = __FILE__;

namespace I2C {
  boolean lcdDetected = false;

  boolean lcdPresent() {
    return lcdDetected;
  }

  static SemaphoreHandle_t i2cMutex = xSemaphoreCreateMutex();

  boolean takeMutex(TickType_t blockTime) {
    //  ESP_LOGD(TAG, "%s attempting to take mutex with blockTime: %u", pcTaskGetTaskName(NULL), blockTime);
    if (i2cMutex == NULL) {
      ESP_LOGD(TAG, "i2cMutex is NULL unsuccessful <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
      return false;
    }
    boolean result = (xSemaphoreTake(i2cMutex, blockTime) == pdTRUE);
    if (!result) ESP_LOGD(TAG, "%s take mutex was: %s", pcTaskGetTaskName(NULL), result ? "successful" : "unsuccessful <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
    return result;
  }

  void giveMutex() {
    xSemaphoreGive(i2cMutex);
  }

  void initI2C() {
    if (i2cMutex == NULL) {
      ESP_LOGE(TAG, "Could not create I2C Mutex");
      delay(1000);
      esp_restart();
    }
    if (!takeMutex(portMAX_DELAY)) {
      return;
    }
    byte err, addr;
    uint8_t nDevices = 0;
    for (addr = 1; addr < 127; addr++) {
      Wire.beginTransmission(addr);
      err = Wire.endTransmission();
      if (err == 0) {
        nDevices++;
        if (addr == SSD1306_I2C_ADR) {
          lcdDetected = true;
          ESP_LOGD(TAG, "SSD1306 display found");
        } else {
          ESP_LOGI(TAG, "I2C device found at address %x !", addr);
        }
      } else if (err == 4) {
        ESP_LOGW(TAG, "Unknow error at address %x !", addr);
      }
    }
    if (nDevices == 0)
      ESP_LOGD(TAG, "No I2C devices found");
    Wire.setClock(I2C_CLK);
    giveMutex();
  }

}