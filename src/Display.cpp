#include "Display.h"
#include <time.h>

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

Display::Display() 
    : display(GxEPD2_730c_GDEP073E01(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)),
      weatherIcons(display) 
{
}

void Display::init() {
    Serial.println("Initializing display...");
    // Increase reset duration to 10ms to ensure display wakes up properly
    display.init(115200, true, 10, false);
    delay(100); // Give it a moment to settle
    display.setRotation(0);
    display.setFullWindow();
}

void Display::RenderText(int16_t x, int16_t y, const GFXfont *font, uint16_t color, String text, int maxCharsPerLine) {
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

void Display::RenderTitleText(int16_t x, int16_t y, String text, int16_t maxCharsPerLine) {
  RenderText(x, y, &FreeMono9pt7b, GxEPD_BLACK, text, maxCharsPerLine); // Assume title max 20 chars per line
}

void Display::RenderPrimaryValue(int16_t x, int16_t y, String text, int16_t maxCharsPerLine) {
  RenderText(x, y, &FreeSansBold18pt7b, GxEPD_BLUE, text, maxCharsPerLine); // Assume subtitle max 30 chars per line
}

void Display::RenderSecondaryValue(int16_t x, int16_t y, String text, int16_t maxCharsPerLine) {
  RenderText(x, y, &FreeSansBold12pt7b, GxEPD_BLACK, text, maxCharsPerLine); // Assume subtitle max 30 chars per line
}

void Display::drawWindDirection(int cx, int cy, int r, float WindDirection)
{
  display.drawCircle(cx, cy, r, GxEPD_BLACK);
  float angle = (WindDirection - 90) * PI / 180.0;

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

void Display::drawDailyForecast(int x, int y, int w, int h, const DailyForecast daily[]) {
    int colW = w / 5;
    for (int i = 0; i < 5; i++) {
        int colX = x + i * colW;
        
        // Separator line
        if (i > 0) display.drawLine(colX, y, colX, y + h, GxEPD_BLACK);
        
        // Day Name
        RenderSecondaryValue(colX + 10, y + 25, daily[i].dayName, 12);
        
        // Icon
        weatherIcons.drawWeatherIcon(daily[i].iconName, colX + (colW-64)/2, y + 35, 64);
        
        // High / Low
        String tempStr = String(daily[i].tempHigh, 0) + " / " + String(daily[i].tempLow, 0);
        RenderSecondaryValue(colX + (colW/2) - 30, y + 110, tempStr, 15);
    }
}

void Display::drawGraphs(int x, int y, int w, int h, const HourlyData hourly[]) {
    // Margins
    int marginLeft = 40;
    int marginBottom = 30;
    int marginRight = 45; 
    int graphW = w - marginLeft - marginRight;
    int graphH = h - marginBottom - 10;
    int originX = x + marginLeft;
    int originY = y + graphH; // Bottom of graph area
    
    // 1. Determine Min/Max Temp
    float minVal = 100.0;
    float maxVal = -100.0;
    bool foundData = false;
    
    for (int i = 0; i < 24; i++) {
        if (hourly[i].temp > -99.0) {
            if (hourly[i].temp < minVal) minVal = hourly[i].temp;
            if (hourly[i].temp > maxVal) maxVal = hourly[i].temp;
            foundData = true;
        }
        if (hourly[i].actualTemp > -99.0) {
            if (hourly[i].actualTemp < minVal) minVal = hourly[i].actualTemp;
            if (hourly[i].actualTemp > maxVal) maxVal = hourly[i].actualTemp;
            foundData = true;
        }
    }
    
    if (!foundData) {
        minVal = 0;
        maxVal = 30;
    }

    // 2. Auto-range with padding (+- 5)
    float targetMin = minVal - 5.0;
    float targetMax = maxVal + 5.0;
    float range = targetMax - targetMin;
    
    // 3. Calculate Tick Step (aim for 7-10 ticks)
    float rawStep = range / 8.0;
    int step = 1;
    if (rawStep > 5) step = 10;
    else if (rawStep > 2) step = 5;
    else if (rawStep > 1) step = 2;
    else step = 1;
    
    // 4. Use exact range (+-5) for axis bounds, don't snap to tick steps
    float minAxis = targetMin;
    float maxAxis = targetMax;
    
    // Draw Axes
    display.drawLine(originX, y, originX, originY, GxEPD_BLACK); // Left Y axis (Temp)
    display.drawLine(originX + graphW, y, originX + graphW, originY, GxEPD_BLACK); // Right Y axis (Rain)
    display.drawLine(originX, originY, originX + graphW, originY, GxEPD_BLACK); // X axis
    
    // X Axis Labels (Hours)
    display.setFont(&FreeMono9pt7b);
    display.setTextColor(GxEPD_BLACK);
    for (int i = 0; i < 24; i += 3) {
        int px = originX + (i * graphW / 24) + (graphW / 48);
        display.drawLine(px, originY, px, originY + 5, GxEPD_BLACK);
        display.setCursor(px - 10, originY + 20);
        display.print(String(i));
    }
    
    // Y Axis Labels (Temp) - Left side
    // Start from the first multiple of 'step' that is >= minAxis
    int startTick = ceil(minAxis / step) * step;
    for (int t = startTick; t <= maxAxis; t += step) {
        int py = originY - ((t - minAxis) * graphH / (maxAxis - minAxis));
        if (py >= y && py <= originY) {
            display.drawLine(originX - 5, py, originX, py, GxEPD_BLACK);
            // Grid line
            display.drawLine(originX, py, originX + graphW, py, GxEPD_YELLOW);
            display.setCursor(originX - 35, py + 5);
            display.print(String(t));
        }
    }

    // Y Axis Labels (Rain Prob) - Right side
    int maxRain = 100;
    for (int r = 0; r <= maxRain; r += 10) {
        int py = originY - (r * graphH / maxRain);
        display.drawLine(originX + graphW, py, originX + graphW + 5, py, GxEPD_BLACK);
        if (r % 20 == 0) {
            display.setCursor(originX + graphW + 8, py + 5);
            display.print(String(r));
        }
    }
    
    // Plot Rain Probability (Bars) - Blue
    for (int i = 0; i < 24; i++) {
        if (hourly[i].rainProb >= 0) {
            int barH = (hourly[i].rainProb * graphH) / 100;
            int barW = (graphW / 24) - 2;
            int px = originX + (i * graphW / 24) + 1;
            int py = originY - barH;
            display.fillRect(px, py, barW, barH, GxEPD_BLUE);
        }
    }

    // Plot Actual Rain (Bars) - Orange
    for (int i = 0; i < 24; i++) {
        if (hourly[i].actualRain >= 0) {
            // Scale: 1mm = 10% of graph height? Or 1mm = 1 unit on 0-100 scale?
            // Let's use 1mm = 1 unit on 0-100 scale for now.
            int barH = (hourly[i].actualRain * graphH) / 100;
            if (barH > graphH) barH = graphH; // Clamp
            
            int barW = (graphW / 24) - 2;
            int px = originX + (i * graphW / 24) + 1;
            int py = originY - barH;
            display.fillRect(px, py, barW, barH, GxEPD_ORANGE);
        }
    }
    
    // Plot Temperature (Line) - Red
    int prevX = -1, prevY = -1;
    for (int i = 0; i < 24; i++) {
        if (hourly[i].temp > -99.0) {
            int px = originX + (i * graphW / 24) + (graphW / 48);
            int py = originY - ((hourly[i].temp - minAxis) * graphH / (maxAxis - minAxis));
            
            if (prevX != -1) {
                display.drawLine(prevX, prevY, px, py, GxEPD_RED);
                display.drawLine(prevX, prevY-1, px, py-1, GxEPD_RED);
                display.drawLine(prevX, prevY+1, px, py+1, GxEPD_RED);
            }
            prevX = px;
            prevY = py;
        } else {
            prevX = -1;
        }
    }
    
    // Plot Actual Temperature (Line) - Green
    prevX = -1; prevY = -1;
    for (int i = 0; i < 24; i++) {
        if (hourly[i].actualTemp > -99.0) {
            int px = originX + (i * graphW / 24) + (graphW / 48);
            int py = originY - ((hourly[i].actualTemp - minAxis) * graphH / (maxAxis - minAxis));
            
            if (prevX != -1) {
                display.drawLine(prevX, prevY, px, py, GxEPD_GREEN);
                display.drawLine(prevX, prevY-1, px, py-1, GxEPD_GREEN);
                display.drawLine(prevX, prevY+1, px, py+1, GxEPD_GREEN);
            }
            prevX = px;
            prevY = py;
        } else {
            prevX = -1;
        }
    }

    // Find Min/Max for Forecast (Red)
    float minF = 100.0, maxF = -100.0;
    int minF_idx = -1, maxF_idx = -1;
    
    for(int i=0; i<24; i++) {
        if(hourly[i].temp > -99.0) {
            if(hourly[i].temp < minF) { minF = hourly[i].temp; minF_idx = i; }
            if(hourly[i].temp > maxF) { maxF = hourly[i].temp; maxF_idx = i; }
        }
    }
    
    // Find Min/Max for History (Green)
    float minH = 100.0, maxH = -100.0;
    int minH_idx = -1, maxH_idx = -1;
    
    for(int i=0; i<24; i++) {
        if(hourly[i].actualTemp > -99.0) {
            if(hourly[i].actualTemp < minH) { minH = hourly[i].actualTemp; minH_idx = i; }
            if(hourly[i].actualTemp > maxH) { maxH = hourly[i].actualTemp; maxH_idx = i; }
        }
    }
    
    display.setFont(&FreeSansBold9pt7b);
    display.setTextColor(GxEPD_BLACK);

    // Draw Forecast Min/Max
    if (minF_idx != -1) {
        int px = originX + (minF_idx * graphW / 24) + (graphW / 48);
        int py = originY - ((minF - minAxis) * graphH / (maxAxis - minAxis));
        display.fillCircle(px, py, 3, GxEPD_RED);
        display.setCursor(px - 10, py + 15);
        display.print(String(minF, 1));
    }
    if (maxF_idx != -1 && maxF_idx != minF_idx) {
        int px = originX + (maxF_idx * graphW / 24) + (graphW / 48);
        int py = originY - ((maxF - minAxis) * graphH / (maxAxis - minAxis));
        display.fillCircle(px, py, 3, GxEPD_RED);
        display.setCursor(px - 10, py - 8);
        display.print(String(maxF, 1));
    }

    // Draw History Min/Max
    if (minH_idx != -1) {
        int px = originX + (minH_idx * graphW / 24) + (graphW / 48);
        int py = originY - ((minH - minAxis) * graphH / (maxAxis - minAxis));
        display.fillCircle(px, py, 3, GxEPD_GREEN);
        display.setCursor(px - 10, py + 15);
        display.print(String(minH, 1));
    }
    if (maxH_idx != -1 && maxH_idx != minH_idx) {
        int px = originX + (maxH_idx * graphW / 24) + (graphW / 48);
        int py = originY - ((maxH - minAxis) * graphH / (maxAxis - minAxis));
        display.fillCircle(px, py, 3, GxEPD_GREEN);
        display.setCursor(px - 10, py - 8);
        display.print(String(maxH, 1));
    }
}

void Display::drawWeather(const WeatherData& current, const DailyForecast daily[], const HourlyData hourly[]) {
  // Use paged drawing mode (like your weather display)
  Serial.println("Starting paged rendering...");
  display.firstPage();
  int page = 0;
  do
  {
    Serial.print("Rendering Page: "); Serial.println(page++);
    
    // Fill with white
    display.fillScreen(GxEPD_WHITE);
    
    if (current.valid) {
      int headerH = 30;
      int bottomH = 130;
      int mainH = 480 - headerH - bottomH; // 320
      int mainY = headerH;
      int bottomY = 480 - bottomH;
      int w = 800;

      // Draw Header (Date/Time)
      struct tm timeinfo;
      if(getLocalTime(&timeinfo)){
        char timeStr[64];
        strftime(timeStr, sizeof(timeStr), "%A %d %B %H:%M", &timeinfo);
        
        display.setFont(&FreeSansBold12pt7b);
        display.setTextColor(GxEPD_BLACK);
        int16_t tbx, tby; uint16_t tbw, tbh;
        display.getTextBounds(timeStr, 0, 0, &tbx, &tby, &tbw, &tbh);
        display.setCursor((w - tbw) / 2, headerH - 6);
        display.print(timeStr);
      }
      
      // Header separator
      display.drawLine(0, headerH, w, headerH, GxEPD_BLACK);

      // --- Left Column: Current Weather (1/3 width = ~266px) ---
      int leftW = 266;
      int centerX = leftW / 2;
      
      // Icon
      weatherIcons.drawWeatherIcon(current.iconName, centerX - 32, mainY + 10);
      
      // Condition Text
      int textY = mainY + 90;
      RenderSecondaryValue(10, textY, current.conditionText, 20);
      
      // Temp
      RenderPrimaryValue(10, textY + 40, String(current.temp, 1) + " C");
      RenderSecondaryValue(10, textY + 70, "Feels: " + String(current.feelsLike, 1), 20);
      
      // Wind
      RenderSecondaryValue(10, textY + 100, "Wind: " + String(current.windSpeed, 1) + " km/h", 20);
      drawWindDirection(220, textY + 95, 20, current.windDirection);
      
      // Humidity / Rain
      RenderSecondaryValue(10, textY + 130, "Hum: " + String(current.humidity) + "%", 20);
      RenderSecondaryValue(10, textY + 160, "Rain: " + String(current.precipitationProbability) + "%", 20);
      
      // UV / Pressure
      RenderSecondaryValue(10, textY + 190, "UV: " + String(current.uvIndex), 20);
      RenderSecondaryValue(10, textY + 220, "Press: " + String(current.pressure), 20);

      // Vertical Separator
      display.drawLine(leftW, mainY, leftW, bottomY, GxEPD_BLACK);

      // --- Right Column: Graph (2/3 width = ~534px) ---
      drawGraphs(leftW, mainY, w - leftW, mainH, hourly);
      
      // Horizontal Separator
      display.drawLine(0, bottomY, w, bottomY, GxEPD_BLACK);
      
      // --- Bottom Row: Daily Forecast ---
      drawDailyForecast(0, bottomY, w, bottomH, daily);

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
