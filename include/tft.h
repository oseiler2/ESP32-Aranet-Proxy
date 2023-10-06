#ifndef _TFT_H
#define _TFT_H

#ifdef ST7789_DRIVER
#include <globals.h>
#include <SPI.h>

#include <Adafruit_ST7789.h>
#include <aranet-scanner.h>

class TFT {
public:
  TFT(SPIClass *spiClass);
  ~TFT();

  void update(AranetMonitor* monitor);
  void updateMessage(char const* msg);
  void setPriorityMessage(char const* msg);
  void clearPriorityMessage();

private:
  Adafruit_ST7789* display;

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
#endif