# EPD-Weather-Colour

## Project Overview
This project is a weather station application designed for an ESP32 microcontroller driving a 7.3-inch 7-Color E-Paper Display (EPD). It fetches weather data and displays current conditions, forecasts, and graphs using a custom UI.

## Hardware
- **Microcontroller:** DFRobot FireBeetle 2 ESP32-E (`dfrobot_firebeetle2_esp32e`)
- **Display:** Waveshare / GoodDisplay 7.3" 7-Color E-Paper Display (GDEP073E01)
- **Connection:** SPI (Busy, RST, DC, CS, SCK, MOSI)

## Software Stack
- **Framework:** Arduino (via PlatformIO)
- **Build System:** PlatformIO
- **Key Libraries:**
  - `GxEPD2`: E-Paper display driver.
  - `Adafruit_GFX`: Graphics primitives.
  - `ArduinoJson`: JSON parsing for API responses.
  - `WiFi`, `HTTPClient`, `WiFiClientSecure`: Network connectivity.

## Project Structure
- **`src/main.cpp`**: Main application logic. Handles WiFi connection, data fetching (currently mocked), and display rendering.
- **`lib/WeatherIcons/`**: Custom library for drawing scalable, vector-like weather icons directly on the display using `Adafruit_GFX` primitives.
  - `WeatherIcons.h`: Class definition.
  - `WeatherIcons.cpp`: Implementation of icon drawing routines (Sun, Cloud, Rain, etc.).
- **`platformio.ini`**: Project configuration file defining the environment, board, and dependencies.
- **`include/secrets.h`**: (Expected) Header file for sensitive data like WiFi credentials and API keys.
- **`generate_icons.py`**: Helper script (likely for asset generation).

## Features
- **Current Weather:** Displays temperature, feels like, wind, humidity, UV index, and pressure.
- **Forecast:** Shows a 3-day daily forecast with high/low temps and weather icons.
- **Graphs:** Renders 24-hour graphs for temperature and rain probability.
- **Custom Icons:** Uses a custom `WeatherIcons` class to draw weather symbols (Sunny, Cloudy, Rain, Snow, etc.) dynamically.
  - Icons support scaling via an `iconSize` parameter.

## API & Data
- **Source:** Google Weather API (referenced in code).
- **Current State:** The project currently uses **mock data** for both current conditions and forecasts to facilitate development without API usage limits.
  - `initMockForecastData()`: Sets up dummy forecast and hourly data.
  - `getWeatherCurrentData()`: Uses a hardcoded JSON payload.
  - `getWeatherCurrentData_ForOnline()`: (Commented out) Implementation for fetching live data.

## Configuration
- **Pins:**
  - `EPD_BUSY`: 25
  - `EPD_RST`: 26
  - `EPD_DC`: 2
  - `EPD_CS`: 14
  - `EPD_SCK`: 18
  - `EPD_MOSI`: 23
  - `EPD_PWR`: 27
