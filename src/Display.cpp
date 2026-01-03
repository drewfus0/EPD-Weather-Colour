#include "Display.h"

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
    int rowH = h / 3;
    for (int i = 0; i < 3; i++) {
        int rowY = y + i * rowH;
        
        // Separator line
        if (i > 0) display.drawLine(x, rowY, x + w, rowY, GxEPD_BLACK);
        
        // Day Name
        RenderSecondaryValue(x + 10, rowY + 25, daily[i].dayName, 15);
        
        // Icon
        weatherIcons.drawWeatherIcon(daily[i].iconName, x + 10, rowY + 35, 64);
        
        // High / Low
        RenderPrimaryValue(x + 120, rowY + 55, String(daily[i].tempHigh, 1), 15);
        RenderSecondaryValue(x + 220, rowY + 55, String(daily[i].tempLow, 1), 20);

        // Description
        RenderSecondaryValue(x + 120, rowY + 85, daily[i].conditionText, 20);
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

    // 2. Auto-range with padding (+- 10)
    float targetMin = minVal - 10.0;
    float targetMax = maxVal + 10.0;
    float range = targetMax - targetMin;
    
    // 3. Calculate Tick Step (aim for 7-10 ticks)
    float rawStep = range / 8.0;
    int step = 1;
    if (rawStep > 5) step = 10;
    else if (rawStep > 2) step = 5;
    else if (rawStep > 1) step = 2;
    else step = 1;
    
    // 4. Align Min/Max to Step
    int minAxis = floor(targetMin / step) * step;
    int maxAxis = ceil(targetMax / step) * step;
    
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
    for (int t = minAxis; t <= maxAxis; t += step) {
        int py = originY - ((t - minAxis) * graphH / (maxAxis - minAxis));
        if (py >= y && py <= originY) {
            display.drawLine(originX - 5, py, originX, py, GxEPD_BLACK);
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
      int topH = 140;
      int w = 800;
      int colW = w / 5;
      int yCenter = topH / 2;

      // Draw separator line
      display.drawLine(0, topH, w, topH, GxEPD_YELLOW);
      display.drawLine(0, topH-1, w, topH-1, GxEPD_YELLOW);
      display.drawLine(0, topH+1, w, topH+1, GxEPD_YELLOW);

      // Col 1: Condition (Icon + Text)
      // Icon
      int iconX = colW * 0 + 20;
      int iconY = 20;
      weatherIcons.drawWeatherIcon(current.iconName, iconX, iconY);
      // Text
      RenderSecondaryValue(colW * 0 + 10, 130, String(current.conditionText), 12);

      // Col 2: Temp
      RenderTitleText(colW * 1 + 10, yCenter - 60, "Temperature");
      RenderPrimaryValue(colW * 1 + 10, yCenter - 20, String(current.temp) + " c");
      RenderTitleText(colW * 1 + 10, yCenter + 20, "Feels Like:");
      RenderSecondaryValue(colW * 1 + 10, yCenter + 50, String(current.feelsLike) + " c");

      // Col 3: Wind
      //RenderText(colW * 2 + 10, yCenter - 40, &FreeMonoBold12pt7b, GxEPD_BLACK, "Wind Info:");
      RenderTitleText(colW * 2 + 10, yCenter - 60, "Wind :");
      RenderPrimaryValue(colW * 2 + 10, yCenter - 20, String(current.windSpeed));
      RenderTitleText(colW * 2 + 10, yCenter + 20, "Gust:");
      RenderSecondaryValue(colW * 2 + 10, yCenter + 50, String(current.windGust));
      
      // Arrow
      //Convert the Arrow into an Triangle inside a circle radius 30
      drawWindDirection(colW * 2 + 120, yCenter + 30, 30, current.windDirection);

      // Col 4: Humidity & Rain
      RenderTitleText(colW * 3 + 10, yCenter - 60, "Humidity");
      RenderSecondaryValue(colW * 3 + 10, yCenter - 20, String(current.humidity) + "%");
      RenderTitleText(colW * 3 + 10, yCenter + 20, "Rain Prob:");
      RenderSecondaryValue(colW * 3 + 10, yCenter + 50, String(current.precipitationProbability) + "%");

      // Col 5: UV - pressure
      RenderTitleText(colW * 4 + 10, yCenter - 60, "UV Index:");
      RenderSecondaryValue(colW * 4 + 10, yCenter - 20, String(current.uvIndex));
      RenderTitleText(colW * 4 + 10, yCenter + 20, "Pressure:");
      RenderSecondaryValue(colW * 4 + 10, yCenter + 50, String(current.pressure) + " hPa");
      
      // bottom 2/3rds forcast / actuals
      int bottomY = 160;
      int bottomH = 320; // 480 - 160
      
      // Left 2/5ths = 320px
      drawDailyForecast(0, bottomY, 320, bottomH, daily);
      
      // Right 3/5ths = 480px
      drawGraphs(320, bottomY, 480, bottomH, hourly);

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
