#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <PNGdec.h>
#include "secrets.h"
#include <epd7c/GxEPD2_730c_GDEP073E01.h>

// Forward declaration
void ListWifiAPs();

// Icon handling
PNG png;
uint16_t* iconBuffer = NULL;
int iconWidth = 0;
int iconHeight = 0;

struct WeatherData {
  String conditionText;
  String iconUri;
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
#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeSans18pt7b.h>

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

uint16_t getClosestEpdColor(uint8_t r, uint8_t g, uint8_t b) {
  // Palette: Black, White, Green, Blue, Red, Yellow, Orange
  struct Color { uint8_t r, g, b; uint16_t code; };
  Color palette[] = {
    {0, 0, 0, GxEPD_BLACK},
    {255, 255, 255, GxEPD_WHITE},
    {0, 255, 0, GxEPD_GREEN},
    {0, 0, 255, GxEPD_BLUE},
    {255, 0, 0, GxEPD_RED},
    {255, 255, 0, GxEPD_YELLOW},
    {255, 165, 0, GxEPD_ORANGE}
  };
  
  uint16_t bestColor = GxEPD_WHITE;
  long minDist = 200000; // Large number
  
  for (int i = 0; i < 7; i++) {
    long dist = ((long)r - palette[i].r) * ((long)r - palette[i].r) +
                ((long)g - palette[i].g) * ((long)g - palette[i].g) +
                ((long)b - palette[i].b) * ((long)b - palette[i].b);
    if (dist < minDist) {
      minDist = dist;
      bestColor = palette[i].code;
    }
  }
  return bestColor;
}

int PNGDraw(PNGDRAW *pDraw) {
  // Serial.printf("PNGDraw: y=%d, w=%d\n", pDraw->y, pDraw->iWidth);
  uint16_t lineBuffer[pDraw->iWidth];
  
  // Convert to RGB565, blending with white background (0xFFFFFF)
  png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_LITTLE_ENDIAN, 0xFFFFFF);
  
  int y = pDraw->y;
  if (iconBuffer == NULL) return 0; // Abort if no buffer
  
  for (int x = 0; x < pDraw->iWidth; x++) {
     uint16_t color = lineBuffer[x];
     // RGB565 to RGB888 extraction
     uint8_t r = (color & 0xF800) >> 8;
     uint8_t g = (color & 0x07E0) >> 3;
     uint8_t b = (color & 0x001F) << 3;
     
     if (y < iconHeight && x < iconWidth) {
       iconBuffer[y * iconWidth + x] = getClosestEpdColor(r, g, b);
     }
  }
  return 1; // Return 1 to continue decoding
}

void downloadIcon(String url) {
  if (WiFi.status() != WL_CONNECTED) return;
  
  Serial.println("Downloading icon: " + url);
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, url);
  
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    int len = http.getSize();
    if (len > 0) {
      uint8_t *pngData = (uint8_t*)malloc(len);
      if (pngData) {
        WiFiClient *stream = http.getStreamPtr();
        // Read with a loop to ensure we get all data
        int totalRead = 0;
        while (totalRead < len && client.connected()) {
            int available = stream->available();
            if (available) {
                int read = stream->read(pngData + totalRead, len - totalRead);
                if (read > 0) totalRead += read;
            }
            delay(10);
        }
        
        if (totalRead == len) {
          int rc = png.openRAM(pngData, len, PNGDraw);
          if (rc == PNG_SUCCESS) {
            iconWidth = png.getWidth();
            iconHeight = png.getHeight();
            Serial.printf("Icon size: %dx%d\n", iconWidth, iconHeight);
            
            if (iconBuffer) free(iconBuffer);
            iconBuffer = (uint16_t*)malloc(iconWidth * iconHeight * sizeof(uint16_t));
            
            if (iconBuffer) {
              // Initialize buffer to White
              for(int i=0; i<iconWidth*iconHeight; i++) iconBuffer[i] = GxEPD_WHITE;
              
              rc = png.decode(NULL, 0);
              Serial.printf("Decode result: %d\n", rc);
            } else {
              Serial.println("Failed to allocate iconBuffer");
            }
          } else {
             Serial.printf("PNG openRAM failed: %d\n", rc);
          }
        } else {
            Serial.printf("Download incomplete: %d/%d\n", totalRead, len);
        }
        free(pngData);
      } else {
          Serial.println("Malloc failed for pngData");
      }
    }
  } else {
    Serial.printf("Icon download failed: %d\n", httpCode);
  }
  http.end();
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
  currentWeather.iconUri = hourly["weatherCondition"]["iconBaseUri"].as<String>() + ".png";
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
  Serial.println("Icon URI: " + currentWeather.iconUri);
  Serial.println("Temp: " + String(currentWeather.temp));
  Serial.println("Feels Like: " + String(currentWeather.feelsLike));
  Serial.println("Wind: " + String(currentWeather.windSpeed) + " km/h, Dir: " + String(currentWeather.windDirection));
  Serial.println("Humidity: " + String(currentWeather.humidity) + "%");
  Serial.println("Rain Prob: " + String(currentWeather.precipitationProbability) + "%");
  Serial.println("UV: " + String(currentWeather.uvIndex));
  Serial.println("Pressure: " + String(currentWeather.pressure));
  
  downloadIcon(currentWeather.iconUri);
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

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println();
  Serial.println("--- Test Start: Paged Rendering with Fonts ---");

  // Connect to WiFi and get weather data
  connectToWiFi();

  //getWeatherForcastData();
  getWeatherCurrentData();

  // Skip displaying for now while debugging api.
  //return;

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
      int colW = w / 6;
      int yCenter = topH / 2;

      // Draw separator line
      display.drawLine(0, topH, w, topH, GxEPD_BLACK);

      // Col 1: Condition (Icon + Text)
      // Icon
      if (iconBuffer) {
        int iconX = colW * 0 + 20;
        int iconY = 20;
        for (int y = 0; y < iconHeight; y++) {
          for (int x = 0; x < iconWidth; x++) {
            display.drawPixel(iconX + x, iconY + y, iconBuffer[y * iconWidth + x]);
          }
        }
      } else {
        display.drawRect(colW * 0 + 20, 20, 90, 90, GxEPD_BLACK);
        display.setCursor(colW * 0 + 30, 80);
        display.setFont(NULL);
        display.print("Icon");
      }
      
      // Text
      display.setCursor(colW * 0 + 5, 130);
      // Wrap text if too long? For now just print
      String cond = currentWeather.conditionText;
      if (cond.length() > 15) cond = cond.substring(0, 15);
      display.print(cond);

      // Col 2: Temp
      display.setFont(&FreeSans18pt7b);
      display.setTextColor(GxEPD_RED);
      display.setCursor(colW * 1 + 10, yCenter + 10);
      display.print(currentWeather.temp, 1); display.print("C");
      
      display.setFont(NULL);
      display.setTextColor(GxEPD_BLACK);
      display.setCursor(colW * 1 + 10, yCenter + 40);
      display.print("Feels: "); display.print(currentWeather.feelsLike, 1);

      // Col 3: Wind
      display.setCursor(colW * 2 + 10, yCenter - 20);
      display.print("Wind: "); display.print(currentWeather.windSpeed, 1);
      display.setCursor(colW * 2 + 10, yCenter);
      display.print("Gust: "); display.print(currentWeather.windGust, 1);
      
      // Arrow
      int cx = colW * 2 + 60;
      int cy = yCenter + 40;
      int r = 15;
      display.drawCircle(cx, cy, r, GxEPD_BLACK);
      float angle = (currentWeather.windDirection - 90) * PI / 180.0;
      display.drawLine(cx, cy, cx + r * cos(angle), cy + r * sin(angle), GxEPD_RED);

      // Col 4: Humidity & Rain
      display.setCursor(colW * 3 + 10, yCenter - 10);
      display.print("Hum: "); display.print(currentWeather.humidity); display.print("%");
      display.setCursor(colW * 3 + 10, yCenter + 20);
      display.print("Rain: "); display.print(currentWeather.precipitationProbability); display.print("%");

      // Col 5: UV
      display.setCursor(colW * 4 + 10, yCenter);
      display.print("UV Index: "); 
      display.setFont(&FreeSans18pt7b);
      display.print(currentWeather.uvIndex);

      // Col 6: Pressure
      display.setFont(NULL);
      display.setCursor(colW * 5 + 10, yCenter);
      display.print("Press:"); 
      display.setCursor(colW * 5 + 10, yCenter + 20);
      display.print(currentWeather.pressure); display.print("hPa");
      
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