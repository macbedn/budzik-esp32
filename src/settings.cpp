#include "settings.hpp"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "esp_log.h"
#include "sun.hpp"

// Wartości domyślne z secrets.h (plik nieobowiązkowy, poza Git), np.:
//   #define WIFI_SSID_HARDCODED     "nazwa sieci"
//   #define WIFI_PASSWORD_HARDCODED "hasło"
// Gdy go nie ma albo są w nim teksty "TU_WPISZ_...", zegar startuje bez sieci: Wi-Fi ustawia się przez sieć zegara.
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_SSID_HARDCODED
#define WIFI_SSID_HARDCODED ""
#endif
#ifndef WIFI_PASSWORD_HARDCODED
#define WIFI_PASSWORD_HARDCODED ""
#endif

#define DEFAULT_CITY "Warszawa"
#define DEFAULT_LAT  "52.2297"
#define DEFAULT_LON  "21.0122"

// NVS: osobna przestrzeń nazw, żeby nie mieszać z godziną budzika ("storage")
#define NVS_SETTINGS   "ustawienia"
#define KEY_SSID       "ssid"
#define KEY_PASS       "haslo"
#define KEY_WIFI_OK    "wifi_ok"
#define KEY_CITY       "miasto"
#define KEY_LAT        "lat"
#define KEY_LON        "lon"
#define KEY_FEELS      "odczuwalna"  // 1 = temperatura odczuwalna, 0 / brak = z pomiaru
#define KEY_SNOOZE     "drzemka"     // długość drzemki w minutach (5..9, domyślnie 5)
#define KEY_BRIGHT     "jasnosc"
#define BRIGHT_VERSION 3  // pierwszy bajt zapisu jasności: 3 = przepis (2 = lista zmian, 1 = stałe godziny)

static const char *TAG = "settings";

typedef struct {
    char ssid[SETTINGS_SSID_LEN];
    char pass[SETTINGS_PASS_LEN];
    bool wifi_ok;
    char city[SETTINGS_CITY_LEN];
    char lat[SETTINGS_COORD_LEN];
    char lon[SETTINGS_COORD_LEN];
    bool feels_like;
    uint8_t snooze_min;
    brightness_schedule_t bright;
} settings_t;

static settings_t s;
static SemaphoreHandle_t mutex = NULL;

static void lock(void) { xSemaphoreTake(mutex, portMAX_DELAY); }
static void unlock(void) { xSemaphoreGive(mutex); }

// Kopia z obcięciem do rozmiaru bufora (zawsze z zerem na końcu)
static void copyStr(char *dst, size_t dst_len, const char *src) {
    if (dst_len == 0) return;
    strncpy(dst, src, dst_len - 1);
    dst[dst_len - 1] = '\0';
}

// Tekst-wzór "TU_WPISZ_..." traktujemy jak pusty
static const char *defaultOrEmpty(const char *value) {
    return (strncmp(value, "TU_WPISZ", 8) == 0) ? "" : value;
}

static void readStr(nvs_handle_t h, const char *key, char *dst, size_t dst_len, const char *def) {
    size_t len = dst_len;
    if (nvs_get_str(h, key, dst, &len) != ESP_OK)
        copyStr(dst, dst_len, def);
}

static void defaultBrightness(brightness_schedule_t *b) {
    // Zgaszony od 0:00, od wschodu przez 2 h płynnie do pełnego (nie wcześniej niż 7:00),
    // od zachodu 30 min do średniego, od 22:00 nocny. Sobota i niedziela przypisane do "wolnych" (ten sam przepis).
    static const bright_recipe_t def = {0, 120, 7 * 60, 0, 22 * 60};
    memset(b, 0, sizeof(*b));
    for (int p = 0; p < BRIGHT_PROFILES; p++)
        b->profile[p] = def;
    for (int d = 0; d < 7; d++)
        b->day_profile[d] = (d == 0 || d == 6) ? 1 : 0;
}

void Settings::init(void) {
    mutex = xSemaphoreCreateMutex();
    memset(&s, 0, sizeof(s));
    s.snooze_min = SNOOZE_MIN_DEFAULT;
    defaultBrightness(&s.bright);

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_SETTINGS, NVS_READONLY, &h);
    if (err == ESP_OK) {
        readStr(h, KEY_SSID, s.ssid, sizeof(s.ssid), defaultOrEmpty(WIFI_SSID_HARDCODED));
        readStr(h, KEY_PASS, s.pass, sizeof(s.pass), defaultOrEmpty(WIFI_PASSWORD_HARDCODED));
        uint8_t ok = 0;
        s.wifi_ok = (nvs_get_u8(h, KEY_WIFI_OK, &ok) == ESP_OK) && ok;
        readStr(h, KEY_CITY, s.city, sizeof(s.city), DEFAULT_CITY);
        readStr(h, KEY_LAT, s.lat, sizeof(s.lat), DEFAULT_LAT);
        readStr(h, KEY_LON, s.lon, sizeof(s.lon), DEFAULT_LON);
        uint8_t feels = 0;
        s.feels_like = (nvs_get_u8(h, KEY_FEELS, &feels) == ESP_OK) && feels;
        uint8_t snooze = SNOOZE_MIN_DEFAULT;
        if (nvs_get_u8(h, KEY_SNOOZE, &snooze) == ESP_OK)
            s.snooze_min = snooze < SNOOZE_MIN_MIN ? SNOOZE_MIN_MIN : (snooze > SNOOZE_MIN_MAX ? SNOOZE_MIN_MAX : snooze);
        uint8_t blob[1 + sizeof(brightness_schedule_t)];
        size_t len = sizeof(blob);
        if (nvs_get_blob(h, KEY_BRIGHT, blob, &len) == ESP_OK) {
            brightness_schedule_t b;
            if (blob[0] == BRIGHT_VERSION && len == 1 + sizeof(b)) {
                memcpy(&b, blob + 1, sizeof(b));
                if (isBrightnessValid(&b))
                    s.bright = b;
                else
                    ESP_LOGE(TAG, "Zapisana jasność błędna, domyślna");
            } else {
                // Starsze wersje (lista zmian): przepis domyślny, taki sam jak ostatni harmonogram z wersji 2
                ESP_LOGI(TAG, "Jasność w starym układzie (wersja %d), przepis domyślny", blob[0]);
            }
        }
        nvs_close(h);
    } else {
        // Pierwsze uruchomienie tej wersji: w NVS nic nie ma, same wartości domyślne
        copyStr(s.ssid, sizeof(s.ssid), defaultOrEmpty(WIFI_SSID_HARDCODED));
        copyStr(s.pass, sizeof(s.pass), defaultOrEmpty(WIFI_PASSWORD_HARDCODED));
        copyStr(s.city, sizeof(s.city), DEFAULT_CITY);
        copyStr(s.lat, sizeof(s.lat), DEFAULT_LAT);
        copyStr(s.lon, sizeof(s.lon), DEFAULT_LON);
    }
    ESP_LOGI(TAG, "Wi-Fi: \"%s\"%s, miejscowość: %s (%s, %s), temperatura: %s",
             s.ssid, s.wifi_ok ? " (sprawdzona)" : "", s.city, s.lat, s.lon, s.feels_like ? "odczuwalna" : "z pomiaru");
}

// Zapis kilku napisów naraz (jeden commit)
static bool writeStrs(const char *const keys[], const char *const values[], int n) {
    nvs_handle_t h;
    if (nvs_open(NVS_SETTINGS, NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = ESP_OK;
    for (int i = 0; i < n && err == ESP_OK; i++)
        err = nvs_set_str(h, keys[i], values[i]);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) ESP_LOGE(TAG, "Błąd zapisu NVS: %s", esp_err_to_name(err));
    return err == ESP_OK;
}

static bool writeU8(const char *key, uint8_t value) {
    nvs_handle_t h;
    if (nvs_open(NVS_SETTINGS, NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = nvs_set_u8(h, key, value);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err == ESP_OK;
}

void Settings::getWifi(char *ssid, size_t ssid_len, char *pass, size_t pass_len) {
    lock();
    copyStr(ssid, ssid_len, s.ssid);
    copyStr(pass, pass_len, s.pass);
    unlock();
}

bool Settings::hasWifi(void) {
    lock();
    bool has = s.ssid[0] != '\0';
    unlock();
    return has;
}

bool Settings::saveWifi(const char *ssid, const char *pass, bool verified) {
    const char *keys[] = {KEY_SSID, KEY_PASS};
    const char *values[] = {ssid, pass};
    // Niesprawdzona: najpierw flaga, żeby nowa sieć nigdy nie została z flagą "działała" po starej
    if (!verified && !writeU8(KEY_WIFI_OK, 0)) return false;
    if (!writeStrs(keys, values, 2)) return false;
    if (verified && !writeU8(KEY_WIFI_OK, 1)) return false;
    lock();
    copyStr(s.ssid, sizeof(s.ssid), ssid);
    copyStr(s.pass, sizeof(s.pass), pass);
    s.wifi_ok = verified;
    unlock();
    ESP_LOGI(TAG, "Zapisana sieć Wi-Fi \"%s\"%s", ssid, verified ? "" : " (niesprawdzona)");
    return true;
}

bool Settings::isWifiOk(void) {
    lock();
    bool ok = s.wifi_ok;
    unlock();
    return ok;
}

void Settings::setWifiOk(void) {
    if (isWifiOk()) return;
    // Pierwsze połączenie z tą siecią: zapisujemy też samą sieć (mogła przyjść z secrets.h),
    // żeby nowszy program bez secrets.h dalej ją znał. Flaga dopiero po udanym zapisie.
    char ssid[SETTINGS_SSID_LEN], pass[SETTINGS_PASS_LEN];
    getWifi(ssid, sizeof(ssid), pass, sizeof(pass));
    const char *keys[] = {KEY_SSID, KEY_PASS};
    const char *values[] = {ssid, pass};
    bool saved = writeStrs(keys, values, 2) && writeU8(KEY_WIFI_OK, 1);
    memset(pass, 0, sizeof(pass));
    if (!saved) {
        ESP_LOGE(TAG, "Nie zapisano sieci Wi-Fi");
        return;
    }
    lock();
    s.wifi_ok = true;
    unlock();
}

void Settings::getCity(char *name, size_t name_len, char *lat, size_t lat_len, char *lon, size_t lon_len) {
    lock();
    copyStr(name, name_len, s.city);
    copyStr(lat, lat_len, s.lat);
    copyStr(lon, lon_len, s.lon);
    unlock();
}

bool Settings::saveCity(const char *name, const char *lat, const char *lon) {
    const char *keys[] = {KEY_CITY, KEY_LAT, KEY_LON};
    const char *values[] = {name, lat, lon};
    if (!writeStrs(keys, values, 3)) return false;
    lock();
    copyStr(s.city, sizeof(s.city), name);
    copyStr(s.lat, sizeof(s.lat), lat);
    copyStr(s.lon, sizeof(s.lon), lon);
    unlock();
    ESP_LOGI(TAG, "Zapisana miejscowość %s (%s, %s)", name, lat, lon);
    return true;
}

bool Settings::getFeelsLike(void) {
    lock();
    bool feels = s.feels_like;
    unlock();
    return feels;
}

bool Settings::saveFeelsLike(bool feels_like) {
    if (!writeU8(KEY_FEELS, feels_like ? 1 : 0)) return false;
    lock();
    s.feels_like = feels_like;
    unlock();
    ESP_LOGI(TAG, "Temperatura na zegarze: %s", feels_like ? "odczuwalna" : "z pomiaru");
    return true;
}

uint8_t Settings::getSnoozeMinutes(void) {
    lock();
    uint8_t minutes = s.snooze_min;
    unlock();
    return minutes;
}

bool Settings::saveSnoozeMinutes(uint8_t minutes) {
    if (minutes < SNOOZE_MIN_MIN || minutes > SNOOZE_MIN_MAX) return false;
    if (!writeU8(KEY_SNOOZE, minutes)) return false;
    lock();
    s.snooze_min = minutes;
    unlock();
    ESP_LOGI(TAG, "Długość drzemki: %d min", minutes);
    return true;
}

void Settings::getBrightness(brightness_schedule_t *schedule) {
    lock();
    *schedule = s.bright;
    unlock();
}

bool Settings::isBrightnessValid(const brightness_schedule_t *b) {
    for (int p = 0; p < BRIGHT_PROFILES; p++) {
        const bright_recipe_t *r = &b->profile[p];
        if (r->noc < 0 || r->noc >= 24 * 60) return false;
        if (r->rano < 30 || r->rano > 240) return false;
        if (r->pelny < 0 || r->pelny >= 24 * 60) return false;
        if (r->wieczor < -120 || r->wieczor > 120) return false;
        if (r->sen < 12 * 60 || r->sen >= 24 * 60) return false;
        // Noc (zgaszony od): od "przed snem" wzwyż (przed północą) albo między 0:00 a 6:00 (noc po północy)
        if (r->noc < r->sen && r->noc > 6 * 60) return false;
    }
    for (int d = 0; d < 7; d++)
        if (b->day_profile[d] >= BRIGHT_PROFILES) return false;
    return true;
}

bool Settings::saveBrightness(const brightness_schedule_t *schedule) {
    if (!isBrightnessValid(schedule)) return false;
    uint8_t blob[1 + sizeof(brightness_schedule_t)];
    blob[0] = BRIGHT_VERSION;
    memcpy(blob + 1, schedule, sizeof(*schedule));
    nvs_handle_t h;
    if (nvs_open(NVS_SETTINGS, NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = nvs_set_blob(h, KEY_BRIGHT, blob, sizeof(blob));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) return false;
    lock();
    s.bright = *schedule;
    unlock();
    ESP_LOGI(TAG, "Zapisany harmonogram jasności");
    return true;
}

// Gdy wschodu / zachodu nie da się policzyć (brak godziny, złe lat/lon): przyjmujemy 6:00 i 18:00
#define SUNRISE_FALLBACK (6 * 60)
#define SUNSET_FALLBACK  (18 * 60)

static bright_point_t point(uint8_t from, uint8_t to, float t) {
    bright_point_t p = {from, to, t < 0 ? 0.0f : (t > 1 ? 1.0f : t)};
    return p;
}

// Ta sama kolejność sprawdzania co w skrypcie strony (web_page.h, funkcja jasn)
bright_point_t Settings::brightnessAt(int weekday, float m) {
    int sunrise, sunset;
    Sun::today(&sunrise, &sunset);  // przed lock(): sam czyta miejscowość z ustawień
    if (sunrise < 0) sunrise = SUNRISE_FALLBACK;
    if (sunset < 0) sunset = SUNSET_FALLBACK;
    lock();
    bright_recipe_t r = s.bright.profile[s.bright.day_profile[(weekday % 7 + 7) % 7]];
    unlock();

    int morning_end = sunrise + r.rano;            // rano > 0, więc koniec zawsze po wschodzie
    if (morning_end < r.pelny) morning_end = r.pelny;
    int evening = sunset + r.wieczor;

    if (r.noc >= 12 * 60 && m >= r.noc) return point(0, 0, 0);       // noc przed północą
    if (m >= r.sen) return point(1, 1, 0);                             // przed snem
    if (m < sunrise) return (r.noc > 0 && r.noc < 12 * 60 && m < r.noc) ? point(1, 1, 0) : point(0, 0, 0);
    if (m < morning_end) return point(1, 3, (m - sunrise) / (float)(morning_end - sunrise));  // płynnie w górę
    if (m < evening) return point(3, 3, 0);
    if (m < evening + BRIGHT_EVENING_FADE) return point(3, 2, (m - evening) / (float)BRIGHT_EVENING_FADE);
    return point(2, 2, 0);
}
