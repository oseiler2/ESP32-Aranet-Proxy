#include <config.h>
#include <Arduino.h>
#include <tft.h>
#include <configManager.h>

#ifdef ST7789_DRIVER

#include <Adafruit_GFX.h>

#include <Fonts/FreeMono18pt7b.h>
#include <Fonts/FreeMono12pt7b.h>
#include <Fonts/FreeMono9pt7b.h>

#define FONT_9 &FreeMono9pt7b
#define FONT_12 &FreeMono12pt7b
#define FONT_18 &FreeMono18pt7b

// Local logging tag
static const char TAG[] = __FILE__;

TFT::TFT(SPIClass* _spiClass) {
  priorityMessageActive = false;
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  pinMode(TFT_CS, OUTPUT);
  _spiClass->begin((int8_t)TFT_SCK, (int8_t)TFT_MISO, (int8_t)TFT_MOSI, TFT_CS);

  // turn on power for TFT
#ifdef AUX_PWR 
  pinMode(AUX_PWR, OUTPUT);
  digitalWrite(AUX_PWR, HIGH);
#endif


  display = new Adafruit_ST7789(_spiClass, (int8_t)TFT_CS, (int8_t)TFT_DC, (int8_t)TFT_RST);

  display->init((uint16_t)TFT_WIDTH, (uint16_t)TFT_HEIGHT);
  display->setRotation(3);


  // status line
  status_y = 13;
  status_height = 18;
  // temperature/humidity line. For 32 row displays same as status line
  temp_hum_y = 130;
  temp_hum_height = 18;

  line1_y = 50;
  line2_y = 86;
  line3_y = 110;
  line_height = 35;

  // Clear the buffer.
  display->fillScreen(ST77XX_BLACK);
  display->setTextWrap(false);
  display->setCursor(0, 0);
  display->setTextSize(1);
  display->setTextColor(ST77XX_WHITE);
}

TFT::~TFT() {
  if (this->display) delete display;
};

void TFT::updateMessage(char const* msg) {
  if (priorityMessageActive) return;
  this->display->fillRect(0, status_y + 4 - status_height, display->width(), status_height, ST77XX_BLACK);
  this->display->setFont(FONT_9);
  this->display->setCursor(0, status_y);
  this->display->printf("%-21s", msg);
}

void TFT::setPriorityMessage(char const* msg) {
  display->setTextColor(ST77XX_ORANGE);
  this->priorityMessageActive = true;
  this->display->fillRect(0, status_y + 4 - status_height, display->width(), status_height, ST77XX_BLACK);
  this->display->setFont(FONT_9);
  this->display->setCursor(0, status_y);
  this->display->printf("%-21s", msg);
  display->setTextColor(ST77XX_WHITE);
}

void TFT::clearPriorityMessage() {
  this->display->fillRect(0, status_y + 4 - status_height, display->width(), status_height, ST77XX_BLACK);
  this->priorityMessageActive = false;
}

void TFT::update(AranetMonitor* monitor) {
  // 8-24 vs 12-40
  this->display->fillRect(0, line1_y + 4 - line_height, display->width(), TFT_HEIGHT - (line1_y + 4 - line_height), ST77XX_BLACK);

  // line 1
  this->display->setFont(FONT_18);
  this->display->setCursor(0, line1_y);
  this->display->print("CO2: ");
  if (monitor->lastData.co2 == 0) {
    this->display->print("----");
  } else {
    if (monitor->lastData.co2 <= 700) display->setTextColor(ST77XX_GREEN);
    else if (monitor->lastData.co2 <= 900) display->setTextColor(ST77XX_YELLOW);
    else if (monitor->lastData.co2 <= 1200) display->setTextColor(ST77XX_ORANGE);
    else  display->setTextColor(ST77XX_RED);
    this->display->printf("%4u", monitor->lastData.co2);
    display->setTextColor(ST77XX_WHITE);
  }
  this->display->setFont(FONT_9);
  this->display->setCursor(this->display->getCursorX() + 3, this->display->getCursorY());
  this->display->print("ppm");

  // line 2
  this->display->setFont(FONT_18);
  this->display->setCursor(0, line2_y);
  this->display->printf("%s", monitor->name);

  // line 3
  this->display->setFont(FONT_9);
  this->display->setCursor(0, line3_y);
  this->display->printf("Rssi: %4i  Bat: %u%%", monitor->rssi, monitor->lastData.battery);

  // line 4
  this->display->setFont(FONT_9);
  this->display->setCursor(0, temp_hum_y);
  this->display->printf("Temp: %4.1f  Hum: %u%%", monitor->lastData.temperature / 20.0, monitor->lastData.humidity);

}


#endif