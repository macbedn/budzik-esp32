#ifndef _INCLUDE_WEATHER_HPP_
#define _INCLUDE_WEATHER_HPP_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "clock_common.hpp"
#include "cJSON.h"

// Pogoda z Open-Meteo (bez klucza). Miejscowość i wybór temperatury (pomiar / odczuwalna): z ustawień
// (settings.hpp), zmieniane na stronie budzik.local
#define WEATHER_UPDATE_INTERVAL 1800000  // 30 minut w ms
#define WEATHER_RETRY_INTERVAL  60000    // 1 minuta w ms: ponowienie po błędzie (np. router startuje dłużej niż zegar)
#define WEATHER_STALE_S         7200     // 2 h bez udanego pobrania: zegar przestaje pokazywać starą pogodę

typedef enum {
    WEATHER_WAITING = 0,   // jeszcze nie pobrano
    WEATHER_OK,
    WEATHER_ERROR,         // inny błąd (sieć, serwer)
} weather_status_t;

#define GEO_MAX_RESULTS 5

typedef struct {
    char name[48];
    char state[128];   // gmina, powiat, województwo
    char country[4];
    char lat[12];
    char lon[12];
} geo_result_t;

class Weather {
    weather_data_t current_weather;
    volatile weather_status_t status = WEATHER_WAITING;
    // Ostatnie udane pobranie: sekundy od startu (esp_timer), -1 = jeszcze nie było. Nie time(NULL), bo ten skacze
    // po otrzymaniu godziny z internetu. 32 bity: odczyt ze strony (inne zadanie) jest niepodzielny.
    volatile int32_t last_ok_s = -1;
    TaskHandle_t task = NULL;
    static void updateWeatherTask(void *pvParameter);
    esp_err_t fetchWeatherData(void);
    esp_err_t parseWeatherJSON(const char *json_data);

   public:
    void init(void);
    weather_data_t* getWeatherData(void);
    weather_status_t getStatus(void) { return status; }
    int32_t getAgeS(void);   // sekundy od ostatniego udanego pobrania, -1 = jeszcze nie pobrano
    bool isStale(void);      // od WEATHER_STALE_S nie ma udanego pobrania (albo nie było go tyle po starcie)
    void getWeatherString(char *buffer, size_t buffer_size);
    const char* getWeatherCondition(int16_t weather_id);
    void refreshNow(void);  // po zmianie miejscowości: pobierz od razu
    // Szukanie miejscowości po nazwie (geokodowanie Open-Meteo, nazwy po polsku).
    // Zwraca liczbę wyników (0..max) albo -1 przy błędzie.
    int findCity(const char *query, geo_result_t *results, int max_results);
};


#endif // _INCLUDE_WEATHER_HPP_
