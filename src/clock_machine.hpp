#ifndef _INCLUDE_CLOCK_MACHINE_HPP_
#define _INCLUDE_CLOCK_MACHINE_HPP_

#include "clock_machine_states.hpp"
#include <rotary_encoder.hpp>
#include <wifi_time.hpp>
#include <display.hpp>
#include <DF_player.hpp>
#include <weather.hpp>  // pogoda
#include "wifi_time.hpp"


#define NVS_STORAGE          "storage"
#define NVS_ALARM_HOUR       "alarm_hour"
#define NVS_ALARM_MINUTE     "alarm_minute"
#define NVS_ALARM_ON         "alarm_on"     // czy budzik włączony (zostaje po zaniku prądu)

#define ADDRESS_SHOW_US        (10 * 1000000LL)        // dwuklik: adres strony przez 10 s w miejscu pogody
#define NO_WIFI_HINT_US        (3 * 3600 * 1000000LL)  // po 3 h bez Wi-Fi komunikat zamiast starej pogody
#define AUDIO_FIRST_CHECK_US   (30 * 1000000LL)        // pierwsze sprawdzenie karty SD w DFPlayerze ok. 30 s po starcie
#define AUDIO_CHECK_EVERY_US   (3600 * 1000000LL)      // potem co godzinę
#define AUDIO_CHECK_ALARM_MIN  2                       // nie na 2 minuty przed alarmem (pytanie blokuje na 200 ms)
#define SETUP_AUTO_START_US    (120 * 1000000LL)       // zapisana sieć nigdy nie działała: sieć zegara po 2 min
#define SETUP_IDLE_TIMEOUT_US  (10 * 60 * 1000000LL)   // tryb ustawień kończy się po 10 min bez użycia strony

// Forward declaration to resolve circular dependency/include
class ClockState;

class ClockMachine {
  public:
    ClockMachine(RotaryEncoder* encoder_ref);
    void saveAlarmTimeInNVS();
    void saveAlarmOnInNVS();
    void setState(ClockState& newState);
    clock_time_t getTimeToAlarm(clock_time_t current_time, clock_time_t alarm_time);
    WifiTime* getWifiTime();
    Display* getDisplay();
    RotaryEncoder* getEncoder();
    DFPlayer* getPlayer();
    Weather* getWeather();  // pogoda
    void triggerTimer(uint16_t timer_ms);
    void checkWifiStatus(bool force_update);
    void showAddress(void);       // dwuklik: adres strony ustawień w miejscu pogody
    void enterSetup(const char *warning = NULL);  // tryb ustawień: sieć zegara, kod QR na ekranie;
                                                  // warning: czerwony napis na dole (np. zła sieć)
    void leaveSetup(void);
    bool isSetupActive(void) { return setup_active; }
    void redrawScreen(void);      // cały zwykły ekran od nowa (po trybie ustawień)
    void run();
    void buttonShortPressed();
    void buttonLongPressed();
    void buttonDoublePressed();
    void encoderRotated(rotary_encoder_pos_t position, rotary_encoder_dir_t direction);
    ~ClockMachine();

    clock_time_t stored_time;
    bool is_alarm_set = false;
    clock_time_t alarm_time;
    bool time_has_changed;
    bool colon_visible = true;  // migający dwukropek

    struct {
        uint8_t crescendo_factor = 6;     // "crescendo_factor" half-seconds per volume step. If factor == 2 -> 30 seconds until maximum volume
        bool alarm_set_confirmation_sound = false;
        uint8_t melody_nr = 1;
        uint8_t max_alarm_volume = 20;  // aktualne MAX VOLUME (max to 30)
    } settings;

  private:
    esp_err_t readNVSValues();
    void writeNVSDefaultValues();
    void checkTimeUpdate(void);
    void checkTimeKnown(void);
    void updateWeatherLine(bool force);
    void checkAutoSetup(void);
    void checkAudioModule(void);  // co godzinę pytanie DFPlayera o kartę SD (tylko w TimeState)

    int64_t next_audio_check_us = AUDIO_FIRST_CHECK_US;  // esp_timer, czyli czas od startu
    ClockState* state;
    WifiTime wifi_time;
    Display display;
    RotaryEncoder* encoder;
    DFPlayer audio_player;
    int64_t active_timer_us;
    int64_t trigger_timestamp_us;
    Weather weather;  // pogoda
    bool last_wifi_connected_status;
    bool last_audio_online_status;
    uint8_t no_time_message = 0;  // pokazany komunikat "nie znam godziny" (0 = żaden)
    // Wiersz pogody: co teraz jest narysowane (żeby nie rysować co 10 ms tego samego)
    char weather_line[64] = "";
    int8_t weather_line_kind = -1;   // -1 nic/nieznane, 0 pusty, 1 pogoda, 2 komunikat
    int64_t weather_line_check_us = 0;
    int64_t address_until_us = 0;    // do kiedy pokazywać adres (dwuklik)
    // Tryb ustawień
    bool setup_active = false;
    bool setup_auto_done = false;    // sieć zegara włącza się sama najwyżej raz od startu
    char setup_password[SETUP_AP_PASS_LEN];
};

#endif /* _INCLUDE_CLOCK_MACHINE_HPP_ */