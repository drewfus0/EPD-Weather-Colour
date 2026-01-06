#ifndef WEATHER_API_H
#define WEATHER_API_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "Display.h"

// Declare external global variables that these functions modify
extern WeatherData currentWeather;
extern DailyForecast dailyForecasts[5];
extern HourlyData hourlyData[24];

// Function declarations
void getMockForecastData();
String getIconNameFromUri(String uri);
String getAPIData(String url);
void updateCurrentWeather(JsonObject hourly);
String getDayName(int year, int month, int day);
bool getDailyForecastData();
bool getHourlyForecastData(int hoursCount);
bool getHistoryData(int hoursCount);
bool getWeatherCurrentData();

#endif
