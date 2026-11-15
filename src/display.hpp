#ifndef _INCLUDE_DISPLAY_HPP
#define _INCLUDE_DISPLAY_HPP

#include "freertos/queue.h"
#include "lgfx_st7789.hpp"
#include "settings.hpp"

#define DISPLAY_BRIGHTNESS_LEVELS_NR    4  // Not including the off-level!

#define DISPLAY_SYMBOL_WIFI_ON   ";"
#define DISPLAY_SYMBOL_WIFI_OFF  "<"
#define DISPLAY_SYMBOL_ALARM_ON  "?"
#define DISPLAY_SYMBOL_ALARM_OFF "@"
#define DISPLAY_SYMBOL_ALARM_L   "A"
#define DISPLAY_SYMBOL_ALARM_R   "B"
#define DISPLAY_SYMBOL_SNOOZE    "C"
#define DISPLAY_SYMBOL_BED       "D"
#define DISPLAY_SYMBOL_AUDIO_OFF "E"

typedef enum {
    D_E_TIME = 0,
    D_E_ALARM_TIME,
    D_E_ALARM_ACTIVE,
    D_E_BED_TIME,
    D_E_SNOOZE_TIME,
    D_E_SNOOZE_CANCEL,
    D_E_WIFI_STATUS,
    D_E_AUDIO,
    D_E_WEATHER,  // pogoda
    D_E_MESSAGE,  // komunikat w wierszu pogody (np. gdy zegar nie zna godziny)
    D_E_SETUP,         // tryb ustawień: cały ekran (value: setup_screen_t*), D_A_OFF wraca do zwykłego ekranu
    D_E_SETUP_STATUS,  // tryb ustawień: dolny wiersz (value: tekst)
} display_element_t;

// Ekran trybu ustawień: kod QR do sieci zegara, jej nazwa, hasło i adres strony
typedef struct {
    const char *ssid;
    const char *password;
    const char *address;
    const char *status;
} setup_screen_t;

typedef enum {
    D_A_OFF = 0,
    D_A_ON,
    D_A_HIDE_HOURS,
    D_A_HIDE_MINUTES,
    D_A_ONE_BAR,
    D_A_TWO_BARS,
    D_A_SHOW_COLON,  // migający dwukropek w godzinie
    D_A_HIDE_COLON,
    D_A_WARNING,     // komunikat czerwony (D_A_ON: szary)
} display_action_t;

class Display {
    LGFX_ST7789 lcd;
    const uint8_t display_light_brightness[DISPLAY_BRIGHTNESS_LEVELS_NR + 1] = {0, 1, 8, 30, 100};
    uint8_t display_brightness_level = 2;
    bool max_brightness_requested = false;
    bool increased_brightness_requested = false;
    QueueHandle_t queue;
    // Ikona "brak dźwięku" pokazywana w miejscu dzwonka budzika; dzwonek zapamiętany, żeby go potem przywrócić
    bool audio_offline = false;
    char alarm_symbol[2] = "";
    uint16_t alarm_symbol_color = TFT_WHITE;
    // Zegar nie zna godziny (brak NTP po włączeniu prądu): "--:--", bez czasu do budzika.
    // Jasność wake_level (po włączeniu 1, każde dotknięcie +1 aż do 3) przez NO_TIME_WAKE_US
    // od ostatniego dotknięcia, potem ekran gaśnie i następne dotknięcie zaczyna znowu od 1.
    volatile bool time_unknown = false;
    volatile int64_t wake_timestamp_us = 0;
    volatile uint8_t wake_level = 1;
    // Tryb ustawień: ekran z kodem QR zamiast zegara; inne elementy nie są wtedy rysowane
    volatile bool setup_screen = false;

    static void monitorBrightnessTask(void *pvParameter);
    void drawAlarmSymbol(const char *symbol, uint16_t color);
    void setBrightness(uint8_t brightness_level);
    void setBrightnessRaw(uint8_t target_brightness);  // wartość dla LovyanGFX (0..255), płynne przejście
    uint8_t rawFromPoint(bright_point_t point);         // jasność z przepisu -> wartość dla LovyanGFX
    void controlBrightness(void);

   public:
    void init(void);
    void updateContent(display_element_t element, void *value, display_action_t action);
    void updateContent(display_element_t element, display_action_t action);
    void setMaxBrightness(bool request_max_brightness);
    void setIncreasedBrightness(bool request_inc_brightness);
    bool isDisplayOn(void);
    // Ekran rozjaśniony dotknięciem albo ma wg harmonogramu poziom 2 lub wyższy (nie nocny): wtedy kliknięcie
    // przycisku wolno traktować jako przełączenie budzika, a nie przypadkowe dotknięcie w nocy.
    bool isBrightEnoughForClick(void);
    void setTimeUnknown(bool unknown);
    bool isTimeUnknown(void) { return time_unknown; }
    bool isSetupScreen(void) { return setup_screen; }
    void clearScreen(void);
};

#endif // _INCLUDE_DISPLAY_HPP