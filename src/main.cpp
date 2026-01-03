#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "secrets.h"
#include <epd7c/GxEPD2_730c_GDEP073E01.h>
#include "WeatherIcons.h"

// Forward declaration
void ListWifiAPs();

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

WeatherData currentWeather;

#include <GxEPD2_7C.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans24pt7b.h>

#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMono12pt7b.h>
#include <Fonts/FreeMono18pt7b.h>
#include <Fonts/FreeMono24pt7b.h>

#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

// Pin definitions based on your wiring
#define EPD_BUSY 25
#define EPD_RST  26
#define EPD_DC   2
#define EPD_CS   14
#define EPD_SCK  18
#define EPD_MOSI 23

// Select the display class. 
// Waveshare 7.3inch e-Paper HAT (E) uses the GDEP073E01 (Spectra 6) panel.
// We should use a smaller page height (HEIGHT / 4) to avoid RAM overflow on ESP32.
// Reducing page height further to save RAM for other tasks
GxEPD2_7C<GxEPD2_730c_GDEP073E01, GxEPD2_730c_GDEP073E01::HEIGHT / 8> display(GxEPD2_730c_GDEP073E01(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));
WeatherIcons weatherIcons(display);

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

void getWeatherForcastData(int hours = 24) {
  if (WiFi.status() == WL_CONNECTED) {
    String url = "https://weather.googleapis.com/v1/forecast/hours:lookup?key=" + String(GOOGLE_API_KEY) 
      + "&location.latitude=" + String(LATITUDE) 
      + "&location.longitude=" + String(LONGITUDE) 
      + "&hours=" + String(hours)
      + "&unitsSystem=METRIC";
    
    Serial.println("Requesting URL: " + url);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, url);
    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
      Serial.println("HTTP Response code: " + String(httpResponseCode));
      Serial.println("Payload size: " + String(http.getSize()) + " bytes");
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}

// void getWeatherCurrentData_ForOnline(){
//     if (WiFi.status() == WL_CONNECTED) {
//     String url = "https://weather.googleapis.com/v1/currentConditions:lookup?key=" + String(GOOGLE_API_KEY) 
//       + "&location.latitude=" + String(LATITUDE) 
//       + "&location.longitude=" + String(LONGITUDE)
//       + "&unitsSystem=METRIC";
    
//     Serial.println("Requesting URL: " + url);

//     WiFiClientSecure client;
//     client.setInsecure();
//     HTTPClient http;
//     http.begin(client, url);
//     int httpResponseCode = http.GET();
    
//     if (httpResponseCode > 0) {
//       String payload = http.getString();
//       Serial.println("HTTP Response code: " + String(httpResponseCode));
//       Serial.println("Payload: " + payload);

//       JsonDocument filter;
//       filter["weatherCondition"]["description"]["text"] = true;
//       filter["weatherCondition"]["iconBaseUri"] = true;
//       filter["temperature"]["degrees"] = true;
//       filter["feelsLikeTemperature"]["degrees"] = true;
//       filter["wind"]["speed"]["value"] = true;
//       filter["wind"]["gust"]["value"] = true;
//       filter["wind"]["direction"]["degrees"] = true;
//       filter["relativeHumidity"] = true;
//       filter["precipitation"]["probability"]["percent"] = true;
//       filter["uvIndex"] = true;
//       filter["airPressure"]["meanSeaLevelMillibars"] = true;

//       JsonDocument doc;
//       DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));

//       if (error) {
//         Serial.print("deserializeJson() failed: ");
//         Serial.println(error.c_str());
//       } else {
//         JsonObject newWeather = doc.as<JsonObject>();
//         updateCurrentWeather(newWeather);
//       }
//     } else {
//       Serial.print("Error code: ");
//       Serial.println(httpResponseCode);
//     }
//     http.end();
//   } else {
//     Serial.println("WiFi Disconnected");
//   }
// }


void getWeatherCurrentData() {
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

void RenderText(int16_t x, int16_t y, const GFXfont *font, uint16_t color, String text, int maxCharsPerLine = 12) {
  display.setFont(font);
  display.setTextColor(color);
  display.setCursor(x, y);

  int currentLineLen = 0;
  int start = 0;
  int end = text.indexOf(' ');

  while (end != -1) {
    String word = text.substring(start, end);
    
    // Check if adding this word exceeds the line limit
    // We only wrap if it's not the first word on the line
    if (currentLineLen + word.length() > maxCharsPerLine && currentLineLen > 0) {
      display.println();
      currentLineLen = 0;
    }
    
    // Add space before word if it's not the start of a line
    if (currentLineLen > 0) {
      display.print(" ");
      currentLineLen++;
    }
    
    display.print(word);
    currentLineLen += word.length();
    
    start = end + 1;
    end = text.indexOf(' ', start);
  }
  
  // Process the remaining part of the string (last word)
  String lastWord = text.substring(start);
  if (lastWord.length() > 0) {
    if (currentLineLen + lastWord.length() > maxCharsPerLine && currentLineLen > 0) {
      display.println();
    } else if (currentLineLen > 0) {
      display.print(" ");
    }
    display.print(lastWord);
  }
}

void RenderTitleText(int16_t x, int16_t y, String text) {
  RenderText(x, y, &FreeMono9pt7b, GxEPD_BLACK, text, 15); // Assume title max 20 chars per line
}

void RenderPrimaryValue(int16_t x, int16_t y, String text) {
  RenderText(x, y, &FreeSansBold18pt7b, GxEPD_BLUE, text, 15); // Assume subtitle max 30 chars per line
}

void RenderSecondaryValue(int16_t x, int16_t y, String text) {
  RenderText(x, y, &FreeSansBold12pt7b, GxEPD_BLACK, text, 20); // Assume subtitle max 30 chars per line
}

void drawWindDirection(int colW, int yCenter)
{
  int cx = colW * 2 + 70;
  int cy = yCenter + 80;
  int r = 30;
  display.drawCircle(cx, cy, r, GxEPD_BLACK);

  float angle = (currentWeather.windDirection - 90) * PI / 180.0;

  // Calculate triangle vertices
  // Tip
  int x1 = cx + (r - 6) * cos(angle);
  int y1 = cy + (r - 6) * sin(angle);

  // Back Left (offset by ~150 degrees)
  int x2 = cx + (r - 6) * cos(angle + 2.6);
  int y2 = cy + (r - 6) * sin(angle + 2.6);

  // Back Right (offset by ~-150 degrees)
  int x3 = cx + (r - 6) * cos(angle - 2.6);
  int y3 = cy + (r - 6) * sin(angle - 2.6);

  display.fillTriangle(x1, y1, x2, y2, x3, y3, GxEPD_RED);
}

void setup() {
  Serial.begin(115200);
  delay(10000);
  Serial.println();
  
  Serial.println("--- Test Start: Paged Rendering with Fonts ---");
  
  // Connect to WiFi and get weather data
  connectToWiFi();

  //getWeatherForcastData();
  getWeatherCurrentData();

  // Initialize display
  Serial.println("Initializing display...");
  display.init(115200, true, 2, false);
  display.setRotation(0);
  display.setFullWindow();
  
  // Use paged drawing mode (like your weather display)
  Serial.println("Starting paged rendering...");
  display.firstPage();
  int page = 0;
  do
  {
    Serial.print("Rendering Page: "); Serial.println(page++);
    
    // Fill with white
    display.fillScreen(GxEPD_WHITE);
    
    if (currentWeather.valid) {
      int topH = 160;
      int w = 800;
      int colW = w / 5;
      int yCenter = topH / 2;

      // Draw separator line
      display.drawLine(0, topH, w, topH, GxEPD_BLACK);

      // Col 1: Condition (Icon + Text)
      // Icon
      int iconX = colW * 0 + 20;
      int iconY = 20;
      weatherIcons.drawWeatherIcon(currentWeather.iconName, iconX, iconY);
      
      // Text
      display.setCursor(colW * 0 + 10, 130);
      // Wrap text if too long? For now just print
      String cond = currentWeather.conditionText;
      if (cond.length() > 15) cond = cond.substring(0, 15);
      display.print(cond);

      // Col 2: Temp
      RenderTitleText(colW * 1 + 10, yCenter - 60, "Temperature");
      RenderPrimaryValue(colW * 1 + 10, yCenter - 20, String(currentWeather.temp) + " c");
      RenderTitleText(colW * 1 + 10, yCenter + 20, "Feels Like:");
      RenderSecondaryValue(colW * 1 + 10, yCenter + 50, String(currentWeather.feelsLike) + " c");

      // Col 3: Wind
      //RenderText(colW * 2 + 10, yCenter - 40, &FreeMonoBold12pt7b, GxEPD_BLACK, "Wind Info:");
      RenderTitleText(colW * 2 + 10, yCenter - 60, "Wind :");
      RenderPrimaryValue(colW * 2 + 10, yCenter - 20, String(currentWeather.windSpeed) + " km/h");
      RenderTitleText(colW * 2 + 10, yCenter + 20, "Gust:");
      RenderSecondaryValue(colW * 2 + 10, yCenter + 30, String(currentWeather.windGust));
      
      // Arrow
      //Convert the Arrow into an Triangle inside a circle radius 30
      drawWindDirection(colW, yCenter);

      // Col 4: Humidity & Rain
      RenderTitleText(colW * 3 + 10, yCenter - 60, "Humidity");
      RenderSecondaryValue(colW * 3 + 10, yCenter - 20, String(currentWeather.humidity) + "%");
      RenderTitleText(colW * 3 + 10, yCenter + 20, "Rain Prob:");
      RenderSecondaryValue(colW * 3 + 10, yCenter + 50, String(currentWeather.precipitationProbability) + "%");

      // Col 5: UV - pressure
      RenderTitleText(colW * 4 + 10, yCenter - 60, "UV Index:");
      RenderSecondaryValue(colW * 4 + 10, yCenter - 20, String(currentWeather.uvIndex));
      RenderTitleText(colW * 4 + 10, yCenter + 20, "Pressure:");
      RenderSecondaryValue(colW * 4 + 10, yCenter + 50, String(currentWeather.pressure) + " hPa");
      
      // --- Test Render All Icons (Bottom 3rd) ---
      int testY = 350;
      int testX = 10;
      int spacing = 100;
      
      const char* testIcons[] = {
        "sunny", "partly_cloudy", "cloudy", "rain", 
        "snow", "thunderstorm", "fog", "wind"
      };
      
      for(int i=0; i<8; i++) {
          weatherIcons.drawWeatherIcon(testIcons[i], testX + i*spacing, testY);
          // Optional: Label
          display.setCursor(testX + i*spacing, testY + 100);
          display.print(testIcons[i]);
      }
      
    } else {
      display.setFont(&FreeMonoBold24pt7b);
      display.setTextColor(GxEPD_BLACK);
      display.setCursor(50, 100);
      display.println("No Weather Data");
    }
  }
  while (display.nextPage());
  
  Serial.println("Paged rendering complete - display should show content");
}



void loop() {
}