#include <config.h>
#include <Arduino.h>
#include <GC9107.h>
#include <configManager.h>


#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMono9pt7b.h>

#define FONT_64 &FreeMonoBold24pt7b
#define FONT_32 &FreeMonoBold18pt7b
#define FONT_9 &FreeMono9pt7b

// Local logging tag
static const char TAG[] = __FILE__;
/*

GPIO15 	LCD_CS 	GPIO15
GPIO16 	LCD_BL 	GPIO16
GPIO17 	LCD_SCLK 	GPIO17
GPIO21 	LCD_MOSI 	GPIO21
GPIO33 	LCD_DC 	GPIO33
GPIO34 	LCD_RST 	GPIO34

          bus_cfg.freq_write = 40000000;
          bus_cfg.freq_read  = 16000000;

*/

GC9107::GC9107() {
  priorityMessageActive = false;

  bus = new Arduino_HWSPI(33, 15, 17, 21, 13);
  display = new Arduino_GC9107(bus, 34, 0 /* rotation */, true /* IPS */);

  display->begin();
  display->fillScreen(BLACK);

  pinMode(16, OUTPUT);
  digitalWrite(16, HIGH);

  // status line
  status_y = 0;
  status_height = 8;
  // temperature/humidity line. For 32 row displays same as status line
  temp_hum_y = 56;
  temp_hum_height = 8;

  line1_y = 8;
  line2_y = 24;
  line3_y = 40;
  line_height = 16;

  // Clear the buffer.

//  this->display->clearDisplay();
//  this->display->display();
  this->display->setTextColor(WHITE);
  this->display->setTextWrap(false);
}

GC9107::~GC9107() {
  if (this->display) delete display;
  if (this->bus) delete bus;
};

void GC9107::updateMessage(char const* msg) {
  if (priorityMessageActive) return;
  this->display->fillRect(0, status_y, 128, status_height, BLACK);
  this->display->setFont(NULL);
  this->display->setCursor(0, status_y);
  this->display->setTextSize(1);
  this->display->printf("%-21s", msg);
  //  this->display->display();
}

void GC9107::setPriorityMessage(char const* msg) {
  this->priorityMessageActive = true;
  this->display->fillRect(0, status_y, 128, status_height, BLACK);
  this->display->setFont(NULL);
  this->display->setCursor(0, status_y);
  this->display->setTextSize(1);
  this->display->printf("%-21s", msg);
  //  this->display->display();
}

void GC9107::clearPriorityMessage() {
  this->display->fillRect(0, status_y, 128, status_height, BLACK);
  //  this->display->display();
  this->priorityMessageActive = false;
}

void GC9107::update(AranetMonitor* monitor) {

  // 8-24 vs 12-40
  this->display->fillRect(4, line1_y, 120, line_height * 3, BLACK);

  // line 1
  this->display->setFont(FONT_9);
  this->display->setTextSize(1);
  this->display->setCursor(0, line1_y + ((line_height - 4)));
  if (monitor->lastData.co2 == 0) {
    this->display->print("CO2: ----");
  } else {
    this->display->printf("CO2: %4u", monitor->lastData.co2);
  }
  this->display->setFont(NULL);
  this->display->setCursor(this->display->getCursorX() + 3, this->display->getCursorY() - 4);
  this->display->print("ppm");

  // line 2
  this->display->setFont(FONT_9);
  this->display->setCursor(0, line2_y + ((line_height - 4)));
  //  this->display->setFont( FONT_9);
  //  this->display->print(monitor->name);
  this->display->printf("%s", monitor->name);

  // line 3
  this->display->setFont(NULL);
  this->display->setCursor(0, line3_y);
  //  this->display->printf(" %3u s", (millis() - monitor->lastSeen) / 1000);

  this->display->printf("rssi: %4i  bat: %u%%", monitor->rssi, monitor->lastData.battery);

  // line 4
  this->display->fillRect(0, temp_hum_y, 128, temp_hum_height, BLACK);
  this->display->setFont(NULL);
  this->display->setCursor(0, temp_hum_y);
  this->display->printf("temp: %3.1f  hum: %u%%", monitor->lastData.temperature / 20.0, monitor->lastData.humidity);

  //  this->display->display();
}

