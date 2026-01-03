#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <GxEPD2_7C.h>
#include <epd7c/GxEPD2_730c_GDEP073E01.h>
#include <Adafruit_GFX.h>
#include "WeatherIcons.h"

// Pin definitions
#define EPD_BUSY 25
#define EPD_RST  26
#define EPD_DC   2
#define EPD_CS   14
#define EPD_SCK  18
#define EPD_MOSI 23

struct WeatherData {
  String conditionText;
  String iconName;
  float temp;
  float feelsLike;
  float windSpeed;
  float windGust;
  int windDirection;
  int humidity;
  int precipitationProbability;
  int uvIndex;
  int pressure;
  bool valid = false;
};

struct DailyForecast {
  String dayName;
  String iconName;
  String conditionText;
  float tempHigh;
  float tempLow;
};

struct HourlyData {
  int hour; // 0-23
  float temp;
  int rainProb;
  float actualTemp = -100.0; // -100 indicates no data
};

class Display {
public:
    Display();
    void init();
    void drawWeather(const WeatherData& current, const DailyForecast daily[], const HourlyData hourly[]);

private:
    GxEPD2_7C<GxEPD2_730c_GDEP073E01, GxEPD2_730c_GDEP073E01::HEIGHT / 8> display;
    WeatherIcons weatherIcons;

    void RenderText(int16_t x, int16_t y, const GFXfont *font, uint16_t color, String text, int maxCharsPerLine = 12);
    void RenderTitleText(int16_t x, int16_t y, String text, int16_t maxCharsPerLine = 15);
    void RenderPrimaryValue(int16_t x, int16_t y, String text, int16_t maxCharsPerLine = 15);
    void RenderSecondaryValue(int16_t x, int16_t y, String text, int16_t maxCharsPerLine = 20);
    void drawWindDirection(int cx, int cy, int r, float WindDirection);
    void drawDailyForecast(int x, int y, int w, int h, const DailyForecast daily[]);
    void drawGraphs(int x, int y, int w, int h, const HourlyData hourly[]);
};

#endif
