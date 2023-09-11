#include <aranet-scanner.h>
#include <Aranet4.h>
#include <NimBLEScan.h>
#include <configManager.h>
#include <base64.h>
#include <mbedtls/base64.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <timekeeper.h>

// Local logging tag
static const char TAG[] = __FILE__;

#define ARANET_DEVICES_SIZE 1024

#define FILE_MUTEX_DEF_WAIT pdMS_TO_TICKS(1000)

SemaphoreHandle_t AranetScanner::fileMutex = xSemaphoreCreateMutex();

boolean AranetScanner::takeFileMutex(TickType_t blockTime) {
  //  ESP_LOGD(TAG, "%s attempting to take mutex with blockTime: %u", pcTaskGetTaskName(NULL), blockTime);
  if (AranetScanner::fileMutex == NULL) {
    ESP_LOGE(TAG, "fileMutex is NULL unsuccessful <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
    return false;
  }
  boolean result = (xSemaphoreTake(AranetScanner::fileMutex, blockTime) == pdTRUE);
  if (!result) ESP_LOGI(TAG, "%s take mutex was: %s", pcTaskGetTaskName(NULL), result ? "successful" : "unsuccessful <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
  return result;
}

void AranetScanner::giveFileMutex() {
  xSemaphoreGive(AranetScanner::fileMutex);
}

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

boolean AranetScanner::writeAranetDevices(unsigned char* json) {
  ESP_LOGD(TAG, "Saving aranet devices file...");
  if (!takeFileMutex(FILE_MUTEX_DEF_WAIT)) return false;
  // Delete existing file, otherwise the configuration is appended to the file
  if (LittleFS.exists(ARANET_DEVICES_FILENAME)) {
    LittleFS.remove(ARANET_DEVICES_FILENAME);
  }
  File f;
  if (!(f = LittleFS.open(ARANET_DEVICES_FILENAME, FILE_WRITE))) {
    ESP_LOGW(TAG, "Could not create aranet devices file for writing");
    giveFileMutex();
    return false;
  }
  int len = strlen((char*)json);
  if (f.write(json, len) != len) {
    f.close();
    giveFileMutex();
    return false;
  }
  f.close();
  giveFileMutex();
  return true;
}

const char* AranetScanner::readAranetDevices() {
  ESP_LOGD(TAG, "Reading aranet devices file...");
  if (!takeFileMutex(FILE_MUTEX_DEF_WAIT)) return "";
  File file = LittleFS.open(ARANET_DEVICES_FILENAME, FILE_READ);
  if (!file) {
    ESP_LOGW(TAG, "Could not open aranet devices file");
    giveFileMutex();
    return "";// pretend to have been successful to prevent queue from clogging up
  }
  String s = file.readString();
  file.close();
  giveFileMutex();
  return s.c_str();
}

boolean AranetScanner::loadAranetDevices() {
  ESP_LOGD(TAG, "Loading aranet devices file...");
  if (!takeFileMutex(FILE_MUTEX_DEF_WAIT)) return false;
  File file = LittleFS.open(ARANET_DEVICES_FILENAME, FILE_READ);
  if (!file) {
    ESP_LOGW(TAG, "Could not open aranet devices file");
    giveFileMutex();
    return false;
  }

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/v6/assistant to compute the capacity.
  DynamicJsonDocument doc(ARANET_DEVICES_SIZE);

  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    ESP_LOGW(TAG, "Failed to parse aranet devices file: %s", error.f_str());
    file.close();
    giveFileMutex();
    return false;
  }

  for (AranetMonitor* aranetMonitor : aranetMonitorVector) {
    delete(aranetMonitor);
  }
  this->aranetMonitorVector.clear();

  JsonObject obj = doc.as<JsonObject>();
  for (JsonPair p : obj) {
    const char* encodedAddr = p.key().c_str();  //.c_str() // is a JsonString
    JsonObject o = p.value().as<JsonObject>();

    const char* name = o["name"].as<const char*>();
    const char* code = o["code"].as<const char*>();
    if (name == nullptr || code == nullptr) continue;
    AranetMonitor* aranetMonitor = new AranetMonitor();
    aranetMonitor->address = this->base64Decode(encodedAddr);
    ESP_LOGD(TAG, "%s: %s / %s, %s", aranetMonitor->address.toString().c_str(), encodedAddr, name, code);
    strncpy(aranetMonitor->name, name, min((uint8_t)strlen(name), (uint8_t)ARANET_NAME_MAX_LEN));
    strncpy(aranetMonitor->code, code, 6);
    this->aranetMonitorVector.push_back(aranetMonitor);
  }
  file.close();
  giveFileMutex();
  ESP_LOGD(TAG, "Loaded aranet devices successfully");
  return true;
}

boolean AranetScanner::saveAranetDevices() {
  ESP_LOGD(TAG, "Saving aranet devices file...");
  if (!takeFileMutex(FILE_MUTEX_DEF_WAIT)) return false;
  // Delete existing file, otherwise the configuration is appended to the file
  if (LittleFS.exists(ARANET_DEVICES_FILENAME)) {
    LittleFS.remove(ARANET_DEVICES_FILENAME);
  }

  // Open file for writing
  File file = LittleFS.open(ARANET_DEVICES_FILENAME, FILE_WRITE);
  if (!file) {
    ESP_LOGW(TAG, "Could not create aranet devices file for writing");
    giveFileMutex();
    return false;
  }

  DynamicJsonDocument doc(ARANET_DEVICES_SIZE);

  for (AranetMonitor* aranetMonitor : aranetMonitorVector) {
    JsonObject addr = doc.createNestedObject(this->base64Encode(aranetMonitor->address));
    addr["name"] = aranetMonitor->name;
    addr["code"] = aranetMonitor->code;
  }

  // Serialize JSON to file
  if (serializeJson(doc, file) == 0) {
    ESP_LOGW(TAG, "Failed to write to file");
    file.close();
    giveFileMutex();
    return false;
  }

  // Close the file
  file.close();
  giveFileMutex();
  ESP_LOGD(TAG, "Stored aranet devices successfully");
  return true;
}

void AranetScanner::printFile() {
  // Open file for reading
  if (!takeFileMutex(FILE_MUTEX_DEF_WAIT)) return;
  File file = LittleFS.open(ARANET_DEVICES_FILENAME, FILE_READ);
  if (!file) {
    ESP_LOGW(TAG, "Could not open aranet devices file");
    giveFileMutex();
    return;
  }

  // Extract each characters by one by one
  while (file.available()) {
    Serial.print((char)file.read());
  }
  Serial.println();

  // Close the file
  giveFileMutex();
  file.close();
}

String AranetScanner::base64Encode(NimBLEAddress address) {
  String encoded = base64::encode(address.getNative(), 6);
  encoded.replace("+", "-");
  encoded.replace("/", "_");
  return encoded;
}

NimBLEAddress AranetScanner::base64Decode(String encoded) {
  encoded.replace("_", "+");
  encoded.replace("_", "/");
  ble_addr_t adr;
  adr.type = BLE_ADDR_RANDOM;
  size_t outlen;
  mbedtls_base64_decode(adr.val, 6, &outlen, (const unsigned char*)encoded.c_str(), encoded.length());
  return NimBLEAddress(adr);
}

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
  this->loadAranetDevices();
}

AranetScanner::~AranetScanner() {
  delete(this->aranet);
}

void AranetScanner::pause(boolean pauseResume) {
  this->paused = pauseResume;
}

std::vector<AranetMonitor*> AranetScanner::getAranetMonitors() {
  return this->aranetMonitorVector;
}

void AranetScanner::queryKnownMonitors() {
  if (this->paused) return;
  ESP_LOGD(TAG, "Querying known monitors...");
  bool secure = false;
  char msg[60];
  for (AranetMonitor* aranetMonitor : aranetMonitorVector) {
    ESP_LOGI(TAG, "Query %s, adr: %s...", aranetMonitor->name, aranetMonitor->address.toString().c_str());
    sprintf(msg, "Query %s", aranetMonitor->name);
    this->updateMessageCallback(msg);
    if (!takeRadioMutex(BT_MUTEX_DEF_WAIT)) {
      ESP_LOGI(TAG, "Failed taking mutex - skipping device");
      continue;
    }
    ar4_err_t retCode = this->aranet->connect(aranetMonitor->address, secure);
    if (retCode == AR4_OK) {
      AranetData data = this->aranet->getCurrentReadings();
      retCode = this->aranet->getStatus();
      if (retCode == AR4_OK) {
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
          if (Timekeeper::isSynchronised()) {
            time_t now;
            time(&now);
            (*payload)["time"] = now - data.ago;
          }

          publishMeasurement(aranetMonitor->code, payload);
        } else {
          aranetMonitor->lastData = AranetData();
          aranetMonitor->rssi = 0;
          ESP_LOGD(TAG, "No data");
        }
      } else {
        ESP_LOGE(TAG, "Aranet4 %s read failed: %s (%i)", aranetMonitor->code, aranetMonitor->address.toString().c_str(), retCode);
      }
    } else {
      ESP_LOGE(TAG, "Aranet4 %s connect failed: %s (%i)", aranetMonitor->code, aranetMonitor->address.toString().c_str(), retCode);
    }
    this->aranet->disconnect();
    giveRadioMutex();
    vTaskDelay(pdMS_TO_TICKS(200));
  }
  this->queryInterval = max(MIN_QUERY_INTERVAL, this->queryInterval);
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

  boolean foundNewAranets = false;

  NimBLEScanResults scanResults = this->bleScan->getResults();
  ESP_LOGD(TAG, "Found %u BLE devices", scanResults.getCount());
  char msg[256];
  for (uint8_t i = 0; i < scanResults.getCount(); i++) {
    NimBLEAdvertisedDevice device = scanResults.getDevice(i);
    AranetManufacturerData deviceData;
    bool isAranet = deviceData.fromAdvertisement(&device);
    ESP_LOGD(TAG, "BLE device %s, address: %s#%u Aranet? %s", device.getName().c_str(), device.getAddress().toString().c_str(), device.getAddress().getType(), isAranet ? "yes" : "no");
    if (isAranet) {
      AranetMonitor* found = nullptr;
      for (AranetMonitor* aranetMonitor : this->aranetMonitorVector) {
        if (aranetMonitor->address == device.getAddress()) {
          found = aranetMonitor;
          aranetMonitor->lastSeen = millis();
          aranetMonitor->rssi = device.getRSSI();
          aranetMonitor->lastData = deviceData.data;
          ESP_LOGI(TAG, "Aranet already known %s", device.getAddress().toString().c_str());
          break;
        }
      }
      if (deviceData.data.interval > 0) {
        this->queryInterval = min(deviceData.data.interval, this->queryInterval);
      }
      if (!found && (device.getName().length() >= 8 + 5)) {
        foundNewAranets = true;
        AranetMonitor* aranetMonitor = new AranetMonitor();
        const char* code = device.getName().c_str();
        strncpy(aranetMonitor->code, code + 8, 5);
        aranetMonitor->code[5] = 0x00;
        strncpy(aranetMonitor->name, code + 8, 5);
        aranetMonitor->name[5] = 0x00;

        ESP_LOGI(TAG, "New Aranet found %s [v%u.%u.%u RSSI: %i]", aranetMonitor->code, deviceData.version.major, deviceData.version.minor, deviceData.version.patch, device.getRSSI());
        if (deviceData.data.co2 > 0) {
          //          ESP_LOGD(TAG, "%i ppm, %.2f C, %.1f hPa, %i %%rH, %i %%bat, int: %is, %is ago", deviceData.data.co2, deviceData.data.temperature / 20.0, deviceData.data.pressure / 10.0, deviceData.data.humidity, deviceData.data.battery, deviceData.data.interval, deviceData.data.ago);
          sprintf(msg, "Found Aranet %s at %s v%u.%u.%u", aranetMonitor->code, this->base64Encode(device.getAddress()).c_str(), deviceData.version.major, deviceData.version.minor, deviceData.version.patch);
          this->publishMessageCallback(msg);
        } else {
          ESP_LOGI(TAG, "Found Aranet %s but cannot access data - please enable 'Smart Home integration' on Aranet monitor", code);
          sprintf(msg, "Cannot access data for Aranet %s - enable 'Smart Home integration'", code);
          this->publishMessageCallback(msg);
        }
        aranetMonitor->address = device.getAddress();
        aranetMonitor->lastData = deviceData.data;
        aranetMonitor->rssi = device.getRSSI();
        aranetMonitor->lastSeen = millis();
        this->aranetMonitorVector.push_back(aranetMonitor);
      }
    }
  }
  this->queryInterval = max(MIN_QUERY_INTERVAL, this->queryInterval);
  if (foundNewAranets) {
    this->saveAranetDevices();
    this->printFile();
  }

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
