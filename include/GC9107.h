#ifndef _GC9107_H
#define _GC9107_H

#include <globals.h>
#include <Wire.h>

#include <Arduino_GFX_Library.h>
#include <aranet-scanner.h>

class GC9107 {
public:
  GC9107();
  ~GC9107();

  void update(AranetMonitor* monitor);
  void updateMessage(char const* msg);
  void setPriorityMessage(char const* msg);
  void clearPriorityMessage();

private:
  Arduino_GFX* display;
  Arduino_DataBus* bus;

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