#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "secrets.h"
#include "Display.h"

// Forward declaration
void ListWifiAPs();

WeatherData currentWeather;
DailyForecast dailyForecasts[3];
HourlyData hourlyData[24];

Display displayHandler;

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
      Serial.println("Daily Forecast Payload: " + payload);
      
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

void getHourlyForecastData() {
  if (WiFi.status() == WL_CONNECTED) {
    String url = "https://weather.googleapis.com/v1/forecast/hours:lookup?key=" + String(GOOGLE_API_KEY) 
      + "&location.latitude=" + String(LATITUDE) 
      + "&location.longitude=" + String(LONGITUDE) 
      + "&hours=24"
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
      // Serial.println("Hourly Forecast Payload: " + payload);
      /*
      Serial.println("Hourly Forecast Payload (Chunked):");
      int len = payload.length();
      for (int i = 0; i < len; i += 1000) {
        Serial.print(payload.substring(i, min(i + 1000, len)));
        delay(10); // Small delay to prevent buffer overflow
      }
      Serial.println();
      */
      
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
        JsonArray forecasts = doc["forecastHours"];
        for(int i=0; i<24 && i<forecasts.size(); i++) {
            JsonObject f = forecasts[i];
            String timeStr = f["interval"]["startTime"].as<String>();
            // Extract hour from ISO string (e.g. 2026-01-03T17:00:00Z)
            int tIndex = timeStr.indexOf('T');
            if(tIndex != -1) {
                hourlyData[i].hour = timeStr.substring(tIndex+1, tIndex+3).toInt();
            }
            
            hourlyData[i].temp = f["temperature"]["degrees"];
            hourlyData[i].rainProb = f["precipitation"]["probability"]["percent"];
        }
        Serial.println("Hourly data updated.");
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
      Serial.println("Payload: " + payload);

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


void getMOCKWeatherCurrentData() {
  // Mock data for testing to save API calls
  Serial.println("Using Mock Data for Current Weather");
  String payload = R"({
  "currentTime": "2026-01-02T18:04:58.953196344Z",
  "timeZone": {
    "id": "Australia/Melbourne"
  },
  "isDaytime": false,
  "weatherCondition": {
    "iconBaseUri": "https://maps.gstatic.com/weather/v1/partly_clear",
    "description": {
      "text": "Partly cloudy",
      "languageCode": "en"
    },
    "type": "PARTLY_CLOUDY"
  },
  "temperature": {
    "degrees": 13.7,
    "unit": "CELSIUS"
  },
  "feelsLikeTemperature": {
    "degrees": 13.7,
    "unit": "CELSIUS"
  },
  "dewPoint": {
    "degrees": 12.4,
    "unit": "CELSIUS"
  },
  "heatIndex": {
    "degrees": 13.7,
    "unit": "CELSIUS"
  },
  "windChill": {
    "degrees": 13.7,
    "unit": "CELSIUS"
  },
  "relativeHumidity": 92,
  "uvIndex": 0,
  "precipitation": {
    "probability": {
      "percent": 1,
      "type": "RAIN"
    },
    "snowQpf": {
      "quantity": 0,
      "unit": "MILLIMETERS"
    },
    "qpf": {
      "quantity": 0,
      "unit": "MILLIMETERS"
    }
  },
  "thunderstormProbability": 0,
  "airPressure": {
    "meanSeaLevelMillibars": 1010.37
  },
  "wind": {
    "direction": {
      "degrees": 290,
      "cardinal": "WEST_NORTHWEST"
    },
    "speed": {
      "value": 5,
      "unit": "KILOMETERS_PER_HOUR"
    },
    "gust": {
      "value": 8,
      "unit": "KILOMETERS_PER_HOUR"
    }
  },
  "visibility": {
    "distance": 15,
    "unit": "KILOMETERS"
  },
  "cloudCover": 38,
  "currentConditionsHistory": {
    "temperatureChange": {
      "degrees": -0.6,
      "unit": "CELSIUS"
    },
    "maxTemperature": {
      "degrees": 24.7,
      "unit": "CELSIUS"
    },
    "minTemperature": {
      "degrees": 12.4,
      "unit": "CELSIUS"
    },
    "snowQpf": {
      "quantity": 0,
      "unit": "MILLIMETERS"
    },
    "qpf": {
      "quantity": 0,
      "unit": "MILLIMETERS"
    }
  }
})";

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
}

void setup() {
  Serial.begin(115200);
  delay(10000);

  Serial.println();
  Serial.println("--- Test Start: Paged Rendering with Fonts ---");
  
  // Connect to WiFi and get weather data
  connectToWiFi();
  
  //getWeatherForcastData();
  getMOCKWeatherCurrentData();
  //initMockForecastData();
  
  getDailyForecastData();
  getHourlyForecastData();

  displayHandler.init();
  displayHandler.drawWeather(currentWeather, dailyForecasts, hourlyData);
}

void loop() {
}
