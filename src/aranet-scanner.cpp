#include <aranet-scanner.h>
#include <Aranet4.h>
#include <NimBLEScan.h>
#include <configManager.h>

// Local logging tag
static const char TAG[] = __FILE__;

uint32_t AranetScanner::onPinRequested() {
  ESP_LOGI(TAG, "PIN Requested. Enter PIN in serial console.");
  /*
  while (Serial.available() == 0)
    vTaskDelay(500 / portTICK_PERIOD_MS);
  return  Serial.readString().toInt();
  */
}

void AranetScanner::onConnect(NimBLEClient* pClient) {
  this->lastRssi = pClient->getRssi();
}

const uint16_t MAX_QUERY_INTERVAL = 600;
const uint16_t MIN_QUERY_INTERVAL = 60;

AranetScanner::AranetScanner(updateMessageCallback_t _updateMessageCallback, publishMeasurement_t _publishMeasurement, publishMessageCallback_t _publishMessageCallback) {
  this->updateMessageCallback = _updateMessageCallback;
  this->publishMeasurement = _publishMeasurement;
  this->publishMessageCallback = _publishMessageCallback;
  this->bleScan = NimBLEDevice::getScan();
  this->bleScan->setActiveScan(true);
  this->bleScan->setDuplicateFilter(true);
  this->queryInterval = MAX_QUERY_INTERVAL;

  //TODO: For best 
  // CONFIG_BT_CTRL_PINNED_TO_CORE in esp_bt.h BT_CONTROLLER_INIT_CONFIG_DEFAULT()
  // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/coexist.html#setting-coexistence-compile-time-options

  Aranet4::init();
  this->aranet = new Aranet4(this);
  ESP_ERROR_CHECK(esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9));
  ESP_ERROR_CHECK(esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9));
  ESP_ERROR_CHECK(esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN, ESP_PWR_LVL_P9));
}

AranetScanner::~AranetScanner() {}

void AranetScanner::pause(boolean pauseResume) {
  this->paused = pauseResume;
}

std::vector<AranetMonitor*> AranetScanner::getAranetMonitors() {
  return this->aranetMonitorVector;
}

void AranetScanner::queryKnownMonitors() {
  if (this->paused) return;
  ESP_LOGD(TAG, "Querying known monitors...");
  if (!takeRadioMutex(BT_MUTEX_DEF_WAIT)) {
    ESP_LOGI(TAG, "Failed taking mutex - skipping query");
    return;
  }
  bool secure = false;
  char msg[40];
  for (auto& aranetMonitor : aranetMonitorVector) {
    sprintf(msg, "Query %s...", aranetMonitor->name);
    ESP_LOGI(TAG, "%s", msg);
    this->updateMessageCallback(msg);

    ar4_err_t retCode = this->aranet->connect(aranetMonitor->address, secure);
    if (retCode == AR4_OK) {
      AranetData data = this->aranet->getCurrentReadings();
      retCode = this->aranet->getStatus();
      if (retCode == AR4_OK) {
        this->updateMessageCallback(aranetMonitor->name);
        aranetMonitor->lastData = data;
        aranetMonitor->rssi = lastRssi;
        aranetMonitor->lastSeen = millis();
        if (data.interval > 0) {
          this->queryInterval = min(data.interval, this->queryInterval);
        }
        if (data.co2 > 0) {
          ESP_LOGD(TAG, "%i ppm, %.2f C, %.1f hPa, %i %%rH, %i %%bat, int: %is, %is ago, rssi: %i", data.co2, data.temperature / 20.0, data.pressure / 10.0, data.humidity, data.battery, data.interval, data.ago, this->lastRssi);
          DynamicJsonDocument* payload = new DynamicJsonDocument(512);
          char buf[8];
          (*payload)["co2"] = data.co2;
          sprintf(buf, "%.1f", data.temperature / 20.0);
          (*payload)["temperature"] = buf;
          (*payload)["pressure"] = data.pressure / 10;
          (*payload)["humidity"] = data.humidity;
          (*payload)["battery"] = data.battery;
          (*payload)["rssi"] = this->lastRssi;

          publishMeasurement(aranetMonitor->name, payload);
        } else {
          aranetMonitor->lastData = AranetData();
          aranetMonitor->rssi = 0;
          ESP_LOGD(TAG, "No data");
        }
      } else {
        ESP_LOGE(TAG, "Aranet4 %s read failed: %s (%i)", aranetMonitor->name, aranetMonitor->address.toString().c_str(), retCode);
      }
    } else {
      ESP_LOGE(TAG, "Aranet4 %s connect failed: %s (%i)", aranetMonitor->name, aranetMonitor->address.toString().c_str(), retCode);
    }
    this->aranet->disconnect();
    vTaskDelay(pdMS_TO_TICKS(200));
  }
  this->queryInterval = max(MIN_QUERY_INTERVAL, this->queryInterval);
  giveRadioMutex();
  this->updateMessageCallback("");
}

void AranetScanner::scan() {
  if (this->paused) return;
  ESP_LOGD(TAG, "Scanning BLE devices...");
  this->updateMessageCallback("BLE scan...");
  if (!takeRadioMutex(BT_MUTEX_DEF_WAIT)) {
    ESP_LOGI(TAG, "Failed taking mutex - skipping scan");
    return;
  }

  this->bleScan->start(10);

  NimBLEScanResults scanResults = this->bleScan->getResults();
  ESP_LOGD(TAG, "Found %u BLE devices", scanResults.getCount());
  char msg[256];
  for (uint8_t i = 0; i < scanResults.getCount(); i++) {
    NimBLEAdvertisedDevice device = scanResults.getDevice(i);
    AranetManufacturerData deviceData;
    bool isAranet = deviceData.fromAdvertisement(&device);
    ESP_LOGD(TAG, "BLE device %s, Aranet? %s", device.getName().c_str(), isAranet ? "yes" : "no");
    if (isAranet) {
      AranetMonitor* found = nullptr;
      for (auto& aranetMonitor : this->aranetMonitorVector) {
        if (aranetMonitor->address == device.getAddress()) {
          found = aranetMonitor;
          aranetMonitor->lastSeen = millis();
          aranetMonitor->rssi = device.getRSSI();
          aranetMonitor->lastData = deviceData.data;
          ESP_LOGI(TAG, "Aranet already known %s", device.getAddress().toString().c_str());
          break;
        }
      }
      if (!found) {
        AranetMonitor* aranetMonitor = new AranetMonitor();
        const char* name = device.getName().c_str();
        if (strncmp(name, "Aranet4 ", 8) == 0) {
          strncpy(aranetMonitor->name, name + 8, sizeof(aranetMonitor->name));
        } else {
          strncpy(aranetMonitor->name, name, sizeof(aranetMonitor->name));
        }
        aranetMonitor->name[sizeof(aranetMonitor->name) - 1] = 0x00;

        ESP_LOGI(TAG, "New Aranet found %s", name);
        ESP_LOGD(TAG, "%s v%u.%u.%u RSSI: %i", name, deviceData.version.major, deviceData.version.minor, deviceData.version.patch, device.getRSSI());
        if (deviceData.data.co2 > 0) {
          //          ESP_LOGD(TAG, "%i ppm, %.2f C, %.1f hPa, %i %%rH, %i %%bat, int: %is, %is ago", deviceData.data.co2, deviceData.data.temperature / 20.0, deviceData.data.pressure / 10.0, deviceData.data.humidity, deviceData.data.battery, deviceData.data.interval, deviceData.data.ago);
          sprintf(msg, "Found Aranet %s v%u.%u.%u", name, deviceData.version.major, deviceData.version.minor, deviceData.version.patch);
          this->publishMessageCallback(msg);
        } else {
          ESP_LOGI(TAG, "Found Aranet %s but cannot access data - please enable 'Smart Home integration' on Aranet monitor", name);
          sprintf(msg, "Cannot access data for Aranet %s - enable 'Smart Home integration'", name);
          this->publishMessageCallback(msg);
        }
        aranetMonitor->address = device.getAddress();
        aranetMonitor->lastData = deviceData.data;
        aranetMonitor->rssi = device.getRSSI();
        aranetMonitor->lastSeen = millis();
        this->aranetMonitorVector.push_back(aranetMonitor);
        if (deviceData.data.interval > 0) {
          this->queryInterval = min(deviceData.data.interval, this->queryInterval);
        }
      }
    }
  }
  this->queryInterval = max(MIN_QUERY_INTERVAL, this->queryInterval);
  ESP_LOGD(TAG, "Setting query interval to %u seconds", this->queryInterval);
  giveRadioMutex();
  this->updateMessageCallback("");
}

TaskHandle_t AranetScanner::start(const char* name, uint32_t stackSize, UBaseType_t priority, BaseType_t core) {
  xTaskCreatePinnedToCore(
    this->aranetScanLoop, // task function
    name,                 // name of task
    stackSize,            // stack size of task
    this,                 // parameter of the task
    priority,             // priority of the task
    &task,                // task handle
    core);                // CPU core
  return this->task;
}

void AranetScanner::aranetScanLoop(void* pvParameters) {
  AranetScanner* instance = (AranetScanner*)pvParameters;
  BaseType_t notified;

  uint32_t lastQuery = millis();
  uint32_t lastScan = millis();

  // initial scan
  instance->scan();

  ESP_LOGD(TAG, "AranetScanner initialised");

  char msg[10];
  while (1) {
    if (millis() - lastScan >= (config.scanInterval * 1000)) {
      lastScan += config.scanInterval * 1000;
      instance->scan();
    }
    if (millis() - lastQuery >= (instance->queryInterval * 1000)) {
      lastQuery += instance->queryInterval * 1000;
      instance->queryKnownMonitors();
    }
    sprintf(msg, "%u", (lastQuery + (instance->queryInterval * 1000) - millis()) / 1000);
    instance->updateMessageCallback(msg);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  vTaskDelete(NULL);
}
