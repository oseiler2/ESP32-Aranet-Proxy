#include <config.h>
#include <Arduino.h>
#include <lcd.h>
#include <i2c.h>
#include <configManager.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMono9pt7b.h>

#define FONT_64 &FreeMonoBold24pt7b
#define FONT_32 &FreeMonoBold18pt7b
#define FONT_9 &FreeMono9pt7b

// Local logging tag
static const char TAG[] = __FILE__;

LCD::LCD(TwoWire* _wire) {
  priorityMessageActive = false;
  display = new Adafruit_SSD1306(128, config.ssd1306Rows, _wire, -1, 800000, I2C_CLK);

  // status line
  status_y = config.ssd1306Rows == 32 ? 24 : 0;
  status_height = 8;
  // temperature/humidity line. For 32 row displays same as status line
  temp_hum_y = config.ssd1306Rows == 32 ? 24 : 56;
  temp_hum_height = 8;

  line1_y = config.ssd1306Rows == 32 ? 0 : 8;
  line2_y = config.ssd1306Rows == 32 ? 8 : 24;
  line3_y = config.ssd1306Rows == 32 ? 16 : 40;
  line_height = config.ssd1306Rows == 32 ? 8 : 16;

  if (!I2C::takeMutex(portMAX_DELAY)) return;
  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  this->display->begin(SSD1306_SWITCHCAPVCC, SSD1306_I2C_ADR, false, false);  // initialize with the I2C addr 0x3C (for the 128x32)

  // Clear the buffer.
  this->display->clearDisplay();
  this->display->display();
  this->display->setTextColor(WHITE);
  this->display->setTextWrap(false);
  I2C::giveMutex();
}

LCD::~LCD() {
  if (this->display) delete display;
};

void LCD::updateMessage(char const* msg) {
  if (priorityMessageActive) return;
  if (!I2C::takeMutex(I2C_MUTEX_DEF_WAIT)) return;
  this->display->writeFillRect(0, status_y, 128, status_height, BLACK);
  this->display->setFont(NULL);
  this->display->setCursor(0, status_y);
  this->display->setTextSize(1);
  this->display->printf("%-21s", msg);
  this->display->display();
  I2C::giveMutex();
}

void LCD::setPriorityMessage(char const* msg) {
  if (!I2C::takeMutex(I2C_MUTEX_DEF_WAIT)) return;
  this->priorityMessageActive = true;
  this->display->writeFillRect(0, status_y, 128, status_height, BLACK);
  this->display->setFont(NULL);
  this->display->setCursor(0, status_y);
  this->display->setTextSize(1);
  this->display->printf("%-21s", msg);
  this->display->display();
  I2C::giveMutex();
}

void LCD::clearPriorityMessage() {
  if (!I2C::takeMutex(I2C_MUTEX_DEF_WAIT)) return;
  this->display->writeFillRect(0, status_y, 128, status_height, BLACK);
  this->display->display();
  this->priorityMessageActive = false;
  I2C::giveMutex();
}

void LCD::update(AranetMonitor* monitor) {
  if (!I2C::takeMutex(I2C_MUTEX_DEF_WAIT)) return;

  // 8-24 vs 12-40
  this->display->writeFillRect(0, line1_y, 128, line_height * 3, BLACK);

  // line 1
  this->display->setFont(config.ssd1306Rows == 32 ? NULL : FONT_9);
  this->display->setTextSize(1);
  this->display->setCursor(0, line1_y + (config.ssd1306Rows == 32 ? 0 : (line_height - 4)));
  if (monitor->lastData.co2 == 0) {
    this->display->print("CO2: ----");
  } else {
    this->display->printf("CO2: %4u", monitor->lastData.co2);
  }
  this->display->setFont(NULL);
  this->display->setCursor(this->display->getCursorX() + 3, this->display->getCursorY());
  this->display->print("ppm");

  // line 2
  this->display->setFont(config.ssd1306Rows == 32 ? NULL : FONT_9);
  this->display->setCursor(0, line2_y + (config.ssd1306Rows == 32 ? 0 : (line_height - 4)));
  //  this->display->setFont(config.ssd1306Rows == 32 ? NULL : FONT_9);
  //  this->display->print(monitor->name);
  this->display->printf("%s", monitor->name);

  // line 3
  this->display->setCursor(0, line3_y + (config.ssd1306Rows == 32 ? 0 : (line_height - 4)));
  //  this->display->printf(" %3u s", (millis() - monitor->lastSeen) / 1000);

  this->display->setFont(NULL);
  this->display->printf("rssi: %4i  bat: %u%%", monitor->rssi, monitor->lastData.battery);

  // line 4
  this->display->writeFillRect(0, temp_hum_y, 128, temp_hum_height, BLACK);
  this->display->setFont(NULL);
  this->display->setCursor(0, temp_hum_y);
  this->display->printf("temp: %3.1f  hum: %u%%", monitor->lastData.temperature / 20.0, monitor->lastData.humidity);

  this->display->display();
  I2C::giveMutex();
}

