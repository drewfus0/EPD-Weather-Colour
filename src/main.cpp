#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>
#include <Preferences.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "secrets.h"
#include "Display.h"
#include "WeatherAPI.h"

// Forward declaration
void ListWifiAPs();

const char* ntpServer = "pool.ntp.org";
// Melbourne Timezone
const char* time_zone = "AEST-10AEDT,M10.1.0,M4.1.0/3";

Preferences preferences;
Adafruit_BME280 bme; // I2C


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

WeatherData currentWeather;
DailyForecast dailyForecasts[5];
HourlyData hourlyData[24];

Display displayHandler;

void saveWeatherData(int typeMask, int currentHour, int currentDay) {
    preferences.begin("weather", false);
    
    int savedHour = preferences.getInt("hour", -1);
    int savedDay = preferences.getInt("day", -1);
    int status = preferences.getInt("status", DATA_NONE);

    // If day changed, reset everything
    if (savedDay != currentDay) {
        status = DATA_NONE;
        preferences.putInt("day", currentDay);
        preferences.putInt("hour", currentHour);
    }
    // If hour changed (but day is same), keep Daily, clear others
    else if (savedHour != currentHour) {
        status &= DATA_DAILY;
        preferences.putInt("hour", currentHour);
    }

    // Save specific data based on mask
    if (typeMask & DATA_CURRENT) {
        WeatherDataRTC wd;
        strlcpy(wd.conditionText, currentWeather.conditionText.c_str(), sizeof(wd.conditionText));
        strlcpy(wd.iconName, currentWeather.iconName.c_str(), sizeof(wd.iconName));
        wd.temp = currentWeather.temp;
        wd.feelsLike = currentWeather.feelsLike;
        wd.windSpeed = currentWeather.windSpeed;
        wd.windGust = currentWeather.windGust;
        wd.windDirection = currentWeather.windDirection;
        wd.humidity = currentWeather.humidity;
        wd.precipitationProbability = currentWeather.precipitationProbability;
        wd.uvIndex = currentWeather.uvIndex;
        wd.pressure = currentWeather.pressure;
        wd.valid = currentWeather.valid;
        preferences.putBytes("current", &wd, sizeof(wd));
    }

    if (typeMask & DATA_DAILY) {
        DailyForecastRTC daily[5];
        for(int i=0; i<5; i++) {
            strlcpy(daily[i].dayName, dailyForecasts[i].dayName.c_str(), sizeof(daily[i].dayName));
            strlcpy(daily[i].iconName, dailyForecasts[i].iconName.c_str(), sizeof(daily[i].iconName));
            strlcpy(daily[i].conditionText, dailyForecasts[i].conditionText.c_str(), sizeof(daily[i].conditionText));
            daily[i].tempHigh = dailyForecasts[i].tempHigh;
            daily[i].tempLow = dailyForecasts[i].tempLow;
            strlcpy(daily[i].sunrise, dailyForecasts[i].sunrise.c_str(), sizeof(daily[i].sunrise));
            strlcpy(daily[i].sunset, dailyForecasts[i].sunset.c_str(), sizeof(daily[i].sunset));
            daily[i].sunriseHour = dailyForecasts[i].sunriseHour;
            daily[i].sunsetHour = dailyForecasts[i].sunsetHour;
        }
        preferences.putBytes("daily", daily, sizeof(daily));
    }

    // Both Hourly Forecast and History update the hourlyData array
    if ((typeMask & DATA_HOURLY) || (typeMask & DATA_HISTORY)) {
        preferences.putBytes("hourly", hourlyData, sizeof(hourlyData));
    }

    // Update status
    status |= typeMask;
    preferences.putInt("status", status);

    preferences.end();
    Serial.printf("Weather data saved (Mask: %d, New Status: %d)\n", typeMask, status);
}

int loadWeatherData(int currentHour, int currentDay) {
    preferences.begin("weather", true);
    int savedHour = preferences.getInt("hour", -1);
    int savedDay = preferences.getInt("day", -1);
    int status = preferences.getInt("status", DATA_NONE);
    
    if (savedDay != currentDay) {
        Serial.printf("New day (Saved: %d, Current: %d). Resetting data.\n", savedDay, currentDay);
        preferences.end();
        return DATA_NONE;
    }

    int validStatus = DATA_NONE;

    // Load Daily (Valid as long as day matches)
    if (status & DATA_DAILY) {
        DailyForecastRTC daily[5];
        if (preferences.getBytes("daily", daily, sizeof(daily)) == sizeof(daily)) {
            for(int i=0; i<5; i++) {
                dailyForecasts[i].dayName = String(daily[i].dayName);
                dailyForecasts[i].iconName = String(daily[i].iconName);
                dailyForecasts[i].conditionText = String(daily[i].conditionText);
                dailyForecasts[i].tempHigh = daily[i].tempHigh;
                dailyForecasts[i].tempLow = daily[i].tempLow;
                dailyForecasts[i].sunrise = String(daily[i].sunrise);
                dailyForecasts[i].sunset = String(daily[i].sunset);
                dailyForecasts[i].sunriseHour = daily[i].sunriseHour;
                dailyForecasts[i].sunsetHour = daily[i].sunsetHour;
            }
            validStatus |= DATA_DAILY;
        }
    }

    // Load Hour-Specific Data (Only if hour matches)
    if (savedHour == currentHour) {
        // Load Current
        if (status & DATA_CURRENT) {
            WeatherDataRTC wd;
            if (preferences.getBytes("current", &wd, sizeof(wd)) == sizeof(wd)) {
                currentWeather.conditionText = String(wd.conditionText);
                currentWeather.iconName = String(wd.iconName);
                currentWeather.temp = wd.temp;
                currentWeather.feelsLike = wd.feelsLike;
                currentWeather.windSpeed = wd.windSpeed;
                currentWeather.windGust = wd.windGust;
                currentWeather.windDirection = wd.windDirection;
                currentWeather.humidity = wd.humidity;
                currentWeather.precipitationProbability = wd.precipitationProbability;
                currentWeather.uvIndex = wd.uvIndex;
                currentWeather.pressure = wd.pressure;
                currentWeather.valid = wd.valid;
                validStatus |= DATA_CURRENT;
            }
        }
    } else {
        Serial.printf("New hour (Saved: %d, Current: %d). Retaining Daily, clearing others.\n", savedHour, currentHour);
    }
    
    // Load Hourly/History even if hour doesn't match (as long as day matches, which is checked above)
    // We need this to preserve accumulated indoor temperature and history data
    if ((status & DATA_HOURLY) || (status & DATA_HISTORY)) {
        if (preferences.getBytes("hourly", hourlyData, sizeof(hourlyData)) == sizeof(hourlyData)) {
                // If hour matches, we consider the forecast/history valid (no need to re-fetch)
                if (savedHour == currentHour) {
                    validStatus |= (status & (DATA_HOURLY | DATA_HISTORY));
                }
        }
    }

    preferences.end();
    Serial.printf("Weather data loaded (Status: %d)\n", validStatus);
    return validStatus;
}

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

    int status = loadWeatherData(currentHour, currentDay);

    // Initialize and read BME280
    // Standard I2C: SDA=21, SCL=22. Address 0x76 (SDO=GND) is common for modules.
    Serial.println("Initializing BME280...");
    if (!bme.begin(0x76)) {
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
    } else {
        // Mode normal, standby time 1000ms, filter off?
        // Default settings are usually fine for single shot reading.
        // Or force a measurement if in sleep mode?
        // adafruit library puts it in normal mode by default.
        
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
        
        // Mark as needing save? We'll just call save if we don't fetch anything else, 
        // or let the fetch save it (since it saves the whole array).
        // To be safe, let's flag that we have updated hourly data.
        // We can just rely on the saveWeatherData calls later if they happen.
        // If NO fetch happens (status == stored), we still want to save this new reading if it wasn't there.
        // Let's do a save right here? It might be redundant but safe.
        // Actually, let's just do it at the end of the logic if needed.
        // Or just do it here. It's fast enough.
        saveWeatherData(DATA_HOURLY, currentHour, currentDay);
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
            saveWeatherData(DATA_CURRENT, currentHour, currentDay);
        }
    }

    if (!(status & DATA_DAILY)) {
        Serial.println("Fetching Daily Forecast...");
        if (getDailyForecastData()) {
            saveWeatherData(DATA_DAILY, currentHour, currentDay);
        }
    }

    if (!(status & DATA_HOURLY)) {
        Serial.println("Fetching Hourly Forecast...");
        // Forecast: From now until end of day (approx). 
        int forecastHours = 24 - currentHour + 2;
        if (forecastHours > 48) forecastHours = 48; 
        if (getHourlyForecastData(forecastHours)) {
            saveWeatherData(DATA_HOURLY, currentHour, currentDay);
        }
    }

    if (!(status & DATA_HISTORY)) {
        Serial.println("Fetching History...");
        // History: From midnight until now.
        int historyHours = currentHour + 1;
        if (historyHours > 24) historyHours = 24;
        if (getHistoryData(historyHours)) {
            saveWeatherData(DATA_HISTORY, currentHour, currentDay);
        }
    }
    
    // display forcast data hourly to serial
    Serial.println("--- Forecast Data: 5 day ---");
    for(int i=0; i<5; i++) {
        Serial.printf("Forecast Day %d: %s, High: %.1f, Low: %.1f, Icon: %s\n", i, dailyForecasts[i].dayName.c_str(), dailyForecasts[i].tempHigh, dailyForecasts[i].tempLow, dailyForecasts[i].iconName.c_str());
    }
    Serial.println("--- Hourly Data: 24 hour ---");
    Serial.printf("%8s|%8s|%8s|%8s|%8s|%8s|%8s|%8s|%8s\n", "Hour", "Temp", "Actual", "Indoor", "Rain", "Prob", "Press", "IndPre", "Wind");
    Serial.println("--------|--------|--------|--------|--------|--------|--------|--------|--------");
    for(int i=0; i<24; i++) {
      Serial.printf("%8d|%8.1f|%8.1f|%8.1f|%8.1f|%7d%%|%8.1f|%8.1f|%8.1f\n", 
        if (hourlyData[i].indoorPressure == -1.0) hourlyData[i].indoorPressure = -100.0; // Alignment for print if needed or keep uniform sentinel

        hourlyData[i].hour, 
        hourlyData[i].temp, 
        hourlyData[i].actualTemp, 
        hourlyData[i].indoorTemp, 
        hourlyData[i].actualRain, 
        hourlyData[i].rainProb,
        hourlyData[i].pressure,
        hourlyData[i].indoorPressure);
        //hourlyData[i].windSpeed);
    }

  } else {
    Serial.println("Failed to obtain time, forcing forecast update...");
    // If we can't get time, we can't validate stored data, so we might as well try to fetch if we have wifi, or just sleep.
    // But we already connected to WiFi.
    // Let's try to fetch anyway? No, without time we can't filter history/forecast correctly.
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
