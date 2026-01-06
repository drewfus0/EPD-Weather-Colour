#include "WeatherStorage.h"

void WeatherStorage::begin() {
    // No explicit initialization needed for Preferences here, handled in methods
}

void WeatherStorage::saveWeatherData(int typeMask, int currentHour, int currentDay, const WeatherData& current, const DailyForecast daily[], const HourlyData hourly[]) {
    preferences.begin("weather", false);
    
    int savedHour = preferences.getInt("hour", -1);
    int savedDay = preferences.getInt("day", -1);
    int status = preferences.getInt("status", DATA_NONE);

    // If day changed, reset everything
    if (savedDay != currentDay) {
        status = DATA_NONE;
        preferences.putInt("day", currentDay);
        preferences.putInt("hour", currentHour);
    }
    // If hour changed (but day is same), keep Daily, clear others
    else if (savedHour != currentHour) {
        status &= DATA_DAILY;
        preferences.putInt("hour", currentHour);
    }

    // Save specific data based on mask
    if (typeMask & DATA_CURRENT) {
        WeatherDataRTC wd;
        strlcpy(wd.conditionText, current.conditionText.c_str(), sizeof(wd.conditionText));
        strlcpy(wd.iconName, current.iconName.c_str(), sizeof(wd.iconName));
        wd.temp = current.temp;
        wd.feelsLike = current.feelsLike;
        wd.windSpeed = current.windSpeed;
        wd.windGust = current.windGust;
        wd.windDirection = current.windDirection;
        wd.humidity = current.humidity;
        wd.precipitationProbability = current.precipitationProbability;
        wd.uvIndex = current.uvIndex;
        wd.pressure = current.pressure;
        wd.valid = current.valid;
        preferences.putBytes("current", &wd, sizeof(wd));
    }

    if (typeMask & DATA_DAILY) {
        DailyForecastRTC dailyRTC[5];
        for(int i=0; i<5; i++) {
            strlcpy(dailyRTC[i].dayName, daily[i].dayName.c_str(), sizeof(dailyRTC[i].dayName));
            strlcpy(dailyRTC[i].iconName, daily[i].iconName.c_str(), sizeof(dailyRTC[i].iconName));
            strlcpy(dailyRTC[i].conditionText, daily[i].conditionText.c_str(), sizeof(dailyRTC[i].conditionText));
            dailyRTC[i].tempHigh = daily[i].tempHigh;
            dailyRTC[i].tempLow = daily[i].tempLow;
            strlcpy(dailyRTC[i].sunrise, daily[i].sunrise.c_str(), sizeof(dailyRTC[i].sunrise));
            strlcpy(dailyRTC[i].sunset, daily[i].sunset.c_str(), sizeof(dailyRTC[i].sunset));
            dailyRTC[i].sunriseHour = daily[i].sunriseHour;
            dailyRTC[i].sunsetHour = daily[i].sunsetHour;
        }
        preferences.putBytes("daily", dailyRTC, sizeof(dailyRTC));
    }

    // Both Hourly Forecast and History update the hourlyData array
    if ((typeMask & DATA_HOURLY) || (typeMask & DATA_HISTORY)) {
        preferences.putBytes("hourly", (void*)hourly, sizeof(HourlyData) * 24);
    }

    // Update status
    status |= typeMask;
    preferences.putInt("status", status);

    preferences.end();
    Serial.printf("Weather data saved (Mask: %d, New Status: %d)\n", typeMask, status);
}

int WeatherStorage::loadWeatherData(int currentHour, int currentDay, WeatherData& current, DailyForecast daily[], HourlyData hourly[]) {
    preferences.begin("weather", true); // Read-only mode
    int savedHour = preferences.getInt("hour", -1);
    int savedDay = preferences.getInt("day", -1);
    int status = preferences.getInt("status", DATA_NONE);
    
    if (savedDay != currentDay) {
        Serial.printf("New day (Saved: %d, Current: %d). Resetting data.\n", savedDay, currentDay);
        preferences.end();
        return DATA_NONE;
    }

    int validStatus = DATA_NONE;

    // Load Daily (Valid as long as day matches)
    if (status & DATA_DAILY) {
        DailyForecastRTC dailyRTC[5];
        if (preferences.getBytes("daily", dailyRTC, sizeof(dailyRTC)) == sizeof(dailyRTC)) {
            for(int i=0; i<5; i++) {
                daily[i].dayName = String(dailyRTC[i].dayName);
                daily[i].iconName = String(dailyRTC[i].iconName);
                daily[i].conditionText = String(dailyRTC[i].conditionText);
                daily[i].tempHigh = dailyRTC[i].tempHigh;
                daily[i].tempLow = dailyRTC[i].tempLow;
                daily[i].sunrise = String(dailyRTC[i].sunrise);
                daily[i].sunset = String(dailyRTC[i].sunset);
                daily[i].sunriseHour = dailyRTC[i].sunriseHour;
                daily[i].sunsetHour = dailyRTC[i].sunsetHour;
            }
            validStatus |= DATA_DAILY;
        }
    }

    // Load Hour-Specific Data (Only if hour matches)
    if (savedHour == currentHour) {
        // Load Current
        if (status & DATA_CURRENT) {
            WeatherDataRTC wd;
            if (preferences.getBytes("current", &wd, sizeof(wd)) == sizeof(wd)) {
                current.conditionText = String(wd.conditionText);
                current.iconName = String(wd.iconName);
                current.temp = wd.temp;
                current.feelsLike = wd.feelsLike;
                current.windSpeed = wd.windSpeed;
                current.windGust = wd.windGust;
                current.windDirection = wd.windDirection;
                current.humidity = wd.humidity;
                current.precipitationProbability = wd.precipitationProbability;
                current.uvIndex = wd.uvIndex;
                current.pressure = wd.pressure;
                current.valid = wd.valid;
                validStatus |= DATA_CURRENT;
            }
        }
    } else {
        Serial.printf("New hour (Saved: %d, Current: %d). Retaining Daily, clearing others.\n", savedHour, currentHour);
    }
    
    // Load Hourly/History even if hour doesn't match (as long as day matches, which is checked above)
    // We need this to preserve accumulated indoor temperature and history data
    if ((status & DATA_HOURLY) || (status & DATA_HISTORY)) {
        if (preferences.getBytes("hourly", (void*)hourly, sizeof(HourlyData) * 24) == sizeof(HourlyData) * 24) {
                // If hour matches, we consider the forecast/history valid (no need to re-fetch)
                if (savedHour == currentHour) {
                    validStatus |= (status & (DATA_HOURLY | DATA_HISTORY));
                }
        }
    }

    preferences.end();
    Serial.printf("Weather data loaded (Status: %d)\n", validStatus);
    return validStatus;
}
