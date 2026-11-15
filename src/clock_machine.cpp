#include "nvs.h"
#include "clock_machine.hpp"
#include "clock_machine_states.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "settings.hpp"
#include "web.hpp"
#include "ota.hpp"
#include "teksty_ekranu.h"

static const char *TAG = "clock_machine";

ClockMachine::ClockMachine(RotaryEncoder* encoder_ref) {
    // This will retrieve all stored data from NVS
    if (readNVSValues() == ESP_ERR_NVS_NOT_FOUND) {
        // This is the fault we get when we try to read data which has not yet been written in the memory. In that case we accept that and rewrite
        // the default values (all of them) into the flash. Then we try again but in this case we don't accept errors anymore and crash the SW
        writeNVSDefaultValues();
        ESP_ERROR_CHECK(readNVSValues());
    }

    // Initialize the wifi + sntp stuff
    wifi_time.init();
    wifi_time.getTime(&stored_time);

    weather.init();
    Web::start(&wifi_time, &weather);  // strona ustawień (budzik.local)
    Ota::init();                       // aktualizacja przez Wi-Fi: czy to nowa wersja do sprawdzenia (ota.hpp)

    display.init();
    // Bez godziny z internetu (np. po zaniku prądu): "--:--" i komunikat zamiast godziny od 01:00 (checkTimeKnown)
    display.setTimeUnknown(!wifi_time.isTimeSet());

    if (!audio_player.init(MP3_PLAYER_UART_PORT_NUM, MP3_PLAYER_TX, MP3_PLAYER_RX)) {
        ESP_LOGE(TAG, "There was an error initializing the MP3 player");
    } else {
        ESP_LOGI(TAG, "MP3 player online");
    }

    encoder = encoder_ref;

    // Stan budzika (włączony/wyłączony) jest odczytany z NVS, więc po zaniku prądu budzik dalej działa
    display_action_t alarm_action = is_alarm_set ? D_A_ON : D_A_OFF;
    display.updateContent(D_E_ALARM_TIME, &alarm_time, alarm_action);
    if (is_alarm_set) {
        clock_time_t bed_time = getTimeToAlarm(stored_time, alarm_time);
        display.updateContent(D_E_BED_TIME, &bed_time, D_A_ON);
    }

    // We initialize with the opposite values to force a display update in the next run
    last_wifi_connected_status = !wifi_time.isWifiConnected();
    last_audio_online_status = !audio_player.isDeviceOnline();

    state = &TimeState::getInstance();
    state->enter(this);

   
}

esp_err_t ClockMachine::readNVSValues() {
    nvs_handle_t NVS_handle;

    esp_err_t err = nvs_open(NVS_STORAGE, NVS_READONLY, &NVS_handle);
    if (err != ESP_OK) return err;
    err = nvs_get_u8(NVS_handle, NVS_ALARM_HOUR, &alarm_time.hour);
    if (err != ESP_OK) {
        nvs_close(NVS_handle);
        return err;
    }
    err = nvs_get_u8(NVS_handle, NVS_ALARM_MINUTE, &alarm_time.minute);
    if (err != ESP_OK) {
        nvs_close(NVS_handle);
        return err;
    }
    // Dawny klucz "credentials" (dane Wi-Fi z WPS) nie jest już czytany ani zapisywany: na starych zegarach
    // zostaje w NVS nieużywany. Jego brak nie może dawać błędu, bo zapis wartości domyślnych skasowałby
    // godzinę budzika. Wi-Fi zapisuje sam sterownik (esp_wifi_set_storage).
    // Klucz dodany później: gdy go nie ma (stary zapis), budzik zostaje wyłączony. Brak tego klucza
    // nie może zwrócić błędu, bo zapis wartości domyślnych skasowałby zapisaną godzinę budzika.
    uint8_t alarm_on = 0;
    if (nvs_get_u8(NVS_handle, NVS_ALARM_ON, &alarm_on) == ESP_OK)
        is_alarm_set = (alarm_on != 0);

    nvs_close(NVS_handle);

    return err;
}

void ClockMachine::writeNVSDefaultValues() {
    // Default some values if values not set in NVS yet
    alarm_time.hour = 7;
    alarm_time.minute = 0;

    saveAlarmTimeInNVS();
}

void ClockMachine::saveAlarmTimeInNVS() {
    nvs_handle_t NVS_handle;

    ESP_ERROR_CHECK(nvs_open(NVS_STORAGE, NVS_READWRITE, &NVS_handle));
    ESP_ERROR_CHECK(nvs_set_u8(NVS_handle, NVS_ALARM_HOUR, alarm_time.hour));
    ESP_ERROR_CHECK(nvs_set_u8(NVS_handle, NVS_ALARM_MINUTE, alarm_time.minute));
    ESP_ERROR_CHECK(nvs_commit(NVS_handle));

    nvs_close(NVS_handle);
}

void ClockMachine::saveAlarmOnInNVS() {
    nvs_handle_t NVS_handle;

    ESP_ERROR_CHECK(nvs_open(NVS_STORAGE, NVS_READWRITE, &NVS_handle));
    ESP_ERROR_CHECK(nvs_set_u8(NVS_handle, NVS_ALARM_ON, is_alarm_set ? 1 : 0));
    ESP_ERROR_CHECK(nvs_commit(NVS_handle));

    nvs_close(NVS_handle);
}

void ClockMachine::setState(ClockState& newState) {
    active_timer_us = 0;
    state->exit(this);   // do stuff before we change state
    state = &newState;   // change state
    state->enter(this);  // do stuff after we change state
}

void ClockMachine::checkTimeUpdate(void) {
    clock_time_t current_time;
    wifi_time.getTime(&current_time);

    if ((current_time.hour != stored_time.hour) ||
        (current_time.minute != stored_time.minute)) {
        stored_time.hour = current_time.hour;
        stored_time.minute = current_time.minute;
        display.updateContent(D_E_TIME, &stored_time, D_A_ON);
        colon_visible = true;  // pełne przerysowanie rysuje też dwukropek
        // Pogoda od nowa co minutę (kolor zależy od jasności; bez godziny w tym miejscu jest komunikat)
        updateWeatherLine(true);
        time_has_changed = true;
    }
    else {
        time_has_changed = false;
    }

    // Dwukropek: parzysta sekunda widoczny, nieparzysta ukryty
    bool colon_should_be_visible = (time(NULL) % 2) == 0;
    if (colon_should_be_visible != colon_visible) {
        colon_visible = colon_should_be_visible;
        display.updateContent(D_E_TIME, &stored_time, colon_visible ? D_A_SHOW_COLON : D_A_HIDE_COLON);
    }
}

void ClockMachine::checkTimeKnown(void) {
    if (!wifi_time.isTimeSet()) {
        // Komunikat zależnie od sytuacji
        uint8_t message;
        if (wifi_time.isWifiConnected())
            message = 3;
        else
            message = is_alarm_set ? 1 : 2;
        if (message != no_time_message) {
            no_time_message = message;
            switch (message) {
                case 1:
                    display.updateContent(D_E_MESSAGE, (void *)TXT_BRAK_SIECI_ALARM, D_A_WARNING);
                    break;
                case 2:
                    display.updateContent(D_E_MESSAGE, (void *)TXT_CZEKAM_NA_SIEC, D_A_ON);
                    break;
                default:
                    display.updateContent(D_E_MESSAGE, (void *)TXT_POBIERAM_GODZINE, D_A_ON);
                    break;
            }
        }
    } else if (display.isTimeUnknown()) {
        // Godzina przyszła: zwykły ekran i jasność wg godziny
        ESP_LOGI(TAG, "Time received, leaving no-time mode");
        display.setTimeUnknown(false);
        no_time_message = 0;
        display.updateContent(D_E_MESSAGE, NULL, D_A_OFF);
        wifi_time.getTime(&stored_time);
        display.updateContent(D_E_TIME, &stored_time, D_A_ON);
        colon_visible = true;
        updateWeatherLine(true);
        if (is_alarm_set) {
            clock_time_t bed_time = getTimeToAlarm(stored_time, alarm_time);
            display.updateContent(D_E_BED_TIME, &bed_time, D_A_ON);
        }
    }
}

// Wiersz pod godziną: pogoda albo komunikat. Kolejność ważności: adres po dwukliku, długi brak Wi-Fi,
// brak świeżej pogody (2 h), pogoda. Komunikaty "nie znam godziny" rysuje osobno checkTimeKnown().
void ClockMachine::updateWeatherLine(bool force) {
    if (display.isTimeUnknown() || setup_active)
        return;
    int64_t now = esp_timer_get_time();
    if (!force && now - weather_line_check_us < 250000)
        return;  // wystarczy 4 razy na sekundę
    weather_line_check_us = now;

    char text[64] = "";
    int8_t kind = 0;
    if (now < address_until_us) {
        char ip[16];
        wifi_time.getIP(ip, sizeof(ip));
        if (ip[0])
            snprintf(text, sizeof(text), "%s.local | %s", WEB_HOSTNAME, ip);
        else
            snprintf(text, sizeof(text), "%s", TXT_JAK_USTAWIC_WIFI);  // jak ustawić Wi-Fi
        kind = 2;
    } else if (!wifi_time.isWifiConnected() && now - wifi_time.getDisconnectedSinceUs() > NO_WIFI_HINT_US) {
        snprintf(text, sizeof(text), "%s", TXT_BRAK_WIFI_DLUGO);
        kind = 2;
    } else if (weather.isStale()) {
        // Pogoda nie była pobrana od 2 h (albo wcale od startu): stara nie jest już prawdą
        snprintf(text, sizeof(text), "%s", TXT_POGODA_STARA);
        kind = 2;
    } else if (weather.getWeatherData()->is_valid) {
        weather.getWeatherString(text, sizeof(text));
        kind = 1;
    }

    if (!force && kind == weather_line_kind && strcmp(text, weather_line) == 0)
        return;
    weather_line_kind = kind;
    strncpy(weather_line, text, sizeof(weather_line) - 1);
    weather_line[sizeof(weather_line) - 1] = '\0';
    if (kind == 1)
        display.updateContent(D_E_WEATHER, text, D_A_ON);
    else if (kind == 2)
        display.updateContent(D_E_MESSAGE, text, D_A_ON);
    else
        display.updateContent(D_E_MESSAGE, NULL, D_A_OFF);
}

void ClockMachine::showAddress(void) {
    address_until_us = esp_timer_get_time() + ADDRESS_SHOW_US;
    updateWeatherLine(true);
}

void ClockMachine::enterSetup(const char *warning) {
    if (setup_active) return;
    setup_auto_done = true;
    if (!wifi_time.startSetupAP(setup_password)) {
        ESP_LOGE(TAG, "Nie udało się włączyć sieci zegara");
        return;
    }
    Web::setupStarted();
    setup_active = true;
    setup_screen_t info = {SETUP_AP_SSID, setup_password, SETUP_AP_IP,
                           warning ? warning : TXT_WYJSCIE};
    display.updateContent(D_E_SETUP, &info, D_A_ON);
    if (warning)  // ten sam napis jeszcze raz, na czerwono
        display.updateContent(D_E_SETUP_STATUS, (void *)warning, D_A_WARNING);
    setState(SetupState::getInstance());
}

void ClockMachine::leaveSetup(void) {
    if (!setup_active) return;
    wifi_time.stopSetupAP();
    setup_active = false;
    display.updateContent(D_E_SETUP, NULL, D_A_OFF);
    redrawScreen();
    setState(TimeState::getInstance());
}

void ClockMachine::redrawScreen(void) {
    wifi_time.getTime(&stored_time);
    display.updateContent(D_E_TIME, &stored_time, D_A_ON);
    colon_visible = true;
    display_action_t alarm_action = is_alarm_set ? D_A_ON : D_A_OFF;
    display.updateContent(D_E_ALARM_TIME, &alarm_time, alarm_action);
    if (is_alarm_set) {
        clock_time_t bed_time = getTimeToAlarm(stored_time, alarm_time);
        display.updateContent(D_E_BED_TIME, &bed_time, D_A_ON);
    }
    checkWifiStatus(true);
    last_audio_online_status = !audio_player.isDeviceOnline();  // run() narysuje ikonę dźwięku od nowa
    no_time_message = 0;                                        // i komunikat "nie znam godziny"
    weather_line_kind = -1;
    updateWeatherLine(true);
}

// Sieć zegara włącza się sama tylko wtedy, gdy bez niej nie da się zegara ustawić: brak zapisanej sieci
// albo zapisana sieć nigdy jeszcze nie zadziałała. Zwykły zanik sieci (router, prąd) jej nie włącza.
void ClockMachine::checkAutoSetup(void) {
    if (setup_auto_done || setup_active || state != &TimeState::getInstance())
        return;
    if (!Settings::hasWifi()) {
        ESP_LOGI(TAG, "Brak zapisanej sieci: włączam sieć zegara");
        enterSetup();
    } else if (!Settings::isWifiOk() && !wifi_time.wasEverConnected() && esp_timer_get_time() > SETUP_AUTO_START_US) {
        ESP_LOGI(TAG, "Zapisana sieć nigdy nie działała: włączam sieć zegara");
        enterSetup(TXT_ZLA_SIEC);
    }
}

// Karta SD w DFPlayerze padła 2026-11-08 i zauważyliśmy to dopiero przy ciszy z alarmu. Teraz moduł jest pytany
// o liczbę plików raz na godzinę (i raz ok. 30 s po starcie). Wynik trafia do ikony "brak dźwięku" w run().
// Pytanie blokuje na 200 ms i zeruje zdarzenie modułu, więc tylko w TimeState i nie tuż przed alarmem.
void ClockMachine::checkAudioModule(void) {
    if (state != &TimeState::getInstance() || setup_active)
        return;
    if (esp_timer_get_time() < next_audio_check_us)
        return;
    if (is_alarm_set && wifi_time.isTimeSet()) {
        clock_time_t left = getTimeToAlarm(stored_time, alarm_time);
        if (left.hour == 0 && left.minute <= AUDIO_CHECK_ALARM_MIN)
            return;  // za chwilę alarm: sprawdzimy po nim (kolejne obiegi próbują dalej)
    }
    next_audio_check_us = esp_timer_get_time() + AUDIO_CHECK_EVERY_US;
    bool ok = audio_player.checkSdCard();
    ESP_LOGI(TAG, "SD card check: %s", ok ? "ok" : "no audio");
}

clock_time_t ClockMachine::getTimeToAlarm(clock_time_t current_time, clock_time_t alarm_time) {
    clock_time_t time_to_alarm;

    if (current_time.minute > alarm_time.minute) {
        alarm_time.minute += 60;
        current_time.hour++;
    }
    if (current_time.hour > alarm_time.hour)
        alarm_time.hour += 24;

    time_to_alarm.hour = alarm_time.hour - current_time.hour;
    time_to_alarm.minute = alarm_time.minute - current_time.minute;

    return time_to_alarm;
}

WifiTime* ClockMachine::getWifiTime() {
    return &wifi_time;
}

Weather* ClockMachine::getWeather() {
    return &weather;
}

Display* ClockMachine::getDisplay() {
    return &display;
}

RotaryEncoder* ClockMachine::getEncoder() {
    return encoder;
}

DFPlayer* ClockMachine::getPlayer() {
    return &audio_player;
}

void ClockMachine::triggerTimer(uint16_t timer_ms) {
    active_timer_us = timer_ms * 1000;
    trigger_timestamp_us = esp_timer_get_time();
}

void ClockMachine::checkWifiStatus(bool force_update) {
    bool wifi_connected_status = wifi_time.isWifiConnected();
    if ((last_wifi_connected_status != wifi_connected_status) || force_update) {
        last_wifi_connected_status = wifi_connected_status;
        display_action_t wifi_action = wifi_connected_status ? D_A_ON : D_A_OFF;
        display.updateContent(D_E_WIFI_STATUS, wifi_action);
    }
}

void ClockMachine::run() {
    if (active_timer_us > 0 && (esp_timer_get_time() - trigger_timestamp_us) > active_timer_us) {
        active_timer_us = 0;
        state->timerExpired(this);
    } else {
        checkTimeKnown();
        checkTimeUpdate();
        updateWeatherLine(false);
        checkAutoSetup();
        state->run(this);
        checkAudioModule();
    }
    // Keep track of the wifi status symbol
    checkWifiStatus(false);

    // Keep track of audio player status, if not online then show this as error
    bool audio_online_status = audio_player.isDeviceOnline();
    if (last_audio_online_status != audio_online_status)
    {
        last_audio_online_status = audio_online_status;
        display_action_t audio_action = audio_online_status ? D_A_ON : D_A_OFF; 
        display.updateContent(D_E_AUDIO, audio_action);
    }

    // Alarm dla strony ustawień (tylko podgląd): godzina, włączony, czy dzwoni, ile drzemki zostało
    uint8_t alarm_mode = Web::ALARM_NORMAL;
    uint16_t snooze_left_s = 0;
    if (state == &AlarmState::getInstance()) {
        alarm_mode = Web::ALARM_RINGING;
    } else if (state == &SnoozeState::getInstance()) {
        alarm_mode = Web::ALARM_SNOOZE;
        const SnoozeState &snooze = static_cast<SnoozeState &>(SnoozeState::getInstance());
        int64_t left_s = snooze.getTotalS() - (esp_timer_get_time() / 1000000 - snooze.getStartS());
        snooze_left_s = left_s > 0 ? (uint16_t)left_s : 0;
    }
    Web::setAlarm(alarm_time.hour, alarm_time.minute, is_alarm_set, alarm_mode, snooze_left_s);
    Web::setAudioOk(audio_online_status);

    // Aktualizacja przez Wi-Fi: potwierdzenie nowej wersji, powrót do starej, restart po wgraniu (nie w czasie alarmu)
    Ota::check(wifi_time.isWifiConnected(), Web::isRunning(), !Web::alarmBusy());
}

void ClockMachine::buttonShortPressed() {
    state->buttonShortPressed(this);
}

void ClockMachine::buttonLongPressed() {
    state->buttonLongPressed(this);
}

void ClockMachine::buttonDoublePressed() {
    state->buttonDoublePressed(this);
}

void ClockMachine::encoderRotated(rotary_encoder_pos_t position, rotary_encoder_dir_t direction) {
    state->encoderRotated(this, position, direction);
}
