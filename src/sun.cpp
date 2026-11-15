#include "sun.hpp"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "settings.hpp"

static const char *TAG = "sun";

// Wynik zapamiętany dla jednego dnia i jednej miejscowości (czytany z kilku zadań: ekran, strona)
static portMUX_TYPE cache_mux = portMUX_INITIALIZER_UNLOCKED;
static int cache_year = -1, cache_yday = -1;
static char cache_lat[SETTINGS_COORD_LEN], cache_lon[SETTINGS_COORD_LEN];
static int cache_rise = -1, cache_set = -1;

// Odczyt współrzędnej: liczba z kropką w zakresie [-limit, limit]
static bool parseCoord(const char *text, float limit, float *value) {
    char *end;
    float v = strtof(text, &end);
    if (end == text || *end != '\0' || v < -limit || v > limit) return false;
    *value = v;
    return true;
}

// Wzór NOAA (General Solar Position Calculations). yday: dzień roku od 0, utc_offset: minuty (Polska 60 albo 120).
// Kąt 90,833° = środek tarczy na horyzoncie z poprawką na załamanie światła i promień tarczy.
static void compute(int yday, int utc_offset, float lat, float lon, int *rise, int *set) {
    const float rad = (float)M_PI / 180.0f;
    float g = 2.0f * (float)M_PI / 365.0f * yday;  // rok jako kąt
    float eqtime = 229.18f * (0.000075f + 0.001868f * cosf(g) - 0.032077f * sinf(g)
                              - 0.014615f * cosf(2 * g) - 0.040849f * sinf(2 * g));
    float decl = 0.006918f - 0.399912f * cosf(g) + 0.070257f * sinf(g) - 0.006758f * cosf(2 * g)
                 + 0.000907f * sinf(2 * g) - 0.002697f * cosf(3 * g) + 0.00148f * sinf(3 * g);
    float la = lat * rad;
    float c = cosf(90.833f * rad) / (cosf(la) * cosf(decl)) - tanf(la) * tanf(decl);
    if (c < -1.0f || c > 1.0f) {  // słońce cały dzień nad albo pod horyzontem
        *rise = *set = -1;
        return;
    }
    float ha = acosf(c) / rad;  // kąt godzinny wschodu w stopniach
    float r = 720.0f - 4.0f * (lon + ha) - eqtime + utc_offset;
    float s = 720.0f - 4.0f * (lon - ha) - eqtime + utc_offset;
    *rise = (int)lroundf(r);
    *set = (int)lroundf(s);
    if (*rise < 0 || *rise >= 24 * 60 || *set < 0 || *set >= 24 * 60) *rise = *set = -1;
}

// Różnica czasu lokalnego i UTC w danym dniu (w południe), w minutach; uwzględnia czas letni
static int utcOffset(const struct tm *local_day) {
    struct tm noon = *local_day;
    noon.tm_hour = 12;
    noon.tm_min = noon.tm_sec = 0;
    noon.tm_isdst = -1;
    time_t t = mktime(&noon);
    struct tm utc;
    gmtime_r(&t, &utc);
    int diff = 12 * 60 - (utc.tm_hour * 60 + utc.tm_min);
    if (utc.tm_yday != noon.tm_yday) diff += (utc.tm_yday < noon.tm_yday || utc.tm_year < noon.tm_year) ? 1440 : -1440;
    return diff;
}

void Sun::today(int *sunrise, int *sunset) {
    time_t now;
    struct tm local;
    time(&now);
    localtime_r(&now, &local);
    if (local.tm_year < (2024 - 1900)) {  // brak godziny z NTP
        *sunrise = *sunset = -1;
        return;
    }
    char name[SETTINGS_CITY_LEN], lat_s[SETTINGS_COORD_LEN], lon_s[SETTINGS_COORD_LEN];
    Settings::getCity(name, sizeof(name), lat_s, sizeof(lat_s), lon_s, sizeof(lon_s));

    portENTER_CRITICAL(&cache_mux);
    bool cached = cache_year == local.tm_year && cache_yday == local.tm_yday &&
                  strcmp(cache_lat, lat_s) == 0 && strcmp(cache_lon, lon_s) == 0;
    if (cached) {
        *sunrise = cache_rise;
        *sunset = cache_set;
    }
    portEXIT_CRITICAL(&cache_mux);
    if (cached) return;

    float lat, lon;
    int rise = -1, set = -1;
    if (parseCoord(lat_s, 90.0f, &lat) && parseCoord(lon_s, 180.0f, &lon))
        compute(local.tm_yday, utcOffset(&local), lat, lon, &rise, &set);
    ESP_LOGI(TAG, "%s (%s, %s): wschód %d:%02d, zachód %d:%02d", name, lat_s, lon_s,
             rise < 0 ? 0 : rise / 60, rise < 0 ? 0 : rise % 60, set < 0 ? 0 : set / 60, set < 0 ? 0 : set % 60);

    portENTER_CRITICAL(&cache_mux);
    cache_year = local.tm_year;
    cache_yday = local.tm_yday;
    strcpy(cache_lat, lat_s);
    strcpy(cache_lon, lon_s);
    cache_rise = rise;
    cache_set = set;
    portEXIT_CRITICAL(&cache_mux);
    *sunrise = rise;
    *sunset = set;
}
