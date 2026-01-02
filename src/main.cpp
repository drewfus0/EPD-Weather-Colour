#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "secrets.h"
#include <epd7c/GxEPD2_730c_GDEP073E01.h>
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
// We use a smaller page height (HEIGHT / 4) to avoid RAM overflow on ESP32.
GxEPD2_7C<GxEPD2_730c_GDEP073E01, GxEPD2_730c_GDEP073E01::HEIGHT / 4> display(GxEPD2_730c_GDEP073E01(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

void connectToWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  int retry_count = 0;
  while (WiFi.status() != WL_CONNECTED && retry_count < 20) {
    delay(500);
    Serial.print(".");
    retry_count++;
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected! IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Failed to connect to WiFi. Check credentials in secrets.h");
  }
}

void getWeatherForcastData(int hours = 24) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://weather.googleapis.com/v1/forecast/hours:lookup?key=" + String(GOOGLE_API_KEY) 
      + "&location.latitude=" + String(LATITUDE) 
      + "&location.longitude=" + String(LONGITUDE) 
      + "&hours=" + String(hours)
      + "&unitsSystem=METRIC";
    
    String wf = getAPIData(url);
}

void getWeatherCurrentData() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://weather.googleapis.com/v1/forecast/currentConditions:lookup?key=" + String(GOOGLE_API_KEY) 
      + "&location.latitude=" + String(LATITUDE) 
      + "&location.longitude=" + String(LONGITUDE)
      + "&unitsSystem=METRIC";
    
    String wc = getAPIData(url);
  }
}

String getAPIData(string url) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    Serial.println("Requesting URL: " + url);
    
    http.begin(url);
    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
      String payload = http.getString();
      Serial.println("HTTP Response code: " + String(httpResponseCode));
      Serial.println("Payload size: " + String(payload.length()) + " bytes");
      Serial.println("Payload: " + payload);
      return payload;
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
  return NULL;
}
  
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println();
  Serial.println("--- Test Start: Paged Rendering with Fonts ---");

  // Connect to WiFi and get weather data
  connectToWiFi();
  getWeatherForcastData();
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
    
    // Draw text with fonts (like your weather display)
    display.setFont(&FreeMonoBold24pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(50, 100);
    display.println("Weather Test");
    
    display.setFont(&FreeSans18pt7b);
    display.setTextColor(GxEPD_RED);
    display.setCursor(50, 180);
    display.println("Paged Rendering");
    
    display.setTextColor(GxEPD_BLUE);
    display.setCursor(50, 260);
    display.println("With Custom Fonts");
    
    // Draw some shapes
    display.drawRect(10, 10, 780, 460, GxEPD_BLACK);
    display.fillCircle(400, 360, 40, GxEPD_GREEN);
    display.fillCircle(500, 360, 40, GxEPD_YELLOW);
    display.fillCircle(300, 360, 40, GxEPD_ORANGE);
  }
  while (display.nextPage());
  
  Serial.println("Paged rendering complete - display should show content");
}

void loop() {
}