#include "weather.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "settings.hpp"
#include "teksty_ekranu.h"

static const char *TAG = "weather";

// Pogoda i szukanie miejscowości z Open-Meteo (open-meteo.com): bez klucza, zwykłe http,
// darmowe do użytku niekomercyjnego (limit 10 000 zapytań na dobę, zegar robi ok. 50).
// Do 2026-11-09 było OpenWeatherMap (wymagało klucza).

// Bufor odpowiedzi jednego zapytania. Pogoda i szukanie miejscowości mogą iść jednocześnie
// (zadanie pogody i strona), więc każde ma własny bufor.
typedef struct {
    char *buf;
    size_t cap;
    size_t len;
    bool overflow;
} http_response_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA && evt->user_data) {
        http_response_t *r = (http_response_t *)evt->user_data;
        if (r->len + evt->data_len < r->cap) {
            memcpy(r->buf + r->len, evt->data, evt->data_len);
            r->len += evt->data_len;
            r->buf[r->len] = '\0';
        } else {
            r->overflow = true;
        }
    }
    return ESP_OK;
}

// GET pod adres url; zwraca kod HTTP (albo -1 przy błędzie połączenia)
static int httpGet(const char *url, http_response_t *r) {
    r->len = 0;
    r->buf[0] = '\0';
    esp_http_client_config_t config = {};
    config.url = url;
    config.event_handler = http_event_handler;
    config.user_data = r;
    config.timeout_ms = 10000;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) return -1;
    esp_err_t err = esp_http_client_perform(client);
    int status_code = (err == ESP_OK) ? esp_http_client_get_status_code(client) : -1;
    if (err != ESP_OK) ESP_LOGE(TAG, "HTTP: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return status_code;
}

void Weather::init(void) {
    current_weather.is_valid = false;
    current_weather.temperature = 0.0f;
    current_weather.feels_like = 0.0f;

    // Uruchom task do okresowego pobierania pogody
    xTaskCreate(this->updateWeatherTask, "weather_task", 8192, this, 5, &task);
}

void Weather::refreshNow(void) {
    if (task) xTaskNotifyGive(task);
}

void Weather::updateWeatherTask(void *pvParameter) {
    Weather *pThis = (Weather *)pvParameter;

    // Pierwsze pobranie po 10 sekundach (żeby Wi-Fi było gotowe); refreshNow() budzi wcześniej
    ulTaskNotifyTake(pdTRUE, 10000 / portTICK_PERIOD_MS);

    while (1) {
        uint32_t wait_ms;
        if (pThis->fetchWeatherData() == ESP_OK) {
            ESP_LOGI(TAG, "Weather updated: %.1f°C (odczuwalna %.1f°C), %u hPa, %s",
                     pThis->current_weather.temperature, pThis->current_weather.feels_like,
                     pThis->current_weather.pressure, pThis->getWeatherCondition(pThis->current_weather.weather_id));
            wait_ms = WEATHER_UPDATE_INTERVAL;  // następna aktualizacja za 30 minut
        } else {
            ESP_LOGE(TAG, "Failed to fetch weather data, retry in 1 min");
            wait_ms = WEATHER_RETRY_INTERVAL;
        }
        ulTaskNotifyTake(pdTRUE, wait_ms / portTICK_PERIOD_MS);
    }
}

esp_err_t Weather::fetchWeatherData(void) {
    char name[SETTINGS_CITY_LEN], lat[SETTINGS_COORD_LEN], lon[SETTINGS_COORD_LEN];
    Settings::getCity(name, sizeof(name), lat, sizeof(lat), lon, sizeof(lon));

    // Ciśnienie przeliczone na poziom morza (pressure_msl), jak w prognozach i jak dawniej z OpenWeatherMap
    char url[256];
    snprintf(url, sizeof(url),
             "http://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s"
             "&current=temperature_2m,apparent_temperature,relative_humidity_2m,pressure_msl,weather_code",
             lat, lon);

    static char buf[2048];  // używa go tylko zadanie pogody (odpowiedź ma ok. 600 B)
    http_response_t r = {};
    r.buf = buf;
    r.cap = sizeof(buf);
    int status_code = httpGet(url, &r);
    if (status_code == 200 && !r.overflow) {
        esp_err_t err = parseWeatherJSON(buf);
        if (err == ESP_OK) status = WEATHER_OK;
        return err;
    }
    ESP_LOGE(TAG, "HTTP error: %d%s %s", status_code, r.overflow ? " (za długa odpowiedź)" : "",
             status_code == 400 ? buf : "");  // 400: Open-Meteo podaje przyczynę (np. złe współrzędne)
    // Stara pogoda zostaje na ekranie: błąd sieci zwykle mija
    if (status != WEATHER_OK) status = WEATHER_ERROR;
    return ESP_FAIL;
}

// Kodowanie tekstu do adresu (polskie litery jako %C5%82 itd.)
static void urlEncode(const char *src, char *dst, size_t dst_len) {
    static const char hex[] = "0123456789ABCDEF";
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)src; *p && o + 4 < dst_len; p++) {
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
            *p == '-' || *p == '_' || *p == '.' || *p == '~') {
            dst[o++] = *p;
        } else {
            dst[o++] = '%';
            dst[o++] = hex[*p >> 4];
            dst[o++] = hex[*p & 15];
        }
    }
    dst[o] = '\0';
}

// Dopisuje do opisu regionu kolejny człon ("gmina, powiat, województwo"), bez ucinania znaku w połowie
static void appendRegion(char *dst, size_t dst_len, cJSON *item, const char *field) {
    cJSON *v = cJSON_GetObjectItem(item, field);
    if (!cJSON_IsString(v) || v->valuestring[0] == '\0') return;
    size_t used = strlen(dst), sep = used ? 2 : 0, add = strlen(v->valuestring);
    if (used + sep + add + 1 > dst_len) return;  // nie mieści się: pomijamy cały człon
    if (sep) strcat(dst, ", ");
    strcat(dst, v->valuestring);
}

int Weather::findCity(const char *query, geo_result_t *results, int max_results) {
    char q[160];
    urlEncode(query, q, sizeof(q));
    char url[320];
    snprintf(url, sizeof(url),
             "http://geocoding-api.open-meteo.com/v1/search?name=%s&count=%d&language=pl&format=json",
             q, max_results);

    // 5 wyników to ok. 2 KB; zapas na miejscowości z długą listą kodów pocztowych
    http_response_t r = {};
    r.cap = 6144;
    r.buf = (char *)malloc(r.cap);
    if (r.buf == NULL) return -1;
    int status_code = httpGet(url, &r);
    int count = -1;
    if (status_code == 200 && !r.overflow) {
        cJSON *root = cJSON_Parse(r.buf);
        if (root != NULL) {
            count = 0;  // nic nie znaleziono: odpowiedź bez "results"
            cJSON *list = cJSON_GetObjectItem(root, "results");
            cJSON *item;
            cJSON_ArrayForEach(item, list) {
                if (count >= max_results) break;
                cJSON *name = cJSON_GetObjectItem(item, "name");
                cJSON *lat = cJSON_GetObjectItem(item, "latitude");
                cJSON *lon = cJSON_GetObjectItem(item, "longitude");
                if (!cJSON_IsString(name) || !cJSON_IsNumber(lat) || !cJSON_IsNumber(lon)) continue;
                geo_result_t *g = &results[count];
                memset(g, 0, sizeof(*g));
                strncpy(g->name, name->valuestring, sizeof(g->name) - 1);
                // Gmina, powiat, województwo: przy wielu miejscowościach o tej samej nazwie (np. Nowa Wieś)
                appendRegion(g->state, sizeof(g->state), item, "admin3");
                appendRegion(g->state, sizeof(g->state), item, "admin2");
                appendRegion(g->state, sizeof(g->state), item, "admin1");
                cJSON *country = cJSON_GetObjectItem(item, "country_code");
                if (cJSON_IsString(country)) strncpy(g->country, country->valuestring, sizeof(g->country) - 1);
                snprintf(g->lat, sizeof(g->lat), "%.4f", lat->valuedouble);
                snprintf(g->lon, sizeof(g->lon), "%.4f", lon->valuedouble);
                count++;
            }
        }
        cJSON_Delete(root);
    } else {
        ESP_LOGE(TAG, "Geokodowanie: HTTP %d%s", status_code, r.overflow ? " (za długa odpowiedź)" : "");
    }
    free(r.buf);
    return count;
}

esp_err_t Weather::parseWeatherJSON(const char *json_data) {
    cJSON *root = cJSON_Parse(json_data);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return ESP_FAIL;  // też ponowienie po 1 min, a nie po 30
    }

    // Wszystkie potrzebne wartości muszą być: inaczej zostaje poprzednia pogoda i ponowienie po 1 min
    cJSON *cur = cJSON_GetObjectItem(root, "current");
    cJSON *temp = cJSON_GetObjectItem(cur, "temperature_2m");
    cJSON *feels = cJSON_GetObjectItem(cur, "apparent_temperature");
    cJSON *humidity = cJSON_GetObjectItem(cur, "relative_humidity_2m");
    cJSON *pressure = cJSON_GetObjectItem(cur, "pressure_msl");
    cJSON *code = cJSON_GetObjectItem(cur, "weather_code");
    if (!cJSON_IsNumber(temp) || !cJSON_IsNumber(feels) || !cJSON_IsNumber(pressure) || !cJSON_IsNumber(code)) {
        ESP_LOGE(TAG, "Brak danych w odpowiedzi");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    current_weather.temperature = (float)temp->valuedouble;
    current_weather.feels_like = (float)feels->valuedouble;
    if (cJSON_IsNumber(humidity)) current_weather.humidity = (uint8_t)humidity->valueint;
    current_weather.pressure = (uint16_t)lround(pressure->valuedouble);
    current_weather.weather_id = (int16_t)code->valueint;

    current_weather.is_valid = true;
    last_ok_s = (int32_t)(esp_timer_get_time() / 1000000LL);

    cJSON_Delete(root);
    return ESP_OK;
}

int32_t Weather::getAgeS(void) {
    int32_t last = last_ok_s;
    if (last < 0) return -1;
    return (int32_t)(esp_timer_get_time() / 1000000LL) - last;
}

bool Weather::isStale(void) {
    int32_t last = last_ok_s;
    int32_t since = last < 0 ? 0 : last;  // bez żadnego pobrania liczymy od startu
    return (int32_t)(esp_timer_get_time() / 1000000LL) - since >= WEATHER_STALE_S;
}

weather_data_t* Weather::getWeatherData(void) {
    return &current_weather;
}

void Weather::getWeatherString(char *buffer, size_t buffer_size) {
    if (!current_weather.is_valid) {
        snprintf(buffer, buffer_size, "%s", TXT_BRAK_DANYCH);
        return;
    }

    const char* condition = getWeatherCondition(current_weather.weather_id);

    // Format: "10°C | 1013 hPa | opis" (liczba i jednostka ze spacją); temperatura z pomiaru albo odczuwalna (przełącznik na stronie)
    // Zaokrąglenie do liczby całkowitej: od -0,4 do 0,4 zawsze "0°C" (z %.0f wychodziło "-0°C")
    float t = Settings::getFeelsLike() ? current_weather.feels_like : current_weather.temperature;
    long temperature = lroundf(t);
    snprintf(buffer, buffer_size, "%ld°C | %d hPa | %s",
         temperature,
         current_weather.pressure,
         condition);
}

// Kody pogody WMO (Open-Meteo, pole weather_code). Tylko napisy, które już były: ich szerokość
// na ekranie jest sprawdzona (linia pogody najwyżej 315 px przy -30..40°C i 950..1060 hPa).
const char* Weather::getWeatherCondition(int16_t weather_id) {
    switch (weather_id) {
        case 0: return "bezchmurnie";
        case 1: return "mało chmur";      // przeważnie bezchmurnie
        case 2: return "pochmurno";       // częściowe zachmurzenie
        case 3: return "b.pochmurno";     // całkowite zachmurzenie
        case 45:
        case 48: return "mgła";           // 48: mgła osadzająca szadź
        case 51:
        case 53:
        case 55: return "mżawka";
        case 56:
        case 57:                          // marznąca mżawka
        case 66:
        case 67: return "marzn.deszcz";
        case 61: return "lekki deszcz";
        case 63:
        case 65: return "deszcz";
        case 71: return "lekki śnieg";
        case 73:
        case 75:
        case 77: return "śnieg";          // 77: ziarna śniegu
        case 80:
        case 81:
        case 82: return "przel.deszcz";
        case 85:
        case 86: return "przel.śnieg";
        case 95:
        case 96:
        case 99: return "burza";          // 96, 99: z gradem
        default: return TXT_BRAK_DANYCH;
    }
}
