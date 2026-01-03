#ifndef WEATHER_ICONS_H
#define WEATHER_ICONS_H

#include <Adafruit_GFX.h>
#include <Arduino.h>

// Colors matching GxEPD2_7C
#define WI_BLACK 0x0000
#define WI_WHITE 0xFFFF
#define WI_GREEN 0x07E0
#define WI_BLUE 0x001F
#define WI_RED 0xF800
#define WI_YELLOW 0xFFE0
#define WI_ORANGE 0xFD20 

class WeatherIcons {
public:
    WeatherIcons(Adafruit_GFX& display);
    void drawWeatherIcon(String iconName, int x, int y, int iconSize = 94);

private:
    Adafruit_GFX& _display;
    
    void drawSun(int x, int y, int radius, float scale);
    void drawCloud(int x, int y, float scale);
    void drawRain(int x, int y, float scale);
    void drawSnow(int x, int y, float scale);
    void drawLightning(int x, int y, float scale);
    void drawFog(int x, int y, float scale);
    void drawWind(int x, int y, float scale);
    
    // Helpers
    void drawThickLine(int x1, int y1, int x2, int y2, int width, uint16_t color);
};

#endif
