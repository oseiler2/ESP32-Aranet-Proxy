#include <config.h>
#include <Arduino.h>
#include <M5LCD.h>
#include <configManager.h>


#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>

#define FONT_64 &FreeMonoBold24pt7b
#define FONT_32 &FreeMonoBold18pt7b
#define FONT_9 &FreeMono9pt7b

// Local logging tag
static const char TAG[] = __FILE__;

M5LCD::M5LCD() {
  priorityMessageActive = false;
  //  display = new Adafruit_SSD1306(128, config.ssd1306Rows, _wire, -1, 800000, I2C_CLK);

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

  //if (!I2C::takeMutex(portMAX_DELAY)) return;
  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  //M5.Lcd.begin(SSD1306_SWITCHCAPVCC, SSD1306_I2C_ADR, false, false);  // initialize with the I2C addr 0x3C (for the 128x32)

  // Clear the buffer.

  M5.Lcd.clearDisplay();
  M5.Lcd.display();
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextWrap(false);
}

M5LCD::~M5LCD() {
  //  if (this->display) delete display;
};

void M5LCD::updateMessage(char const* msg) {
  if (priorityMessageActive) return;
  M5.Lcd.fillRect(0, status_y, 128, status_height, BLACK);
  M5.Lcd.setFont(NULL);
  M5.Lcd.setCursor(0, status_y);
  M5.Lcd.setTextSize(1);
  M5.Lcd.printf("%-21s", msg);
  M5.Lcd.display();
}

void M5LCD::setPriorityMessage(char const* msg) {
  this->priorityMessageActive = true;
  M5.Lcd.fillRect(0, status_y, 128, status_height, BLACK);
  M5.Lcd.setFont(NULL);
  M5.Lcd.setCursor(0, status_y);
  M5.Lcd.setTextSize(1);
  M5.Lcd.printf("%-21s", msg);
  M5.Lcd.display();
}

void M5LCD::clearPriorityMessage() {
  M5.Lcd.fillRect(0, status_y, 128, status_height, BLACK);
  M5.Lcd.display();
  this->priorityMessageActive = false;
}

void M5LCD::update(AranetMonitor* monitor) {
  // 8-24 vs 12-40
  M5.Lcd.fillRect(4, line1_y, 120, line_height * 3, BLACK);

  // line 1
  M5.Lcd.setFont(FONT_9);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(0, line1_y + ((line_height - 4)));
  if (monitor->lastData.co2 == 0) {
    M5.Lcd.print("CO2: ----");
  } else {
    M5.Lcd.printf("CO2: %4u", monitor->lastData.co2);
  }
  M5.Lcd.setFont(NULL);
  M5.Lcd.setCursor(M5.Lcd.getCursorX() + 3, M5.Lcd.getCursorY() - 4);
  M5.Lcd.print("ppm");

  // line 2
  M5.Lcd.setFont(FONT_9);
  M5.Lcd.setCursor(0, line2_y + ((line_height - 4)));
  //  M5.Lcd.setFont( FONT_9);
  //  M5.Lcd.print(monitor->name);
  M5.Lcd.printf("%s", monitor->name);

  // line 3
  M5.Lcd.setFont(NULL);
  M5.Lcd.setCursor(0, line3_y);
  //  M5.Lcd.printf(" %3u s", (millis() - monitor->lastSeen) / 1000);

  M5.Lcd.printf("rssi: %4i  bat: %u%%", monitor->rssi, monitor->lastData.battery);

  // line 4
  M5.Lcd.fillRect(0, temp_hum_y, 128, temp_hum_height, BLACK);
  M5.Lcd.setFont(NULL);
  M5.Lcd.setCursor(0, temp_hum_y);
  M5.Lcd.printf("temp: %3.1f  hum: %u%%", monitor->lastData.temperature / 20.0, monitor->lastData.humidity);

  M5.Lcd.display();
}

