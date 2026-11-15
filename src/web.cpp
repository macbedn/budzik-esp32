#include "web.hpp"

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"  // nagłówek pliku programu (sprawdzenie przed aktualizacją)
#include "esp_timer.h"
#include "esp_http_server.h"
#include "esp_app_desc.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_wifi.h"
#include "mdns.h"
#include "cJSON.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "settings.hpp"
#include "sun.hpp"
#include "ota.hpp"
#include "web_page.h"
#include "wersja.h"
#include "teksty_serwera.h"

static const char *TAG = "web";

static WifiTime *wifi_time = NULL;
static Weather *weather = NULL;
static httpd_handle_t server = NULL;
static volatile int32_t last_setup_activity_s = 0;  // sekundy od startu (32 bity: odczyt niepodzielny)
static volatile bool restart_requested = false;
// Alarm (Web::setAlarm): bity 0-4 godzina, 5-10 minuta, 11 włączony, 12-13 tryb, 14-25 sekundy drzemki,
// 31 = jeszcze nie podany
static volatile uint32_t alarm_packed = 0x80000000u;
static volatile bool audio_ok = true;  // moduł MP3 z kartą SD odpowiada (Web::setAudioOk)

#define MAX_BODY_LEN   2048
#define MAX_SCAN       20

//------------------------------------------------------------------//
//  Pomocnicze                                                      //
//------------------------------------------------------------------//

// Czy zapytanie przyszło przez sieć zegara (192.168.4.1), a nie z sieci domowej
static bool isFromSetupAP(httpd_req_t *req) {
    if (!wifi_time->isSetupAPActive()) return false;
    int fd = httpd_req_to_sockfd(req);
    struct sockaddr_storage addr;
    socklen_t len = sizeof(addr);
    if (getsockname(fd, (struct sockaddr *)&addr, &len) != 0) return false;
    uint32_t ip4 = 0;
    if (addr.ss_family == AF_INET6) {
        // Serwer słucha na IPv6, adresy IPv4 przychodzą jako ::ffff:a.b.c.d
        memcpy(&ip4, &((struct sockaddr_in6 *)&addr)->sin6_addr.s6_addr[12], 4);
    } else if (addr.ss_family == AF_INET) {
        ip4 = ((struct sockaddr_in *)&addr)->sin_addr.s_addr;
    }
    if (ip4 != inet_addr(SETUP_AP_IP)) return false;
    // Sam adres docelowy nie wystarczy: lwIP przyjmuje pakiet na dowolny swój adres z dowolnej sieci, więc
    // ktoś z sieci domowej mógłby wysłać na 192.168.4.1 przez adres zegara. Nadawca musi być w sieci zegara.
    len = sizeof(addr);
    if (getpeername(fd, (struct sockaddr *)&addr, &len) != 0) return false;
    uint32_t peer4 = 0;
    if (addr.ss_family == AF_INET6)
        memcpy(&peer4, &((struct sockaddr_in6 *)&addr)->sin6_addr.s6_addr[12], 4);
    else if (addr.ss_family == AF_INET)
        peer4 = ((struct sockaddr_in *)&addr)->sin_addr.s_addr;
    uint32_t mask = inet_addr("255.255.255.0");
    if ((peer4 & mask) != (ip4 & mask)) return false;
    last_setup_activity_s = (int32_t)(esp_timer_get_time() / 1000000LL);
    return true;
}

static esp_err_t sendJson(httpd_req_t *req, cJSON *json) {
    char *text = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (text == NULL) return httpd_resp_send_500(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_sendstr(req, text);
    free(text);
    return err;
}

static esp_err_t sendOk(httpd_req_t *req) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "ok", true);
    return sendJson(req, json);
}

static esp_err_t sendError(httpd_req_t *req, const char *status, const char *message) {
    httpd_resp_set_status(req, status);
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "blad", message);
    return sendJson(req, json);
}

// Ochrona przed cudzymi stronami (CSRF): strona w przeglądarce nie ma hasła, więc każde zapytanie zmieniające
// ustawienia (POST) musi mieć nagłówek "X-Budzik: 1". Cudza strona nie doda go bez zgody zegara (przeglądarka
// najpierw pyta zegar o pozwolenie, a ten nie odpowiada), strona zegara i curl dodają go zawsze.
static bool hasCustomHeader(httpd_req_t *req) {
    char value[4] = "";
    return httpd_req_get_hdr_value_str(req, "X-Budzik", value, sizeof(value)) == ESP_OK && strcmp(value, "1") == 0;
}

static cJSON *readJson(httpd_req_t *req) {
    if (req->content_len <= 0 || req->content_len > MAX_BODY_LEN) return NULL;
    char *buf = (char *)malloc(req->content_len + 1);
    if (buf == NULL) return NULL;
    size_t got = 0;
    int timeouts = 0;
    while (got < req->content_len) {
        int r = httpd_req_recv(req, buf + got, req->content_len - got);
        if (r <= 0) {
            // Kilka przekroczeń czasu dopuszczamy, ale nie bez końca: serwer ma jedno zadanie dla wszystkich
            if (r == HTTPD_SOCK_ERR_TIMEOUT && ++timeouts < 3) continue;
            free(buf);
            return NULL;
        }
        got += r;
    }
    buf[got] = '\0';
    cJSON *json = cJSON_Parse(buf);
    free(buf);
    return json;
}

static const char *jsonString(cJSON *json, const char *name) {
    cJSON *item = cJSON_GetObjectItem(json, name);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

// Współrzędna jako liczba w zakresie; zapis z 4 miejscami po przecinku (ok. 10 m)
static bool parseCoord(const char *text, double limit, char *out, size_t out_len) {
    if (text == NULL || text[0] == '\0') return false;
    char *end;
    double v = strtod(text, &end);
    while (*end == ' ') end++;
    if (*end != '\0' || v < -limit || v > limit) return false;
    snprintf(out, out_len, "%.4f", v);
    return true;
}

static void weatherStatusText(char *buf, size_t len) {
    // Od 2 h bez udanego pobrania zegar nie pokazuje starej pogody, więc strona też nie
    if (weather->isStale()) {
        snprintf(buf, len, "%s", TXT_POGODA_STARA);
        return;
    }
    switch (weather->getStatus()) {
        case WEATHER_OK: weather->getWeatherString(buf, len); break;
        case WEATHER_ERROR: snprintf(buf, len, "%s", TXT_POGODA_BLAD); break;
        default: snprintf(buf, len, "%s", TXT_POGODA_CZEKAM); break;
    }
}

//------------------------------------------------------------------//
//  Strona i stan                                                   //
//------------------------------------------------------------------//

static esp_err_t pageHandler(httpd_req_t *req) {
    isFromSetupAP(req);
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, WEB_PAGE, sizeof(WEB_PAGE) - 1);
}

// Przyczyna ostatniego uruchomienia po polsku; "zle" = coś poszło nie tak (strona pokazuje na czerwono)
static const char *resetReasonText(esp_reset_reason_t r, bool *bad) {
    *bad = false;
    switch (r) {
        case ESP_RST_POWERON: return TXT_START_ZASILANIE;
        case ESP_RST_EXT: return TXT_START_RESET;
        case ESP_RST_SW: return TXT_START_PROGRAM;
        case ESP_RST_DEEPSLEEP: return TXT_START_USPIENIE;
        case ESP_RST_PANIC: *bad = true; return TXT_START_AWARIA;
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT: *bad = true; return TXT_START_ZAWIESZENIE;
        case ESP_RST_BROWNOUT: *bad = true; return TXT_START_NAPIECIE;
        default: return TXT_START_NIEZNANA;
    }
}

// Stan zegara (panel na dole strony): wersja, czas pracy, ostatni restart, pamięć, sygnał Wi-Fi
static void addClockState(cJSON *json) {
    const esp_app_desc_t *app = esp_app_get_description();
    cJSON *z = cJSON_AddObjectToObject(json, "zegar");
    // Wersja (np. "2.1", "2.1+3"), commit i data z Git (wersja.py), nie app->version: ta bywa nieaktualna
    cJSON_AddStringToObject(z, "wersja", WERSJA_PROGRAMU);
    cJSON_AddStringToObject(z, "git", WERSJA_GIT);
    cJSON_AddStringToObject(z, "data", WERSJA_DATA);
    cJSON_AddStringToObject(z, "idf", app->idf_ver);
    cJSON_AddNumberToObject(z, "czas_s", (double)(esp_timer_get_time() / 1000000LL));
    bool bad;
    cJSON_AddStringToObject(z, "restart", resetReasonText(esp_reset_reason(), &bad));
    cJSON_AddBoolToObject(z, "restart_zly", bad);
    cJSON_AddNumberToObject(z, "sterta", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    cJSON_AddNumberToObject(z, "sterta_min", heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
    wifi_ap_record_t ap;
    if (wifi_time->isWifiConnected() && esp_wifi_sta_get_ap_info(&ap) == ESP_OK)
        cJSON_AddNumberToObject(z, "sygnal", ap.rssi);
    // Aktualizacja przez Wi-Fi (ota.hpp): z którego miejsca działa program i czy nowa wersja jest sprawdzona
    cJSON_AddStringToObject(z, "miejsce", Ota::runningLabel());
    cJSON_AddBoolToObject(z, "ota", esp_ota_get_next_update_partition(NULL) != NULL);
    cJSON_AddStringToObject(z, "ota_stan", Ota::isRestartPending() ? "restart" : Ota::isPendingVerify() ? "sprawdzana" :
                                           Ota::wasRolledBack() ? "wycofana" : "ok");
    cJSON_AddBoolToObject(z, "alarm_zajety", Web::alarmBusy());
    cJSON_AddBoolToObject(z, "dzwiek", audio_ok);
    // Godzina, którą zegar zna (bez niej pola nie ma), i wiek ostatniej udanej pogody w sekundach (bez pobrania pola nie ma)
    if (wifi_time->isTimeSet()) {
        clock_time_t now;
        wifi_time->getTime(&now);
        char hm[8];
        snprintf(hm, sizeof(hm), "%02d:%02d", now.hour, now.minute);
        cJSON_AddStringToObject(z, "godzina", hm);
    }
    int32_t weather_age_s = weather->getAgeS();
    if (weather_age_s >= 0)
        cJSON_AddNumberToObject(z, "pogoda_wiek_s", weather_age_s);
}

static esp_err_t stateHandler(httpd_req_t *req) {
    bool from_ap = isFromSetupAP(req);
    cJSON *json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "ap", from_ap);

    char ssid[SETTINGS_SSID_LEN], pass[SETTINGS_PASS_LEN], ip[16];
    Settings::getWifi(ssid, sizeof(ssid), pass, sizeof(pass));
    memset(pass, 0, sizeof(pass));  // hasła strona nigdy nie dostaje
    wifi_time->getIP(ip, sizeof(ip));
    cJSON *wifi = cJSON_AddObjectToObject(json, "wifi");
    cJSON_AddStringToObject(wifi, "ssid", ssid);
    cJSON_AddBoolToObject(wifi, "polaczono", wifi_time->isWifiConnected());
    cJSON_AddStringToObject(wifi, "ip", ip);

    char name[SETTINGS_CITY_LEN], lat[SETTINGS_COORD_LEN], lon[SETTINGS_COORD_LEN];
    Settings::getCity(name, sizeof(name), lat, sizeof(lat), lon, sizeof(lon));
    cJSON *city = cJSON_AddObjectToObject(json, "miasto");
    cJSON_AddStringToObject(city, "nazwa", name);
    cJSON_AddStringToObject(city, "lat", lat);
    cJSON_AddStringToObject(city, "lon", lon);

    char weather_text[64];
    weatherStatusText(weather_text, sizeof(weather_text));
    cJSON_AddStringToObject(json, "pogoda", weather_text);
    // Stan pogody dla strony (na czerwono przy błędzie)
    static const char *const WEATHER_CODES[] = {"czekam", "ok", "blad"};
    weather_status_t ws = weather->getStatus();
    const char *weather_code = (ws >= 0 && ws <= WEATHER_ERROR) ? WEATHER_CODES[ws] : "czekam";
    if (weather->isStale()) weather_code = "blad";  // jak na zegarze: stara pogoda to błąd
    cJSON_AddStringToObject(json, "pogoda_kod", weather_code);
    // Która temperatura na zegarze (przełącznik na stronie) i obie wartości, zaokrąglone jak na ekranie
    cJSON *temp = cJSON_AddObjectToObject(json, "temperatura");
    cJSON_AddBoolToObject(temp, "odczuwalna", Settings::getFeelsLike());
    const weather_data_t *wd = weather->getWeatherData();
    if (wd->is_valid) {
        cJSON_AddNumberToObject(temp, "pomiar_st", lroundf(wd->temperature));
        cJSON_AddNumberToObject(temp, "odczuwalna_st", lroundf(wd->feels_like));
    }

    brightness_schedule_t b;
    Settings::getBrightness(&b);
    cJSON *bright = cJSON_AddObjectToObject(json, "jasnosc");
    // Przepis jasności (settings.hpp): noc, rano, pelny, wieczor, sen; minuty doby albo minuty
    cJSON *profiles = cJSON_AddArrayToObject(bright, "profile");
    for (int p = 0; p < BRIGHT_PROFILES; p++) {
        const bright_recipe_t *r = &b.profile[p];
        cJSON *prof = cJSON_CreateObject();
        cJSON_AddNumberToObject(prof, "noc", r->noc);
        cJSON_AddNumberToObject(prof, "rano", r->rano);
        cJSON_AddNumberToObject(prof, "pelny", r->pelny);
        cJSON_AddNumberToObject(prof, "wieczor", r->wieczor);
        cJSON_AddNumberToObject(prof, "sen", r->sen);
        cJSON_AddItemToArray(profiles, prof);
    }
    cJSON *days = cJSON_AddArrayToObject(bright, "dni");  // 0 = niedziela
    for (int d = 0; d < 7; d++)
        cJSON_AddItemToArray(days, cJSON_CreateNumber(b.day_profile[d]));
    // Dzisiejszy wschód i zachód wg zegara (minuty doby, -1 = nieznane), dla wykresu doby
    int sunrise, sunset;
    Sun::today(&sunrise, &sunset);
    cJSON_AddNumberToObject(bright, "wschod", sunrise);
    cJSON_AddNumberToObject(bright, "zachod", sunset);

    cJSON_AddNumberToObject(json, "drzemka_min", Settings::getSnoozeMinutes());

    // Alarm do podglądu: godzina w minutach doby, tryb, sekundy do końca drzemki
    uint32_t a = alarm_packed;
    if (!(a & 0x80000000u)) {
        static const char *const ALARM_MODES[] = {"zwykly", "dzwoni", "drzemka", "zwykly"};
        cJSON *alarm = cJSON_AddObjectToObject(json, "alarm");
        cJSON_AddNumberToObject(alarm, "godzina", (a & 0x1F) * 60 + ((a >> 5) & 0x3F));
        cJSON_AddBoolToObject(alarm, "wlaczony", (a >> 11) & 1);
        cJSON_AddStringToObject(alarm, "tryb", ALARM_MODES[(a >> 12) & 3]);
        cJSON_AddNumberToObject(alarm, "drzemka_s", (a >> 14) & 0xFFF);
    }

    addClockState(json);
    return sendJson(req, json);
}

//------------------------------------------------------------------//
//  Temperatura, miejscowość, jasność (z sieci domowej i z sieci zegara)   //
//------------------------------------------------------------------//

// Temperatura na zegarze: {"odczuwalna": true} albo false (z pomiaru). Ekran zmienia się sam w ciągu sekundy.
static esp_err_t feelsLikeHandler(httpd_req_t *req) {
    if (!hasCustomHeader(req)) return sendError(req, "403 Forbidden", ERR_NAGLOWEK);
    isFromSetupAP(req);
    cJSON *json = readJson(req);
    cJSON *v = cJSON_GetObjectItem(json, "odczuwalna");
    if (!cJSON_IsBool(v)) {
        cJSON_Delete(json);
        return sendError(req, "400 Bad Request", ERR_TEMPERATURA);
    }
    bool saved = Settings::saveFeelsLike(cJSON_IsTrue(v));
    cJSON_Delete(json);
    if (!saved) return sendError(req, "500 Internal Server Error", ERR_ZAPIS);
    return sendOk(req);
}

static esp_err_t cityHandler(httpd_req_t *req) {
    if (!hasCustomHeader(req)) return sendError(req, "403 Forbidden", ERR_NAGLOWEK);
    isFromSetupAP(req);
    cJSON *json = readJson(req);
    const char *name = jsonString(json, "nazwa");
    char lat[SETTINGS_COORD_LEN], lon[SETTINGS_COORD_LEN];
    const char *error = NULL;
    if (name == NULL || name[0] == '\0' || strlen(name) >= SETTINGS_CITY_LEN)
        error = ERR_NAZWA_MIASTA;
    else if (!parseCoord(jsonString(json, "lat"), 90, lat, sizeof(lat)))
        error = ERR_SZEROKOSC;
    else if (!parseCoord(jsonString(json, "lon"), 180, lon, sizeof(lon)))
        error = ERR_DLUGOSC;
    else if (!Settings::saveCity(name, lat, lon))
        error = ERR_ZAPIS;
    cJSON_Delete(json);
    if (error) return sendError(req, "400 Bad Request", error);
    weather->refreshNow();
    return sendOk(req);
}

static esp_err_t searchHandler(httpd_req_t *req) {
    isFromSetupAP(req);
    char query_str[200], q[128];
    if (httpd_req_get_url_query_str(req, query_str, sizeof(query_str)) != ESP_OK ||
        httpd_query_key_value(query_str, "q", q, sizeof(q)) != ESP_OK)
        return sendError(req, "400 Bad Request", ERR_SZUKAJ_PUSTE);
    // httpd_query_key_value nie dekoduje %XX: robimy to sami (polskie litery)
    char decoded[128];
    size_t o = 0;
    for (size_t i = 0; q[i] && o + 1 < sizeof(decoded); i++) {
        if (q[i] == '%' && q[i + 1] && q[i + 2]) {
            char hex[3] = {q[i + 1], q[i + 2], 0};
            decoded[o++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else {
            decoded[o++] = (q[i] == '+') ? ' ' : q[i];
        }
    }
    decoded[o] = '\0';

    geo_result_t *results = (geo_result_t *)malloc(sizeof(geo_result_t) * GEO_MAX_RESULTS);
    if (results == NULL) return httpd_resp_send_500(req);
    int count = weather->findCity(decoded, results, GEO_MAX_RESULTS);
    if (count < 0) {
        free(results);
        if (!wifi_time->isWifiConnected())
            return sendError(req, "503 Service Unavailable", ERR_SZUKAJ_BEZ_INTERNETU);
        return sendError(req, "502 Bad Gateway", ERR_SZUKAJ_SERWIS);
    }
    cJSON *json = cJSON_CreateObject();
    cJSON *list = cJSON_AddArrayToObject(json, "wyniki");
    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "nazwa", results[i].name);
        cJSON_AddStringToObject(item, "region", results[i].state);
        cJSON_AddStringToObject(item, "kraj", results[i].country);
        cJSON_AddStringToObject(item, "lat", results[i].lat);
        cJSON_AddStringToObject(item, "lon", results[i].lon);
        cJSON_AddItemToArray(list, item);
    }
    free(results);
    return sendJson(req, json);
}

static esp_err_t brightnessHandler(httpd_req_t *req) {
    if (!hasCustomHeader(req)) return sendError(req, "403 Forbidden", ERR_NAGLOWEK);
    isFromSetupAP(req);
    cJSON *json = readJson(req);
    brightness_schedule_t b = {};
    bool valid = json != NULL;
    cJSON *profiles = cJSON_GetObjectItem(json, "profile");
    cJSON *days = cJSON_GetObjectItem(json, "dni");
    if (!cJSON_IsArray(profiles) || cJSON_GetArraySize(profiles) != BRIGHT_PROFILES ||
        !cJSON_IsArray(days) || cJSON_GetArraySize(days) != 7)
        valid = false;
    for (int p = 0; p < BRIGHT_PROFILES && valid; p++) {
        cJSON *prof = cJSON_GetArrayItem(profiles, p);
        static const char *const FIELDS[] = {"noc", "rano", "pelny", "wieczor", "sen"};
        int v[5];
        for (int k = 0; k < 5 && valid; k++) {
            cJSON *item = cJSON_GetObjectItem(prof, FIELDS[k]);
            if (!cJSON_IsNumber(item) || item->valuedouble < -24 * 60 || item->valuedouble >= 24 * 60) valid = false;
            else v[k] = item->valueint;
        }
        if (!valid) break;
        bright_recipe_t r = {(int16_t)v[0], (int16_t)v[1], (int16_t)v[2], (int16_t)v[3], (int16_t)v[4]};
        b.profile[p] = r;
    }
    for (int d = 0; d < 7 && valid; d++) {
        cJSON *v = cJSON_GetArrayItem(days, d);
        if (!cJSON_IsNumber(v) || v->valueint < 0 || v->valueint >= BRIGHT_PROFILES) valid = false;
        else b.day_profile[d] = v->valueint;
    }
    cJSON_Delete(json);
    // Zakresy wartości sprawdza saveBrightness
    if (!valid || !Settings::saveBrightness(&b))
        return sendError(req, "400 Bad Request", ERR_JASNOSC);
    return sendOk(req);
}

// Długość drzemki: {"minuty": 5..9}. Obowiązuje od następnej drzemki (trwająca zostaje przy swojej długości).
static esp_err_t snoozeHandler(httpd_req_t *req) {
    if (!hasCustomHeader(req)) return sendError(req, "403 Forbidden", ERR_NAGLOWEK);
    isFromSetupAP(req);
    cJSON *json = readJson(req);
    cJSON *v = cJSON_GetObjectItem(json, "minuty");
    bool valid = cJSON_IsNumber(v) && v->valuedouble == (double)v->valueint &&
                 v->valueint >= SNOOZE_MIN_MIN && v->valueint <= SNOOZE_MIN_MAX;
    int minutes = valid ? v->valueint : 0;
    cJSON_Delete(json);
    if (!valid) return sendError(req, "400 Bad Request", ERR_DRZEMKA);
    if (!Settings::saveSnoozeMinutes((uint8_t)minutes)) return sendError(req, "500 Internal Server Error", ERR_ZAPIS);
    return sendOk(req);
}

//------------------------------------------------------------------//
//  Aktualizacja programu (ota.hpp)                                 //
//------------------------------------------------------------------//

// Treść zapytania to sam plik firmware.bin (bez formularza), np. ze strony albo:
// curl.exe -H "X-Budzik: 1" --data-binary "@.pio/build/esp32dev/firmware.bin" http://budzik.local/api/aktualizacja
// Bez hasła (decyzja 2026-11-08: zegar to nie system krytyczny), ale z nagłówkiem X-Budzik (ochrona przed
// cudzymi stronami, patrz hasCustomHeader). Odmowa w czasie alarmu i drzemki.
#define OTA_CHUNK 4096
#define OTA_HEAD  (sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t))

static esp_err_t updateHandler(httpd_req_t *req) {
    if (!hasCustomHeader(req)) return sendError(req, "403 Forbidden", ERR_NAGLOWEK);
    isFromSetupAP(req);
    if (Ota::isRestartPending())
        return sendError(req, "409 Conflict", ERR_OTA_JUZ_WGRANA);
    if (Web::alarmBusy())
        return sendError(req, "409 Conflict", ERR_OTA_ALARM);
    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    if (part == NULL)
        return sendError(req, "500 Internal Server Error", ERR_OTA_BRAK_MIEJSCA);
    if (req->content_len < OTA_HEAD || req->content_len > part->size)
        return sendError(req, "400 Bad Request", ERR_OTA_NIE_TEN_PLIK);

    uint8_t *buf = (uint8_t *)malloc(OTA_CHUNK);
    if (buf == NULL) return httpd_resp_send_500(req);
    ESP_LOGI(TAG, "Aktualizacja: %d B do %s", (int)req->content_len, part->label);

    esp_ota_handle_t handle = 0;
    bool started = false;
    const char *error = NULL;
    size_t total = 0;
    int last_tenth = 0;
    while (total < req->content_len && error == NULL) {
        // Kawałek zawsze pełny (poza ostatnim): pierwszy musi mieć cały nagłówek z opisem programu
        size_t want = req->content_len - total;
        if (want > OTA_CHUNK) want = OTA_CHUNK;
        size_t got = 0;
        int timeouts = 0;
        while (got < want) {
            int r = httpd_req_recv(req, (char *)buf + got, want - got);
            if (r == HTTPD_SOCK_ERR_TIMEOUT && ++timeouts < 3) continue;
            if (r <= 0) break;
            got += r;
            timeouts = 0;
        }
        if (got < want) {
            error = ERR_OTA_PRZERWANE;
            break;
        }
        if (!started) {
            // Ten sam projekt co działający: nie da się wgrać programu z innego urządzenia
            const esp_image_header_t *img = (const esp_image_header_t *)buf;
            const esp_app_desc_t *desc =
                (const esp_app_desc_t *)(buf + sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t));
            const esp_app_desc_t *running = esp_app_get_description();
            if (img->magic != ESP_IMAGE_HEADER_MAGIC || desc->magic_word != ESP_APP_DESC_MAGIC_WORD) {
                error = ERR_OTA_NIE_TEN_PLIK;
                break;
            }
            if (strncmp(desc->project_name, running->project_name, sizeof(desc->project_name)) != 0) {
                error = ERR_OTA_NIE_TEN_PLIK;
                break;
            }
            esp_err_t err = esp_ota_begin(part, OTA_WITH_SEQUENTIAL_WRITES, &handle);
            if (err == ESP_ERR_OTA_ROLLBACK_INVALID_STATE) {
                error = ERR_OTA_SPRAWDZANA;
                break;
            }
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "esp_ota_begin: %s", esp_err_to_name(err));
                error = ERR_OTA_PAMIEC;
                break;
            }
            started = true;
        }
        if (esp_ota_write(handle, buf, got) != ESP_OK) {
            error = ERR_OTA_ZAPIS;
            break;
        }
        total += got;
        int tenth = (int)(total * 10 / req->content_len);
        if (tenth != last_tenth) {
            last_tenth = tenth;
            ESP_LOGI(TAG, "Aktualizacja: %d%%", tenth * 10);
        }
    }
    free(buf);

    if (error == NULL) {
        esp_err_t err = esp_ota_end(handle);  // sprawdza cały plik (suma kontrolna, płytka)
        started = false;
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) error = ERR_OTA_USZKODZONY;
        else if (err != ESP_OK) error = ERR_OTA_KONIEC;
        else if (esp_ota_set_boot_partition(part) != ESP_OK) error = ERR_OTA_USTAW;
    }
    if (started) esp_ota_abort(handle);
    if (error) {
        ESP_LOGE(TAG, "Aktualizacja nieudana: %s", error);
        return sendError(req, "400 Bad Request", error);
    }
    ESP_LOGI(TAG, "Aktualizacja wgrana do %s, restart", part->label);
    Ota::requestRestart();  // restart robi pętla zegara (Ota::check) po wysłaniu odpowiedzi
    return sendOk(req);
}

//------------------------------------------------------------------//
//  Wi-Fi: tylko z sieci zegara                                     //
//------------------------------------------------------------------//

static esp_err_t scanHandler(httpd_req_t *req) {
    if (!isFromSetupAP(req)) return sendError(req, "403 Forbidden", ERR_WIFI_TYLKO_AP);
    wifi_ap_record_t *records = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * MAX_SCAN);
    if (records == NULL) return httpd_resp_send_500(req);
    int n = wifi_time->scanNetworks(records, MAX_SCAN);
    if (n < 0) {
        free(records);
        return sendError(req, "503 Service Unavailable", ERR_WIFI_SKAN);
    }
    cJSON *json = cJSON_CreateObject();
    cJSON *list = cJSON_AddArrayToObject(json, "sieci");
    // Wyniki są od najsilniejszej; ta sama nazwa z kilku punktów dostępowych tylko raz
    for (int i = 0; i < n; i++) {
        const char *ssid = (const char *)records[i].ssid;
        if (ssid[0] == '\0' || strcmp(ssid, SETUP_AP_SSID) == 0) continue;
        bool duplicate = false;
        for (int j = 0; j < i && !duplicate; j++)
            duplicate = strcmp(ssid, (const char *)records[j].ssid) == 0;
        if (duplicate) continue;
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "ssid", ssid);
        cJSON_AddNumberToObject(item, "sila", records[i].rssi);
        cJSON_AddItemToArray(list, item);
    }
    free(records);
    return sendJson(req, json);
}

static esp_err_t wifiSetHandler(httpd_req_t *req) {
    if (!hasCustomHeader(req)) return sendError(req, "403 Forbidden", ERR_NAGLOWEK);
    if (!isFromSetupAP(req)) return sendError(req, "403 Forbidden", ERR_WIFI_TYLKO_AP);
    cJSON *json = readJson(req);
    const char *ssid = jsonString(json, "ssid");
    const char *pass = jsonString(json, "haslo");
    const char *error = NULL;
    if (ssid == NULL || ssid[0] == '\0' || strlen(ssid) > 32)
        error = ERR_WIFI_NAZWA;
    else if (pass == NULL || (pass[0] && (strlen(pass) < 8 || strlen(pass) > 64)))
        error = ERR_WIFI_HASLO;
    // Bez sprawdzania przed zapisem: sprawdzanie przestawiało kanał sieci zegara i telefon gubił stronę.
    // Zła nazwa albo hasło: po restarcie zegar się nie połączy i po 2 min sam włączy sieć zegara
    // z komunikatem (ClockMachine::checkAutoSetup), bo sieć zapisana jest jako niesprawdzona.
    else if (!Settings::saveWifi(ssid, pass, false))
        error = ERR_ZAPIS;
    cJSON_Delete(json);
    if (error) return sendError(req, "400 Bad Request", error);
    restart_requested = true;  // restart robi SetupState::run() po wysłaniu tej odpowiedzi
    return sendOk(req);
}

void Web::setupStarted(void) {
    last_setup_activity_s = (int32_t)(esp_timer_get_time() / 1000000LL);
    restart_requested = false;
}

int64_t Web::getLastSetupActivityUs(void) {
    return (int64_t)last_setup_activity_s * 1000000LL;
}

void Web::setAlarm(uint8_t hour, uint8_t minute, bool on, uint8_t mode, uint16_t snooze_left_s) {
    if (snooze_left_s > 0xFFF) snooze_left_s = 0xFFF;
    alarm_packed = (uint32_t)(hour & 0x1F) | ((uint32_t)(minute & 0x3F) << 5) | ((uint32_t)(on ? 1 : 0) << 11) |
                   ((uint32_t)(mode & 3) << 12) | ((uint32_t)snooze_left_s << 14);
}

void Web::setAudioOk(bool ok) {
    audio_ok = ok;
}

bool Web::alarmBusy(void) {
    uint32_t a = alarm_packed;
    if (a & 0x80000000u) return false;               // zegar jeszcze nie podał alarmu
    if (((a >> 12) & 3) != ALARM_NORMAL) return true;  // dzwoni albo drzemka
    if (!((a >> 11) & 1)) return false;              // alarm wyłączony
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    if (t.tm_year < (2024 - 1900)) return false;     // godzina nieznana: alarm i tak nie zadzwoni
    int alarm_min = (a & 0x1F) * 60 + ((a >> 5) & 0x3F);
    int to_alarm = (alarm_min - (t.tm_hour * 60 + t.tm_min) + 24 * 60) % (24 * 60);
    return to_alarm < WEB_ALARM_SOON_MIN;            // 0 = alarm w tej minucie
}

bool Web::isRunning(void) {
    return server != NULL;
}

bool Web::takeRestartRequest(void) {
    bool requested = restart_requested;
    restart_requested = false;
    return requested;
}

//------------------------------------------------------------------//
//  Start                                                           //
//------------------------------------------------------------------//

void Web::start(WifiTime *wifi, Weather *w) {
    wifi_time = wifi;
    weather = w;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;         // szukanie miejscowości (klient HTTP + JSON) w zadaniu serwera
    config.max_open_sockets = 4;      // gniazd lwIP jest 10: reszta dla pogody, NTP, mDNS
    config.lru_purge_enable = true;
    config.max_uri_handlers = 12;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Serwer strony nie wystartował");
        return;
    }
    const httpd_uri_t uris[] = {
        {.uri = "/", .method = HTTP_GET, .handler = pageHandler, .user_ctx = NULL},
        {.uri = "/api/stan", .method = HTTP_GET, .handler = stateHandler, .user_ctx = NULL},
        {.uri = "/api/temperatura", .method = HTTP_POST, .handler = feelsLikeHandler, .user_ctx = NULL},
        {.uri = "/api/miasto", .method = HTTP_POST, .handler = cityHandler, .user_ctx = NULL},
        {.uri = "/api/szukaj", .method = HTTP_GET, .handler = searchHandler, .user_ctx = NULL},
        {.uri = "/api/jasnosc", .method = HTTP_POST, .handler = brightnessHandler, .user_ctx = NULL},
        {.uri = "/api/drzemka", .method = HTTP_POST, .handler = snoozeHandler, .user_ctx = NULL},
        {.uri = "/api/sieci", .method = HTTP_GET, .handler = scanHandler, .user_ctx = NULL},
        {.uri = "/api/wifi", .method = HTTP_POST, .handler = wifiSetHandler, .user_ctx = NULL},
        {.uri = "/api/aktualizacja", .method = HTTP_POST, .handler = updateHandler, .user_ctx = NULL},
    };
    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++)
        httpd_register_uri_handler(server, &uris[i]);

    // Nazwa budzik.local w sieci domowej (adres z DHCP może się zmieniać)
    if (mdns_init() == ESP_OK) {
        mdns_hostname_set(WEB_HOSTNAME);
        mdns_instance_name_set("Budzik");
        mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
        ESP_LOGI(TAG, "Strona: http://%s.local", WEB_HOSTNAME);
    } else {
        ESP_LOGE(TAG, "mDNS nie wystartował");
    }
}
