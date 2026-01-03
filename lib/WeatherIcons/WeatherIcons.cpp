#include "WeatherIcons.h"
#include <math.h>

WeatherIcons::WeatherIcons(Adafruit_GFX& display) : _display(display) {}

void WeatherIcons::drawThickLine(int x1, int y1, int x2, int y2, int width, uint16_t color) {
    // Simple implementation for thick lines
    if (width <= 1) {
        _display.drawLine(x1, y1, x2, y2, color);
        return;
    }
    
    // For purely vertical or horizontal lines, use fillRect
    if (x1 == x2) {
        _display.fillRect(x1 - width/2, min(y1, y2), width, abs(y2-y1), color);
        return;
    }
    if (y1 == y2) {
        _display.fillRect(min(x1, x2), y1 - width/2, abs(x2-x1), width, color);
        return;
    }

    // For diagonal lines, draw multiple lines with offsets
    // This is a naive implementation but sufficient for icon art
    for (int i = -width/2; i <= width/2; i++) {
        _display.drawLine(x1+i, y1, x2+i, y2, color);
        _display.drawLine(x1, y1+i, x2, y2+i, color);
    }
}

void WeatherIcons::drawSun(int x, int y, int radius, float scale) {
    // Draw spokes
    int numSpokes = 8;
    int spokeLen = radius / 2; // Scale spoke length with sun size
    int gap = 6 * scale;
    

    for (int i = 0; i < numSpokes; i++) {
        float angle = i * (2 * PI / numSpokes);
        int x1 = x + (radius + gap) * cos(angle);
        int y1 = y + (radius + gap) * sin(angle);
        int x2 = x + (radius + gap + spokeLen) * cos(angle);
        int y2 = y + (radius + gap + spokeLen) * sin(angle);
        
        drawThickLine(x1, y1, x2, y2, 5, WI_RED);
        // Draw spoke (Yellow)
        drawThickLine(x1, y1, x2, y2, 3, WI_YELLOW);
    }

    // Red outline (thick)
    _display.fillCircle(x, y, radius + 3, WI_RED);
    // Yellow fill
    _display.fillCircle(x, y, radius, WI_YELLOW);
}

void WeatherIcons::drawCloud(int x, int y, float scale) {
    // Simple cloud shape composed of circles
    // Python:
    // (x, y, 20 * scale),
    // (x + 15 * scale, y - 10 * scale, 25 * scale),
    // (x + 35 * scale, y, 20 * scale),
    // (x + 15 * scale, y + 5 * scale, 20 * scale)
    
    struct Circle { int cx; int cy; int r; };
    Circle circles[] = {
        {0, 0, (int)(20 * scale)},
        {(int)(15 * scale), (int)(-10 * scale), (int)(25 * scale)},
        {(int)(35 * scale), 0, (int)(20 * scale)},
        {(int)(15 * scale), (int)(5 * scale), (int)(20 * scale)}
    };
    
    // Draw black outline (larger circles)
    for (int i = 0; i < 4; i++) {
        _display.fillCircle(x + circles[i].cx, y + circles[i].cy, circles[i].r + 3, WI_BLUE);
    }
        
    // Draw white fill
    for (int i = 0; i < 4; i++) {
        _display.fillCircle(x + circles[i].cx, y + circles[i].cy, circles[i].r, WI_WHITE);
    }
}

void WeatherIcons::drawRain(int x, int y, float scale) {
    // Blue drops
    for (int i = 0; i < 3; i++) {
        int dx = x + i * 15 * scale;
        drawThickLine(dx, y, dx - 5 * scale, y + 15 * scale, 4, WI_BLUE);
    }
}

void WeatherIcons::drawSnow(int x, int y, float scale) {
    // Blue flakes
    for (int i = 0; i < 3; i++) {
        int dx = x + i * 15 * scale;
        int dy = y + 10 * scale;
        // Cross shape
        drawThickLine(dx-5*scale, dy, dx+5*scale, dy, 3, WI_BLUE);
        drawThickLine(dx, dy-5*scale, dx, dy+5*scale, 3, WI_BLUE);
    }
}

void WeatherIcons::drawLightning(int x, int y, float scale) {
    // Yellow bolt with Red outline
    // Points: (x, y), (x-15, y+25), (x+5, y+25), (x-5, y+50)
    
    int p1x = x, p1y = y;
    int p2x = x - 15 * scale, p2y = y + 25 * scale;
    int p3x = x + 5 * scale, p3y = y + 25 * scale;
    int p4x = x - 5 * scale, p4y = y + 50 * scale;

    // Outline (Red, width 6)
    drawThickLine(p1x, p1y, p2x, p2y, 6, WI_RED);
    drawThickLine(p2x, p2y, p3x, p3y, 6, WI_RED);
    drawThickLine(p3x, p3y, p4x, p4y, 6, WI_RED);

    // Fill (Yellow, width 2)
    drawThickLine(p1x, p1y, p2x, p2y, 2, WI_YELLOW);
    drawThickLine(p2x, p2y, p3x, p3y, 2, WI_YELLOW);
    drawThickLine(p3x, p3y, p4x, p4y, 2, WI_YELLOW);
}

void WeatherIcons::drawFog(int x, int y, float scale) {
    // Wavy lines
    for (int i = 0; i < 3; i++) {
        int base_y = y + i * 12 * scale;
        for (int px = 0; px < 55 * scale; px++) {
            // Sine wave approximation
            int py = base_y + 4 * scale * sin((px / scale) * 0.2);
            int thickness = 2;
            for(int t=0; t<thickness; t++) {
                 _display.drawPixel(x + px, py + t, WI_BLACK);
            }
        }
    }
}

void WeatherIcons::drawWind(int x, int y, float scale) {
    // Curling lines
    // Top line
    drawThickLine(x, y, x + 35 * scale, y, 3, WI_BLUE);
    
    // Hook 1
    // Draw concentric arcs for thickness
    // Use mask 6 (2 | 4) for Right Half (Top-Right + Bottom-Right)
    // This creates a ')' shape starting from the bottom (end of line) and curling up
    for (int i = -1; i <= 1; i++) {
        int r = (5 + i) * scale;
        if (r < 1) r = 1;
        _display.drawCircleHelper(x + 35 * scale, y - 5 * scale, r, 6, WI_BLUE); 
    }
    
    // Bottom line offset
    int y2 = y + 20 * scale;
    int x2 = x + 15 * scale;
    drawThickLine(x2, y2, x2 + 35 * scale, y2, 3, WI_BLUE);
    // Hook 2
    for (int i = -1; i <= 1; i++) {
        int r = (5 + i) * scale;
        if (r < 1) r = 1;
        _display.drawCircleHelper(x2 + 35 * scale, y2 - 5 * scale, r, 6, WI_BLUE);
    }
}

void WeatherIcons::drawWeatherIcon(String iconName, int x, int y, int iconSize) {
    // Normalize icon name
    iconName.toLowerCase();
    float s = iconSize / 94.0; // Base size is 94

    
    if (iconName == "clear_day" || iconName == "sunny" || iconName == "clear_night") {
        drawSun(x + 48 * s, y + 48 * s, 30 * s, s);
    }
    else if (iconName.indexOf("partly") != -1) {
        drawSun(x + 65 * s, y + 30 * s, 20 * s, s);
        drawCloud(x + 20 * s, y + 55 * s, 1.0 * s);
    }
    else if (iconName == "cloudy" || iconName == "mostly_cloudy") {
        drawCloud(x + 25 * s, y + 45 * s, 1.2 * s);
    }
    else if (iconName.indexOf("rain") != -1 || iconName.indexOf("showers") != -1) {
        drawCloud(x + 25 * s, y + 45 * s, 1.1 * s);
        drawRain(x + 27 * s, y + 77 * s, s);
    }
    else if (iconName.indexOf("snow") != -1 || iconName.indexOf("flurries") != -1) {
        drawCloud(x + 25 * s, y + 45 * s, 1.1 * s);
        drawSnow(x + 35 * s, y + 75 * s, s);
    }
    else if (iconName.indexOf("thunder") != -1 || iconName.indexOf("storm") != -1) {
        drawCloud(x + 25 * s, y + 45 * s, 1.1 * s);
        drawLightning(x + 48 * s, y + 75 * s, s);
    }
    else if (iconName == "fog" || iconName == "mist" || iconName == "haze") {
        drawFog(x + 20 * s, y + 35 * s, s);
    }
    else if (iconName.indexOf("wind") != -1) {
        drawWind(x + 20 * s, y + 40 * s, s);
    }
    else {
        // Default to sunny/clear if unknown
        drawSun(x + 48 * s, y + 48 * s, 30 * s, s);
    }
}
