#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "secrets.h"
#include "Display.h"
#include "WeatherAPI.h"
#include "WeatherStorage.h"

// Forward declaration
void ListWifiAPs();

const char* ntpServer = "pool.ntp.org";
// Melbourne Timezone
const char* time_zone = "AEST-10AEDT,M10.1.0,M4.1.0/3";

Adafruit_BME280 bme; // I2C

WeatherData currentWeather;
DailyForecast dailyForecasts[5];
HourlyData hourlyData[24];

Display displayHandler;
WeatherStorage weatherStorage;

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
  
  // Connect to WiFi to get time
  connectToWiFi();

  // Initialize Storage
  weatherStorage.begin();
  
  struct tm timeinfo;
  bool timeSuccess = getLocalTime(&timeinfo, 10000);
  
  if (timeSuccess) {
    Serial.printf("Current Time: %02d:%02d, Day of Year: %d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_yday);
    
    int currentHour = timeinfo.tm_hour;
    int currentDay = timeinfo.tm_yday;

    // Try to load stored data
    // Initialize hourly array to safe defaults before loading or fetching
    for(int i=0; i<24; i++) {
        hourlyData[i].hour = i;
        hourlyData[i].temp = -100.0;
        hourlyData[i].actualTemp = -100.0;
        hourlyData[i].rainProb = -1;
        hourlyData[i].actualRain = -1.0;
        hourlyData[i].indoorTemp = -100.0;
        hourlyData[i].pressure = -1.0;
        hourlyData[i].actualPressure = -1.0;
        hourlyData[i].indoorPressure = -1.0;
    }

    int status = weatherStorage.loadWeatherData(currentHour, currentDay, currentWeather, dailyForecasts, hourlyData);

    // Initialize and read BME280
    // Standard I2C: SDA=21, SCL=22. Address 0x76 (SDO=GND) is common for modules.
    Serial.println("Initializing BME280...");
    if (!bme.begin(0x76)) {
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
    } else {
        float indoorT = bme.readTemperature();
        float indoorH = bme.readHumidity();
        float indoorP = bme.readPressure() / 100.0F;

        Serial.printf("BME280: Temp=%.2f C, Hum=%.2f %%, Pres=%.2f hPa\n", indoorT, indoorH, indoorP);
        
        // Update Current Weather Indoor Data
        currentWeather.indoorTemp = indoorT;
        currentWeather.indoorHumidity = indoorH;
        currentWeather.indoorPressure = indoorP;
        
        // Update Hourly Data for Graph - Current Hour
        hourlyData[currentHour].indoorTemp = indoorT;
        hourlyData[currentHour].indoorPressure = indoorP;
        
        // Save the BME data
        weatherStorage.saveWeatherData(DATA_HOURLY, currentHour, currentDay, currentWeather, dailyForecasts, hourlyData);
    }

    // Validate loaded data for current hour
    // If we think we have hourly data, but the current hour is empty, force a refresh.
    if ((status & DATA_HOURLY) && hourlyData[currentHour].temp == -100.0 && hourlyData[currentHour].actualTemp == -100.0) {
        Serial.printf("Data for current hour (%d) is missing. Forcing refresh.\n", currentHour);
        status &= ~DATA_HOURLY;
        status &= ~DATA_HISTORY;
        // Also force current to be safe
        status &= ~DATA_CURRENT;
    }

    // Check what is missing and fetch it
    if (!(status & DATA_CURRENT)) {
        Serial.println("Fetching Current Weather...");
        if (getWeatherCurrentData() && currentWeather.valid) {
            weatherStorage.saveWeatherData(DATA_CURRENT, currentHour, currentDay, currentWeather, dailyForecasts, hourlyData);
        }
    }

    if (!(status & DATA_DAILY)) {
        Serial.println("Fetching Daily Forecast...");
        if (getDailyForecastData()) {
            weatherStorage.saveWeatherData(DATA_DAILY, currentHour, currentDay, currentWeather, dailyForecasts, hourlyData);
        }
    }

    if (!(status & DATA_HOURLY)) {
        Serial.println("Fetching Hourly Forecast...");
        // Forecast: From now until end of day (approx). 
        int forecastHours = 24 - currentHour + 2;
        if (forecastHours > 48) forecastHours = 48; 
        if (getHourlyForecastData(forecastHours)) {
            weatherStorage.saveWeatherData(DATA_HOURLY, currentHour, currentDay, currentWeather, dailyForecasts, hourlyData);
        }
    }

    if (!(status & DATA_HISTORY)) {
        Serial.println("Fetching History...");
        // History: From midnight until now.
        int historyHours = currentHour + 1;
        if (historyHours > 24) historyHours = 24;
        if (getHistoryData(historyHours)) {
            weatherStorage.saveWeatherData(DATA_HISTORY, currentHour, currentDay, currentWeather, dailyForecasts, hourlyData);
        }
    }
    
    // display forcast data hourly to serial
    Serial.println("--- Forecast Data: 5 day ---");
    for(int i=0; i<5; i++) {
        Serial.printf("Forecast Day %d: %s, High: %.1f, Low: %.1f, Icon: %s\n", i, dailyForecasts[i].dayName.c_str(), dailyForecasts[i].tempHigh, dailyForecasts[i].tempLow, dailyForecasts[i].iconName.c_str());
    }
    Serial.println("--- Hourly Data: 24 hour ---");
    Serial.printf("%4s|%8s|%8s|%8s|%8s|%5s|%8s|%8s|%8s\n", "Hour", "Temp", "Actual", "Indoor", "Rain", "Prob", "Press", "ActPress", "IndPress");
    Serial.println("----|--------|--------|--------|--------|-----|--------|--------|--------");
    for(int i=0; i<24; i++) {
        Serial.printf("%4d|%8.1f|%8.1f|%8.1f|%8.1f|%4d%%|%8.1f|%8.1f|%8.1f\n", 
        hourlyData[i].hour, 
        hourlyData[i].temp, 
        hourlyData[i].actualTemp, 
        hourlyData[i].indoorTemp, 
        hourlyData[i].actualRain, 
        hourlyData[i].rainProb,
        hourlyData[i].pressure,
        hourlyData[i].actualPressure,
        hourlyData[i].indoorPressure);
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
