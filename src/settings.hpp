#ifndef _INCLUDE_SETTINGS_HPP_
#define _INCLUDE_SETTINGS_HPP_

// Ustawienia zmieniane ze strony (budzik.local albo sieć zegara "Budzik-ustawienia"), zapisane w NVS,
// więc zostają po zaniku prądu i po wgraniu nowego programu.
// Gdy w NVS czegoś nie ma, brana jest wartość domyślna: Wi-Fi z secrets.h (jeśli wpisane),
// miejscowość Warszawa, temperatura z pomiaru, jasność jak dawniej na sztywno w kodzie.
// Dawny klucz OpenWeatherMap ("klucz") może jeszcze leżeć w NVS: nieczytany od 2026-11-09 (Open-Meteo bez klucza).
// Dostęp z kilku zadań (główne, pogoda, ekran, strona), więc wszystko pod muteksem i przez kopie.

#include <stdint.h>
#include <stddef.h>

#define SETTINGS_SSID_LEN     33   // 32 bajty + zero
#define SETTINGS_PASS_LEN     65   // 64 bajty + zero
#define SETTINGS_CITY_LEN     48
#define SETTINGS_COORD_LEN    12   // np. "-123.456789"

#define SNOOZE_MIN_MIN        5    // drzemka: najkrócej 5 minut
#define SNOOZE_MIN_MAX        9    // najdłużej 9 minut (na ekranie "9:00" mieści się w polu czasu drzemki)
#define SNOOZE_MIN_DEFAULT    5

#define BRIGHT_PROFILES       2   // 0 = dni robocze, 1 = dni wolne
#define BRIGHT_EVENING_FADE   30   // wieczorem przyciemnianie z pełnego do średniego trwa tyle minut

// Przepis na jasność doby (minuty doby albo minuty). Poziomy: 0 zgaszony, 1 nocny, 2 średni, 3 pełny.
//   noc:     od tej godziny zgaszony (0:00..6:00: po północy, do tej chwili jeszcze nocny; nie wcześniej niż "sen": przed północą)
//   rano:    od wschodu słońca płynnie z nocnego do pełnego przez tyle minut (30..240)
//   pelny:   pełny nie wcześniej niż o tej godzinie (rozjaśnianie wtedy się wydłuża)
//   wieczor: przesunięcie względem zachodu (-120..120), od tej chwili płynnie do średniego przez BRIGHT_EVENING_FADE
//   sen:     od tej godziny nocny (12:00..23:59)
typedef struct {
    int16_t noc;
    int16_t rano;
    int16_t pelny;
    int16_t wieczor;
    int16_t sen;
} bright_recipe_t;

typedef struct {
    bright_recipe_t profile[BRIGHT_PROFILES];
    uint8_t day_profile[7];  // dzień tygodnia (0 = niedziela, jak tm_wday) -> przepis
} brightness_schedule_t;

// Jasność w danej chwili: od poziomu "from" do "to", t = 0..1 (płynnie wg jasności widzianej okiem, display.cpp).
// Stała jasność: from == to.
typedef struct {
    uint8_t from;
    uint8_t to;
    float t;
} bright_point_t;

namespace Settings {
    void init(void);  // po nvs_flash_init(), przed Wi-Fi i pogodą

    // Wi-Fi
    void getWifi(char *ssid, size_t ssid_len, char *pass, size_t pass_len);
    bool hasWifi(void);                  // czy w ogóle jest zapisana sieć
    // verified = true: sieć już działała (ustawia wifi_ok); false: ze strony, jeszcze niesprawdzona
    bool saveWifi(const char *ssid, const char *pass, bool verified = true);
    bool isWifiOk(void);                 // czy zegar choć raz połączył się z zapisaną siecią
    void setWifiOk(void);

    // Pogoda
    void getCity(char *name, size_t name_len, char *lat, size_t lat_len, char *lon, size_t lon_len);
    bool saveCity(const char *name, const char *lat, const char *lon);
    bool getFeelsLike(void);             // true: zegar pokazuje temperaturę odczuwalną, false: z pomiaru
    bool saveFeelsLike(bool feels_like);

    // Drzemka: długość w minutach (SNOOZE_MIN_MIN..SNOOZE_MIN_MAX), zmieniana na stronie; używana przy starcie drzemki
    uint8_t getSnoozeMinutes(void);
    bool saveSnoozeMinutes(uint8_t minutes);

    // Jasność
    void getBrightness(brightness_schedule_t *schedule);
    bool saveBrightness(const brightness_schedule_t *schedule);
    bool isBrightnessValid(const brightness_schedule_t *schedule);
    // Jasność wg przepisu dla dnia tygodnia i minuty doby (z ułamkiem: sekundy)
    bright_point_t brightnessAt(int weekday, float minute_of_day);
}

#endif // _INCLUDE_SETTINGS_HPP_
