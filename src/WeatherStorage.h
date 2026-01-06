#ifndef WEATHER_STORAGE_H
#define WEATHER_STORAGE_H

#include <Arduino.h>
#include <Preferences.h>
#include "Display.h"

// Data validity flags
#define DATA_NONE    0
#define DATA_CURRENT 1
#define DATA_DAILY   2
#define DATA_HOURLY  4
#define DATA_HISTORY 8

struct WeatherDataRTC {
  char conditionText[64];
  char iconName[32];
  float temp;
  float feelsLike;
  float windSpeed;
  float windGust;
  int windDirection;
  int humidity;
  int precipitationProbability;
  int uvIndex;
  int pressure;
  bool valid;
};

struct DailyForecastRTC {
  char dayName[16];
  char iconName[32];
  char conditionText[64];
  float tempHigh;
  float tempLow;
  char sunrise[8];
  char sunset[8];
  float sunriseHour;
  float sunsetHour;
};

class WeatherStorage {
public:
    void begin();
    void saveWeatherData(int typeMask, int currentHour, int currentDay, const WeatherData& current, const DailyForecast daily[], const HourlyData hourly[]);
    int loadWeatherData(int currentHour, int currentDay, WeatherData& current, DailyForecast daily[], HourlyData hourly[]);

private:
    Preferences preferences;
};

#endif
