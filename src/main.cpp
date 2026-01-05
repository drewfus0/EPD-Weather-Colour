#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>
#include "secrets.h"
#include "Display.h"
#include "WeatherAPI.h"

// Forward declaration
void ListWifiAPs();

const char* ntpServer = "pool.ntp.org";
// Melbourne Timezone
const char* time_zone = "AEST-10AEDT,M10.1.0,M4.1.0/3";

// Preferences preferences; // Removed as per request

WeatherData currentWeather;
DailyForecast dailyForecasts[3];
HourlyData hourlyData[24];

Display displayHandler;

// saveForecastToFlash and loadForecastFromFlash removed

void connectToWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  int retry_count = 0;
  while (WiFi.status() != WL_CONNECTED && retry_count < 40) {
    delay(500);
    Serial.print(".");
    retry_count++;
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected! IP address: ");
    Serial.println(WiFi.localIP());
    
    // Init and get the time
    configTime(0, 0, ntpServer);
    setenv("TZ", time_zone, 1);
    tzset();
  } else {
    Serial.println("Failed to connect to WiFi. Check credentials in secrets.h :(RetryCnt=" + String(retry_count) + ")");
    ListWifiAPs();
  }
}

void ListWifiAPs()
{
  // list wifi networks in range
  Serial.println("Scanning for available WiFi networks...");
  int n = WiFi.scanNetworks();
  if (n == 0)
  {
    Serial.println("No networks found");
  }
  else
  {
    Serial.print(n);
    Serial.println(" Networks found:");
    Serial.printf("%-4s | %-32s | %-6s | %s\n", "No", "SSID", "RSSI", "Enc");
    for (int i = 0; i < n; ++i)
    {
      Serial.printf("%-4d | %-32s | %-6d | %s\n",
                    i + 1,
                    WiFi.SSID(i).c_str(),
                    WiFi.RSSI(i),
                    (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Auth");
    }
  }
}

void sleepUntilNextHour() {
  Serial.println("Calculating sleep duration...");
  struct tm timeinfo;
  // Wait up to 10 seconds for time
  if(!getLocalTime(&timeinfo, 10000)){
    Serial.println("Failed to obtain time, sleeping for 1 hour");
    esp_sleep_enable_timer_wakeup(3600 * 1000000ULL);
    esp_deep_sleep_start();
    return;
  }
  
  Serial.println(&timeinfo, "Current time (UTC): %H:%M:%S");
  
  // Calculate seconds until next hour
  int secondsToSleep = 3600 - (timeinfo.tm_min * 60 + timeinfo.tm_sec);
  
  // Ensure we sleep at least a little bit if we are right on the hour
  if (secondsToSleep <= 0) secondsToSleep = 3600;
  
  Serial.printf("Sleeping for %d seconds until next hour...\n", secondsToSleep);
  Serial.flush(); 
  
  esp_sleep_enable_timer_wakeup((uint64_t)secondsToSleep * 1000000ULL);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(10000);

  Serial.println();
  Serial.println("--- Weather Display Start ---");
  
  // Connect to WiFi and get weather data
  connectToWiFi();
  
  // Always get current weather
  getWeatherCurrentData();
  
  struct tm timeinfo;
  bool timeSuccess = getLocalTime(&timeinfo, 10000);
  
  if (timeSuccess) {
    Serial.printf("Current Day of Year: %d\n", timeinfo.tm_yday);
    
    // Always fetch new data
    Serial.println("Fetching forecast and history data...");
    getDailyForecastData();
    
    int currentHour = timeinfo.tm_hour;
    // Forecast: From now until end of day (approx). 
    // If 10am, we need 14 hours (10,11...23). 24 - 10 = 14.
    // Add 2 for safety/overlap.
    int forecastHours = 24 - currentHour + 2;
    if (forecastHours > 48) forecastHours = 48; 
    getHourlyForecastData(forecastHours);
    
    // History: From midnight until now.
    // If 10am, we need 11 hours (00...10). 10 + 1 = 11.
    int historyHours = currentHour + 1;
    if (historyHours > 24) historyHours = 24;
    getHistoryData(historyHours);
    
    // display forcast data hourly to serial
    Serial.println("--- Forecast Data: 3 day ---");
    for(int i=0; i<3; i++) {
        Serial.printf("Forecast Day %d: %s, High: %.1f, Low: %.1f, Icon: %s\n", i, dailyForecasts[i].dayName.c_str(), dailyForecasts[i].tempHigh, dailyForecasts[i].tempLow, dailyForecasts[i].iconName.c_str());
    }
    Serial.println("--- Hourly Data: 24 hour ---");
    for(int i=0; i<24; i++) {
        Serial.printf("Hour %02d: Temp: %.1f, Rain Prob: %d%%, Actual Temp: %.1f\n", hourlyData[i].hour, hourlyData[i].temp, hourlyData[i].rainProb, hourlyData[i].actualTemp);
    }

  } else {
    Serial.println("Failed to obtain time, forcing forecast update...");
    sleepUntilNextHour();
  }
  delay(1000);
  displayHandler.init();
  delay(1000);
  displayHandler.drawWeather(currentWeather, dailyForecasts, hourlyData);
  
  sleepUntilNextHour();
}

void loop() {
}
