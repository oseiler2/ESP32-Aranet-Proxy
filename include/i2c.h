#ifndef _I2C_H
#define _I2C_H

#include <globals.h>

#define I2C_MUTEX_DEF_WAIT pdMS_TO_TICKS(5000)

namespace I2C {
#define SSD1306_I2C_ADR 0x3C

  void initI2C();

  boolean takeMutex(TickType_t blockTime);
  void giveMutex();

  boolean lcdPresent();
}
#endif