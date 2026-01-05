#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>
#include "secrets.h"
#include "Display.h"

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


void getMockForecastData() {
  // Mock 3-day forecast
  dailyForecasts[0] = {"Tomorrow", "partly_cloudy", "Partly Cloudy", 22.5, 14.0};
  dailyForecasts[1] = {"Wednesday", "rain", "Rain", 18.0, 12.5};
  dailyForecasts[2] = {"Thursday", "sunny", "Sunny", 25.0, 15.0};

  // Mock 24-hour data
  for (int i = 0; i < 24; i++) {
    hourlyData[i].hour = i;
    hourlyData[i].temp = 15.0 + 5.0 * sin((i - 6) * PI / 12.0); // Low at 6am, High at 6pm
    if (i > 10 && i < 18) {
        hourlyData[i].rainProb = 0;
    } else {
        hourlyData[i].rainProb = random(0, 60);
    }
  }
}

// e.g. "https://maps.gstatic.com/weather/v1/partly_clear.png" -> "partly_clear"
String getIconNameFromUri(String uri) {
  int lastSlash = uri.lastIndexOf('/');
  int dot = uri.lastIndexOf('.');
  if (lastSlash != -1 && dot != -1 && dot > lastSlash) {
    return uri.substring(lastSlash + 1, dot);
  }
  // If no extension, maybe it's just the name
  if (lastSlash != -1) return uri.substring(lastSlash + 1);
  return uri;
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

String getAPIData(String url) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure(); // Skip certificate validation
    HTTPClient http;
    Serial.println("Requesting URL: " + url);
    
    http.begin(client, url);
    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
      String payload = http.getString();
      Serial.println("HTTP Response code: " + String(httpResponseCode));
      Serial.println("Payload size: " + String(payload.length()) + " bytes");
      // Print only the first 500 characters to avoid blocking Serial
      Serial.println("Payload preview: " + payload.substring(0, 500) + "...");
      http.end();
      return payload;
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
  return "";
}

void updateCurrentWeather(JsonObject hourly) {
  currentWeather.conditionText = hourly["weatherCondition"]["description"]["text"].as<String>();
  String uri = hourly["weatherCondition"]["iconBaseUri"].as<String>();
  currentWeather.iconName = getIconNameFromUri(uri);
  
  currentWeather.temp = hourly["temperature"]["degrees"];
  currentWeather.feelsLike = hourly["feelsLikeTemperature"]["degrees"];
  currentWeather.windSpeed = hourly["wind"]["speed"]["value"];
  currentWeather.windGust = hourly["wind"]["gust"]["value"];
  currentWeather.windDirection = hourly["wind"]["direction"]["degrees"];
  
  currentWeather.humidity = hourly["relativeHumidity"];
  currentWeather.precipitationProbability = hourly["precipitation"]["probability"]["percent"];
  
  currentWeather.uvIndex = hourly["uvIndex"];
  currentWeather.pressure = hourly["airPressure"]["meanSeaLevelMillibars"];
  
  currentWeather.valid = true;
  
  Serial.println("--- Parsed Weather Data ---");
  Serial.println("Condition: " + currentWeather.conditionText);
  Serial.println("Icon Name: " + currentWeather.iconName);
  Serial.println("Temp: " + String(currentWeather.temp));
  Serial.println("Feels Like: " + String(currentWeather.feelsLike));
  Serial.println("Wind: " + String(currentWeather.windSpeed) + " km/h, Dir: " + String(currentWeather.windDirection));
  Serial.println("Humidity: " + String(currentWeather.humidity) + "%");
  Serial.println("Rain Prob: " + String(currentWeather.precipitationProbability) + "%");
  Serial.println("UV: " + String(currentWeather.uvIndex));
  Serial.println("Pressure: " + String(currentWeather.pressure));
}

String getDayName(int year, int month, int day) {
    // Zeller's congruence
    if (month < 3) {
        month += 12;
        year -= 1;
    }
    int k = year % 100;
    int j = year / 100;
    int h = (day + 13 * (month + 1) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
    
    String days[] = {"Saturday", "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday"};
    return days[h];
}

void getDailyForecastData() {
  if (WiFi.status() == WL_CONNECTED) {
    String url = "https://weather.googleapis.com/v1/forecast/days:lookup?key=" + String(GOOGLE_API_KEY) 
      + "&location.latitude=" + String(LATITUDE) 
      + "&location.longitude=" + String(LONGITUDE) 
      + "&days=3"
      + "&unitsSystem=METRIC";
    
    Serial.println("Requesting URL: " + url);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, url);
    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
      String payload = http.getString();
      Serial.println("HTTP Response code: " + String(httpResponseCode));
      
      JsonDocument filter;
      filter["forecastDays"][0]["displayDate"] = true;
      filter["forecastDays"][0]["maxTemperature"]["degrees"] = true;
      filter["forecastDays"][0]["minTemperature"]["degrees"] = true;
      filter["forecastDays"][0]["daytimeForecast"]["weatherCondition"]["description"]["text"] = true;
      filter["forecastDays"][0]["daytimeForecast"]["weatherCondition"]["iconBaseUri"] = true;

      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));

      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
      } else {
        JsonArray forecasts = doc["forecastDays"];
        for(int i=0; i<3 && i<forecasts.size(); i++) {
            JsonObject f = forecasts[i];
            int y = f["displayDate"]["year"];
            int m = f["displayDate"]["month"];
            int d = f["displayDate"]["day"];
            
            dailyForecasts[i].dayName = getDayName(y, m, d);
            dailyForecasts[i].tempHigh = f["maxTemperature"]["degrees"];
            dailyForecasts[i].tempLow = f["minTemperature"]["degrees"];
            dailyForecasts[i].conditionText = f["daytimeForecast"]["weatherCondition"]["description"]["text"].as<String>();
            
            String uri = f["daytimeForecast"]["weatherCondition"]["iconBaseUri"].as<String>();
            dailyForecasts[i].iconName = getIconNameFromUri(uri);
            
            Serial.printf("Day %d: %s, High: %.1f, Low: %.1f, Icon: %s\n", i, dailyForecasts[i].dayName.c_str(), dailyForecasts[i].tempHigh, dailyForecasts[i].tempLow, dailyForecasts[i].iconName.c_str());
        }
      }
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}

void getHourlyForecastData(int hoursCount) {
  if (WiFi.status() == WL_CONNECTED) {
    // Request hoursCount hours to ensure we cover the rest of the current day
    String url = "https://weather.googleapis.com/v1/forecast/hours:lookup?key=" + String(GOOGLE_API_KEY) 
      + "&location.latitude=" + String(LATITUDE) 
      + "&location.longitude=" + String(LONGITUDE) 
      + "&hours=" + String(hoursCount)
      + "&unitsSystem=METRIC";
    
    Serial.println("Requesting URL: " + url);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, url);
    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
      String payload = http.getString();
      Serial.println("HTTP Response code: " + String(httpResponseCode));
      
      JsonDocument filter;
      filter["forecastHours"][0]["interval"]["startTime"] = true; // "2026-01-03T17:00:00Z"
      filter["forecastHours"][0]["temperature"]["degrees"] = true;
      filter["forecastHours"][0]["precipitation"]["probability"]["percent"] = true;

      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));

      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
      } else {
        // Reset hourly data for the new day
        for(int i=0; i<24; i++) {
            hourlyData[i].hour = i;
            hourlyData[i].temp = -100.0; // Sentinel for no data
            hourlyData[i].rainProb = -1; // Sentinel for no data
            hourlyData[i].actualTemp = -100.0;
        }

        // Get current day to filter forecast
        struct tm timeinfo;
        if(!getLocalTime(&timeinfo, 1000)){
            Serial.println("Failed to obtain time for forecast filtering");
        }
        int currentDay = timeinfo.tm_mday;

        JsonArray forecasts = doc["forecastHours"];
        for(int i=0; i<forecasts.size(); i++) {
            JsonObject f = forecasts[i];
            String timeStr = f["interval"]["startTime"].as<String>();
            
            // Parse ISO string "2026-01-03T17:00:00Z"
            // Note: The API returns UTC time. We need to convert to local time or rely on the fact that we want "today" in local time.
            // However, parsing the string manually is tricky with timezones.
            // Better approach: Parse the time string into a tm struct, apply timezone offset, then check day.
            
            struct tm tm_forecast;
            strptime(timeStr.c_str(), "%Y-%m-%dT%H:%M:%SZ", &tm_forecast);
            time_t t_forecast = mktime(&tm_forecast); // This assumes local time if not careful, but strptime doesn't set timezone.
            // Actually, the API returns UTC (Z).
            // We need to adjust for our timezone offset.
            // Our timezone is set in setup() with configTime.
            // But mktime uses the TZ environment variable.
            // Since we set TZ in setup, mktime might interpret the input as local time? No, strptime just fills the struct.
            // Let's manually adjust for UTC+10/11 (Melbourne).
            // Or simpler: The API returns UTC. We know our offset.
            // But DST makes it hard.
            
            // Alternative: Use the hour from the string and assume the API returns what we want? No, it returns UTC.
            // If we are in Melbourne (UTC+11 in Jan), 00:00 Local is 13:00 UTC previous day.
            // 10:00 Local is 23:00 UTC previous day.
            // 11:00 Local is 00:00 UTC current day.
            
            // Let's use the system time functions which are already configured.
            // We can parse the year, month, day, hour, min, sec from the string.
            int y, M, d, h, m, s;
            sscanf(timeStr.c_str(), "%d-%d-%dT%d:%d:%dZ", &y, &M, &d, &h, &m, &s);
            
            struct tm tm_utc = {0};
            tm_utc.tm_year = y - 1900;
            tm_utc.tm_mon = M - 1;
            tm_utc.tm_mday = d;
            tm_utc.tm_hour = h;
            tm_utc.tm_min = m;
            tm_utc.tm_sec = s;
            tm_utc.tm_isdst = 0; // UTC has no DST
            
            // Convert UTC tm to time_t (assuming UTC)
            // timegm is not standard in all Arduino, but _mkgmtime might be available or we use mktime and adjust.
            // Since we set TZ to Melbourne, mktime will interpret this as Melbourne time, which is wrong.
            // We need to treat it as UTC.
            // Let's just add the offset manually? No, DST.
            
            // Let's use the standard time library correctly.
            // We have the current epoch time.
            // We can convert the forecast time string to epoch.
            // Then convert that epoch to local tm.
            
            // Manual UTC to epoch:
            // This is getting complicated.
            // Simpler hack: The device has the correct local time.
            // We can just iterate 0-23 of "today" and see if we can find a matching forecast in the list?
            // No, we have the list.
            
            // Let's try to convert the UTC struct to time_t as if it was UTC.
            // setenv("TZ", "UTC0", 1); tzset(); time_t t = mktime(&tm_utc); setenv("TZ", time_zone, 1); tzset();
            // This is safe if we restore it.
            
            char oldTZ[64];
            strcpy(oldTZ, getenv("TZ"));
            setenv("TZ", "UTC0", 1);
            tzset();
            time_t t_utc = mktime(&tm_utc);
            
            // Restore TZ
            setenv("TZ", oldTZ, 1);
            tzset();
            
            // Now convert t_utc to local time
            struct tm *tm_local = localtime(&t_utc);
            
            // Check if this forecast is for "today" (the day we are currently in)
            if (tm_local->tm_mday == currentDay) {
                int hour = tm_local->tm_hour;
                if (hour >= 0 && hour < 24) {
                    hourlyData[hour].hour = hour;
                    hourlyData[hour].temp = f["temperature"]["degrees"];
                    hourlyData[hour].rainProb = f["precipitation"]["probability"]["percent"];
                    // actualTemp is left as -100.0
                }
            }
        }
        Serial.println("Hourly data updated (Midnight to Midnight).");
      }
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}

void getHistoryData(int hoursCount) {
  if (WiFi.status() == WL_CONNECTED) {
    // Request hoursCount hours of history to cover the current day so far
    String url = "https://weather.googleapis.com/v1/history/hours:lookup?key=" + String(GOOGLE_API_KEY) 
      + "&location.latitude=" + String(LATITUDE) 
      + "&location.longitude=" + String(LONGITUDE) 
      + "&hours=" + String(hoursCount)
      + "&unitsSystem=METRIC";
    
    Serial.println("Requesting History URL: " + url);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, url);
    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
      String payload = http.getString();
      Serial.println("HTTP Response code: " + String(httpResponseCode));
      
      JsonDocument filter;
      filter["historyHours"][0]["interval"]["startTime"] = true;
      filter["historyHours"][0]["temperature"]["degrees"] = true;

      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));

      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
      } else {
        // Get current day to filter history
        struct tm timeinfo;
        if(!getLocalTime(&timeinfo, 1000)){
            Serial.println("Failed to obtain time for history filtering");
        }
        int currentDay = timeinfo.tm_mday;

        JsonArray history = doc["historyHours"];
        for(int i=0; i<history.size(); i++) {
            JsonObject h_data = history[i];
            String timeStr = h_data["interval"]["startTime"].as<String>();
            
            // Parse ISO string "2026-01-03T17:00:00Z"
            int y, M, d, h, m, s;
            sscanf(timeStr.c_str(), "%d-%d-%dT%d:%d:%dZ", &y, &M, &d, &h, &m, &s);
            
            struct tm tm_utc = {0};
            tm_utc.tm_year = y - 1900;
            tm_utc.tm_mon = M - 1;
            tm_utc.tm_mday = d;
            tm_utc.tm_hour = h;
            tm_utc.tm_min = m;
            tm_utc.tm_sec = s;
            tm_utc.tm_isdst = 0;
            
            // Convert UTC to local time
            char oldTZ[64];
            strcpy(oldTZ, getenv("TZ"));
            setenv("TZ", "UTC0", 1);
            tzset();
            time_t t_utc = mktime(&tm_utc);
            setenv("TZ", oldTZ, 1);
            tzset();
            
            struct tm *tm_local = localtime(&t_utc);
            
            // Check if this history point is for "today"
            if (tm_local->tm_mday == currentDay) {
                int hour = tm_local->tm_hour;
                if (hour >= 0 && hour < 24) {
                    hourlyData[hour].actualTemp = h_data["temperature"]["degrees"];
                }
            }
        }
        Serial.println("History data updated.");
      }
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}

void getWeatherCurrentData(){
    if (WiFi.status() == WL_CONNECTED) {
    String url = "https://weather.googleapis.com/v1/currentConditions:lookup?key=" + String(GOOGLE_API_KEY) 
      + "&location.latitude=" + String(LATITUDE) 
      + "&location.longitude=" + String(LONGITUDE)
      + "&unitsSystem=METRIC";
    
    Serial.println("Requesting URL: " + url);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, url);
    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
      String payload = http.getString();
      Serial.println("HTTP Response code: " + String(httpResponseCode));
      //Serial.println("Payload: " + payload);

      JsonDocument filter;
      filter["weatherCondition"]["description"]["text"] = true;
      filter["weatherCondition"]["iconBaseUri"] = true;
      filter["temperature"]["degrees"] = true;
      filter["feelsLikeTemperature"]["degrees"] = true;
      filter["wind"]["speed"]["value"] = true;
      filter["wind"]["gust"]["value"] = true;
      filter["wind"]["direction"]["degrees"] = true;
      filter["relativeHumidity"] = true;
      filter["precipitation"]["probability"]["percent"] = true;
      filter["uvIndex"] = true;
      filter["airPressure"]["meanSeaLevelMillibars"] = true;

      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));

      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
      } else {
        JsonObject newWeather = doc.as<JsonObject>();
        updateCurrentWeather(newWeather);
      }
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
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
