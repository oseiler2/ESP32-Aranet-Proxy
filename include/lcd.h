#ifndef _LCD_H
#define _LCD_H

#include <globals.h>
#include <Wire.h>

#include <Adafruit_SSD1306.h>
#include <aranet-scanner.h>

class LCD {
public:
  LCD(TwoWire* pwire);
  ~LCD();

  void update(AranetMonitor* monitor);
  void updateMessage(char const* msg);
  void setPriorityMessage(char const* msg);
  void clearPriorityMessage();

private:
  Adafruit_SSD1306* display;

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