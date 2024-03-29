#include <logging.h>
#include <globals.h>
#include <Arduino.h>
#include <config.h>

#include <WiFi.h>
#include <i2c.h>
#include <esp_event.h>
#include <esp_err.h>
#include <esp_task_wdt.h>
#include <rom/rtc.h>

#include <configManager.h>
#include <mqtt.h>
#include <aranet-scanner.h>
#include <housekeeping.h>
#include <lcd.h>
#include <FS.h>
#include <LittleFS.h>
#include <wifiManager.h>
#include <ota.h>
#include <timekeeper.h>

// Local logging tag
static const char TAG[] = __FILE__;

LCD* lcd;
AranetScanner* aranetScanner;

TaskHandle_t aranetScannerTask;
TaskHandle_t wifiManagerTask;

uint8_t timerMonitorIdx;
Ticker* displayTicker = new Ticker();

const uint32_t debounceDelay = 50;
volatile uint32_t lastBtnDebounceTime = 0;
volatile uint8_t buttonState = 0;
uint8_t oldConfirmedButtonState = 0;
uint32_t lastConfirmedBtnPressedTime = 0;

void ICACHE_RAM_ATTR buttonHandler() {
  buttonState = (digitalRead(BTN_1) ? 0 : 1);
  lastBtnDebounceTime = millis();
}

void prepareOta() {
  aranetScanner->pause(true);
}

void updateMessage(char const* msg) {
  if (lcd) {
    lcd->updateMessage(msg);
  }
}

void setPriorityMessage(char const* msg) {
  if (lcd) {
    lcd->setPriorityMessage(msg);
  }
}

void clearPriorityMessage() {
  if (lcd) {
    lcd->clearPriorityMessage();
  }
}

void publishMeasurement(const char* name, DynamicJsonDocument* payload) {
  mqtt::publishMeasurement(name, payload);
}

void publishMessage(char const* msg) {
  mqtt::publishStatusMsg(msg);
}

void configChanged() {}

void logCoreInfo() {
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  ESP_LOGI(TAG,
    "This is ESP32 chip with %d CPU cores, WiFi%s%s, silicon revision "
    "%d, %dMB %s Flash",
    chip_info.cores,
    (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
    (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "",
    chip_info.revision, spi_flash_get_chip_size() / (1024 * 1024),
    (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded"
    : "external");
  ESP_LOGI(TAG, "Internal Total heap %d, internal Free Heap %d",
    ESP.getHeapSize(), ESP.getFreeHeap());
#ifdef BOARD_HAS_PSRAM
  ESP_LOGI(TAG, "SPIRam Total heap %d, SPIRam Free Heap %d",
    ESP.getPsramSize(), ESP.getFreePsram());
#endif
  ESP_LOGI(TAG, "ChipRevision %d, Cpu Freq %d, SDK Version %s",
    ESP.getChipRevision(), ESP.getCpuFreqMHz(), ESP.getSdkVersion());
  ESP_LOGI(TAG, "Flash Size %d, Flash Speed %d", ESP.getFlashChipSize(),
    ESP.getFlashChipSpeed());
}


void displayTimer() {
  if (!aranetScanner) return;
  if (aranetScanner->getAranetMonitors().size() == 0) {
    return;
  }
  timerMonitorIdx++;
  if (timerMonitorIdx < 0 || timerMonitorIdx > aranetScanner->getAranetMonitors().size() - 1) {
    timerMonitorIdx = 0;
  }
  AranetMonitor* mon = aranetScanner->getAranetMonitors().at(timerMonitorIdx);
  if (lcd) {
    lcd->update(mon);
  }
}

static SemaphoreHandle_t radioMutex = xSemaphoreCreateMutex();

boolean takeRadioMutex(TickType_t blockTime) {
  //  ESP_LOGD(TAG, "%s attempting to take mutex with blockTime: %u", pcTaskGetTaskName(NULL), blockTime);
  if (radioMutex == NULL) {
    ESP_LOGE(TAG, "radioMutex is NULL unsuccessful <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
    return false;
  }
  boolean result = (xSemaphoreTake(radioMutex, blockTime) == pdTRUE);
  if (!result) ESP_LOGI(TAG, "%s take mutex was: %s", pcTaskGetTaskName(NULL), result ? "successful" : "unsuccessful <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
  return result;
}

void giveRadioMutex() {
  xSemaphoreGive(radioMutex);
}

boolean writeAranetDevices(unsigned char* json) {
  if (aranetScanner) return aranetScanner->writeAranetDevices(json);
  return false;
}
const char* readAranetDevices(void) {
  if (aranetScanner) return aranetScanner->readAranetDevices();
  return "";
}

void setup() {
  esp_task_wdt_init(20, true);

  if (LED_PIN >= 0) {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
  }
  pinMode(BTN_1, INPUT_PULLUP);
  Serial.begin(115200);
  esp_log_set_vprintf(logging::logger);
  esp_log_level_set("*", ESP_LOG_VERBOSE);
  ESP_LOGI(TAG, "Aranet Proxy v%s. Built from %s @ %s", APP_VERSION, SRC_REVISION, BUILD_TIMESTAMP);

  logCoreInfo();

  RESET_REASON resetReason = rtc_get_reset_reason(0);

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, WifiManager::eventHandler, NULL, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, WifiManager::eventHandler, NULL, NULL));

  setupConfigManager();
  if (!loadConfiguration(config)) {
    getDefaultConfiguration(config);
    saveConfiguration(config);
  }
  logConfiguration(config);

  Timekeeper::init();

  WifiManager::setupWifiManager("Aranet-proxy", getConfigParameters(), false, true,
    updateMessage, setPriorityMessage, clearPriorityMessage, configChanged);

  Wire.begin((int)SDA_PIN, (int)SCL_PIN, (uint32_t)I2C_CLK);

  I2C::initI2C();

  if (I2C::lcdPresent()) lcd = new LCD(&Wire);

  mqtt::setupMqtt("AranetProxy", readAranetDevices, writeAranetDevices, configChanged);

  char msg[128];
  sprintf(msg, "Reset reason: %u", resetReason);
  mqtt::publishStatusMsg(msg);

  aranetScanner = new AranetScanner(updateMessage, publishMeasurement, publishMessage);

  xTaskCreatePinnedToCore(mqtt::mqttLoop,  // task function
    "mqttLoop",         // name of task
    6144,               // stack size of task
    (void*)1,           // parameter of the task
    2,                  // priority of the task
    &mqtt::mqttTask,    // task handle
    0);                 // CPU core

  xTaskCreatePinnedToCore(OTA::otaLoop,  // task function
    "otaLoop",          // name of task
    8192,               // stack size of task
    (void*)1,           // parameter of the task
    2,                  // priority of the task
    &OTA::otaTask,      // task handle
    1);                 // CPU core

  wifiManagerTask = WifiManager::start(
    "wifiManagerLoop",  // name of task
    3072,               // stack size of task
    2,                  // priority of the task
    1);                 // CPU core

  aranetScannerTask = aranetScanner->start(
    "aranetScannerLoop",
    8192,
    3,
    1);

  housekeeping::cyclicTimer.attach(30, housekeeping::doHousekeeping);

  OTA::setupOta(prepareOta, setPriorityMessage, clearPriorityMessage);

  attachInterrupt(BTN_1, buttonHandler, CHANGE);

  displayTicker->attach(3, displayTimer);

  ESP_LOGI(TAG, "Setup done.");
#ifdef SHOW_DEBUG_MSGS
  if (lcd) {
    lcd->updateMessage("Setup done.");
  }
#endif
}

void loop() {
  if (buttonState != oldConfirmedButtonState && (millis() - lastBtnDebounceTime) > debounceDelay) {
    oldConfirmedButtonState = buttonState;
    if (oldConfirmedButtonState == 1) {
      lastConfirmedBtnPressedTime = millis();
    } else if (oldConfirmedButtonState == 0) {
      uint32_t btnPressTime = millis() - lastConfirmedBtnPressedTime;
      ESP_LOGD(TAG, "lastConfirmedBtnPressedTime - millis() %u", btnPressTime);
      if (btnPressTime < 2000) {
        if (LED_PIN >= 0) digitalWrite(LED_PIN, LOW);
        prepareOta();
        WifiManager::startCaptivePortal();
      } else if (btnPressTime > 5000) {
      }
    }
  }
  vTaskDelay(pdMS_TO_TICKS(50));
}