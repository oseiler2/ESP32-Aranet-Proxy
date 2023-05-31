#ifndef _M5LCD_H
#define _M5LCD_H

#include <globals.h>
#include <Wire.h>

#include <aranet-scanner.h>

#include <M5Unified.h>
#include <M5GFX.h>

class M5LCD {
public:
  M5LCD();
  ~M5LCD();

  void update(AranetMonitor* monitor);
  void updateMessage(char const* msg);
  void setPriorityMessage(char const* msg);
  void clearPriorityMessage();

private:
  boolean priorityMessageActive;

  uint8_t status_y;
  uint8_t status_height;
  uint8_t temp_hum_y;
  uint8_t temp_hum_height;
  uint8_t line1_y;
  uint8_t line2_y;
  uint8_t line3_y;
  uint8_t line_height;

};


#endif