#ifndef _ARANET_SCANNER_H
#define _ARANET_SCANNER_H

#include <Arduino.h>
#include <globals.h>
#include <config.h>
#include <messageSupport.h>
#include <NimBLEScan.h>
#include <ArduinoJson.h>
#include <Aranet4.h>
#include <Ticker.h>

#define BT_MUTEX_DEF_WAIT pdMS_TO_TICKS(15000)

extern boolean takeRadioMutex(TickType_t blockTime);
extern void giveRadioMutex();

struct AranetMonitor {
  NimBLEAddress address;
  AranetData lastData;
  int rssi;
  uint32_t lastSeen;
  char name[15];
};

class AranetScanner: public Aranet4Callbacks {
public:
  AranetScanner(updateMessageCallback_t _updateMessageCallback, publishMeasurement_t publishMeasurement, publishMessageCallback_t publishMessageCallback);
  ~AranetScanner();
  TaskHandle_t start(const char* name, uint32_t stackSize, UBaseType_t priority, BaseType_t core);

  std::vector<AranetMonitor*> getAranetMonitors();

  uint32_t onPinRequested();
  void onConnect(NimBLEClient* pClient);
  void pause(boolean pauseResume);

private:
  Aranet4* aranet;
  NimBLEScan* bleScan;
  updateMessageCallback_t updateMessageCallback;
  publishMeasurement_t publishMeasurement;
  publishMessageCallback_t publishMessageCallback;
  TaskHandle_t task;

  std::vector<AranetMonitor*> aranetMonitorVector;
  uint16_t queryInterval;
  int lastRssi;

  void scan();
  void queryKnownMonitors();
  static void aranetScanLoop(void* pvParameters);
  boolean paused = false;
};


#endif