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

void WeatherIcons::drawSun(int x, int y, int radius) {
    // Draw spokes
    int numSpokes = 8;
    int spokeLen = radius / 2; // Scale spoke length with sun size
    int gap = 6;
    
    for (int i = 0; i < numSpokes; i++) {
        float angle = i * (2 * PI / numSpokes);
        int x1 = x + (radius + gap) * cos(angle);
        int y1 = y + (radius + gap) * sin(angle);
        int x2 = x + (radius + gap + spokeLen) * cos(angle);
        int y2 = y + (radius + gap + spokeLen) * sin(angle);
        
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

void WeatherIcons::drawRain(int x, int y) {
    // Blue drops
    for (int i = 0; i < 3; i++) {
        int dx = x + i * 15;
        drawThickLine(dx, y, dx - 5, y + 15, 4, WI_BLUE);
    }
}

void WeatherIcons::drawSnow(int x, int y) {
    // Blue flakes
    for (int i = 0; i < 3; i++) {
        int dx = x + i * 15;
        int dy = y + 10;
        // Cross shape
        drawThickLine(dx-5, dy, dx+5, dy, 3, WI_BLUE);
        drawThickLine(dx, dy-5, dx, dy+5, 3, WI_BLUE);
    }
}

void WeatherIcons::drawLightning(int x, int y) {
    // Yellow bolt with Red outline
    // Points: (x, y), (x-15, y+25), (x+5, y+25), (x-5, y+50)
    
    int p1x = x, p1y = y;
    int p2x = x-15, p2y = y+25;
    int p3x = x+5, p3y = y+25;
    int p4x = x-5, p4y = y+50;

    // Outline (Red, width 6)
    drawThickLine(p1x, p1y, p2x, p2y, 6, WI_RED);
    drawThickLine(p2x, p2y, p3x, p3y, 6, WI_RED);
    drawThickLine(p3x, p3y, p4x, p4y, 6, WI_RED);

    // Fill (Yellow, width 2)
    drawThickLine(p1x, p1y, p2x, p2y, 2, WI_YELLOW);
    drawThickLine(p2x, p2y, p3x, p3y, 2, WI_YELLOW);
    drawThickLine(p3x, p3y, p4x, p4y, 2, WI_YELLOW);
}

void WeatherIcons::drawFog(int x, int y) {
    // Wavy lines
    for (int i = 0; i < 3; i++) {
        int base_y = y + i * 12;
        for (int px = 0; px < 55; px++) {
            // Sine wave approximation
            int py = base_y + 4 * sin(px * 0.2);
            _display.drawPixel(x + px, py, WI_BLACK);
            _display.drawPixel(x + px, py+1, WI_BLACK); // Thickness 2
        }
    }
}

void WeatherIcons::drawWind(int x, int y) {
    // Curling lines
    // Top line
    drawThickLine(x, y, x+35, y, 3, WI_BLUE);
    // Curl back: Arc from (x+35, y) curling up and back
    // Approximate arc with lines
    // Center of curl roughly at x+35, y-10?
    // Python: draw.arc([x+25, y-10, x+45, y+10], start=270, end=100, fill=BLUE, width=3)
    // Bounding box 20x20. Center (x+35, y). Radius 10.
    // Start 270 (Top) -> End 100 (Right-ish)
    
    int cx = x + 35;
    int cy = y;
    int r = 10;
    
    for (int angle = 270; angle >= 100; angle -= 10) { // Python angles are clockwise? No, standard math is CCW. PIL is clockwise from 3 o'clock?
        // PIL arc: start/end in degrees, 0 is 3 o'clock, clockwise.
        // 270 is 6 o'clock (bottom). 100 is slightly past 6 o'clock?
        // Wait, PIL 0 is 3 o'clock. 90 is 6 o'clock. 270 is 12 o'clock (top).
        // So 270 to 100 (crossing 0/360) is Top -> Right -> Bottom -> Leftish.
        // Let's just draw a simple hook.
        
        float rad = angle * PI / 180.0;
        int px = cx + r * cos(rad);
        int py = cy + r * sin(rad);
        _display.drawPixel(px, py, WI_BLUE);
    }
    
    // Let's just hardcode a hook shape for simplicity
    // Hook 1
    _display.drawCircleHelper(x+35, y-5, 5, 1, WI_BLUE); // Top-Right quadrant
    _display.drawCircleHelper(x+35, y-5, 5, 2, WI_BLUE); // Top-Left quadrant
    
    // Bottom line offset
    int y2 = y + 20;
    int x2 = x + 15;
    drawThickLine(x2, y2, x2+35, y2, 3, WI_BLUE);
    // Hook 2
    _display.drawCircleHelper(x2+35, y2-5, 5, 1, WI_BLUE);
    _display.drawCircleHelper(x2+35, y2-5, 5, 2, WI_BLUE);
}

void WeatherIcons::drawWeatherIcon(String iconName, int x, int y) {
    // Normalize icon name
    iconName.toLowerCase();
    
    if (iconName == "clear_day" || iconName == "sunny" || iconName == "clear_night") {
        drawSun(x + 48, y + 48, 30);
    }
    else if (iconName.indexOf("partly") != -1) {
        drawSun(x + 65, y + 30, 20);
        drawCloud(x + 20, y + 55, 1.0);
    }
    else if (iconName == "cloudy" || iconName == "mostly_cloudy") {
        drawCloud(x + 25, y + 45, 1.2);
    }
    else if (iconName.indexOf("rain") != -1 || iconName.indexOf("showers") != -1) {
        drawCloud(x + 25, y + 45, 1.1);
        drawRain(x + 35, y + 75);
    }
    else if (iconName.indexOf("snow") != -1 || iconName.indexOf("flurries") != -1) {
        drawCloud(x + 25, y + 45, 1.1);
        drawSnow(x + 35, y + 75);
    }
    else if (iconName.indexOf("thunder") != -1 || iconName.indexOf("storm") != -1) {
        drawCloud(x + 25, y + 45, 1.1);
        drawLightning(x + 48, y + 75);
    }
    else if (iconName == "fog" || iconName == "mist" || iconName == "haze") {
        drawFog(x + 20, y + 35);
    }
    else if (iconName.indexOf("wind") != -1) {
        drawWind(x + 20, y + 40);
    }
    else {
        // Default to sunny/clear if unknown
        drawSun(x + 48, y + 48, 30);
    }
}
