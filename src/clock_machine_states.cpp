#include "clock_machine_states.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"  // esp_restart()
#include "settings.hpp"
#include "web.hpp"
#include "teksty_ekranu.h"

static const char *TAG_ALARM = "AlarmState";  // tag dla logów


//--------------//
//  TIME STATE  //
//--------------//

ClockState& TimeState::getInstance() {
    static TimeState singleton;
    return singleton;
}

void TimeState::enter(ClockMachine* clock) {
    clock->getDisplay()->updateContent(D_E_TIME, &clock->stored_time, D_A_ON);
    clock->getEncoder()->setDoubleClick(true);  // dwuklik tylko tutaj: w innych stanach kliknięcia bez opóźnienia
}

void TimeState::run(ClockMachine* clock) {
    // Bez godziny z internetu (NTP) alarm nie dzwoni. Zegar nie ma RTC z baterią: po zaniku prądu startuje
    // od 01:00 i bez internetu dzwoniłby o przypadkowej porze. Po zwykłym restarcie czas zostaje w ESP32.
    if (clock->is_alarm_set && clock->getWifiTime()->isTimeSet() &&
        (clock->stored_time.hour == clock->alarm_time.hour) &&
        (clock->stored_time.minute == clock->alarm_time.minute)) {
        clock_time_t bed_time = clock->getTimeToAlarm(clock->stored_time, clock->alarm_time);
        clock->getDisplay()->updateContent(D_E_BED_TIME, &bed_time, D_A_OFF);
        clock->setState(AlarmState::getInstance());
    }
    else if (clock->is_alarm_set && clock->time_has_changed)
    {
        clock_time_t bed_time = clock->getTimeToAlarm(clock->stored_time, clock->alarm_time);
        clock->getDisplay()->updateContent(D_E_BED_TIME, &bed_time, D_A_ON);
    }
    
}

void TimeState::timerExpired(ClockMachine* clock) {
    clock->getDisplay()->setIncreasedBrightness(false);
}

void TimeState::buttonShortPressed(ClockMachine* clock) {
    if (clock->getDisplay()->isBrightEnoughForClick()) {
        // Przełączamy budzik tylko wtedy, gdy ekran był już rozjaśniony dotknięciem albo ma poziom dzienny (2+).
        // W nocy (poziom 0 lub 1) pierwsze kliknięcie tylko rozjaśnia ekran na 3 s: drugie w tym czasie przełącza.
        clock->is_alarm_set = !clock->is_alarm_set;
        clock->saveAlarmOnInNVS();
        display_action_t action = clock->is_alarm_set ? D_A_ON : D_A_OFF;        
        clock->getDisplay()->updateContent(D_E_ALARM_TIME, &clock->alarm_time, action);
        clock_time_t bed_time = clock->getTimeToAlarm(clock->stored_time, clock->alarm_time);
        clock->getDisplay()->updateContent(D_E_BED_TIME, &bed_time, action);
    }
    clock->getDisplay()->setIncreasedBrightness(true);
    clock->triggerTimer(3000);
}

void TimeState::buttonLongPressed(ClockMachine* clock) {
    clock->setState(SetAlarmState::getInstance());
}

void TimeState::buttonDoublePressed(ClockMachine* clock) {
    // Adres strony ustawień na 10 s w miejscu pogody, ekran jaśniej przez ten czas
    clock->showAddress();
    clock->getDisplay()->setIncreasedBrightness(true);
    clock->triggerTimer(ADDRESS_SHOW_US / 1000);
}

void TimeState::encoderRotated(ClockMachine* clock, rotary_encoder_pos_t position, rotary_encoder_dir_t direction) {
    // Pokrętło tylko rozjaśnia ekran na 3 s (Wi-Fi ustawia się na stronie, patrz web.hpp).
    clock->getDisplay()->setIncreasedBrightness(true);
    clock->triggerTimer(3000);
}

void TimeState::exit(ClockMachine* clock) {
    clock->getEncoder()->setDoubleClick(false);
}

TimeState::~TimeState() {}


//---------------//
//  ALARM STATE  //
//---------------//

ClockState& AlarmState::getInstance() {
    static AlarmState singleton;
    return singleton;
}

void AlarmState::enter(ClockMachine* clock) {
    clock->getDisplay()->setMaxBrightness(true);
    alarm_volume = 4;

    // Walidacja: sprawdzanie max nie przekracza 30
    if (clock->settings.max_alarm_volume > 30) {
        clock->settings.max_alarm_volume = 30;
    }
    crescendo_counter = clock->settings.crescendo_factor;   // To force setting the volume in the next trigger
    // Cicho przed włączeniem utworu, bo DFPlayer pamięta głośność z poprzedniego alarmu
    clock->getPlayer()->setVolume(alarm_volume);
    clock->getPlayer()->loopTrack(clock->settings.melody_nr);
    alarm_start_s = esp_timer_get_time() / 1000000;
    clock->triggerTimer(10);  // Short trigger to avoid copying code that will be in the timerExpired method
}

void AlarmState::run(ClockMachine* clock) {
    // Alarm gra najwyżej ALARM_MAX_DURATION_S, potem cichnie i budzik się wyłącza (tak samo jak po
    // anulowaniu drzemki): nikt nie zareagował, więc na następny dzień trzeba go włączyć ręcznie.
    if ((esp_timer_get_time() / 1000000) - alarm_start_s >= ALARM_MAX_DURATION_S) {
        ESP_LOGI(TAG_ALARM, "Alarm stopped after %d min without reaction, alarm turned off", ALARM_MAX_DURATION_S / 60);
        clock->is_alarm_set = false;
        clock->saveAlarmOnInNVS();
        clock->setState(TimeState::getInstance());  // AlarmState::exit narysuje wyłączony budzik
        return;
    }
    // Sprawdź czy moduł zgłosił zakończenie odtwarzania
    dfplayer_event_t event = clock->getPlayer()->getLastEvent();
    if (event == DFPLAYER_PLAY_FINISHED) {
        ESP_LOGI(TAG_ALARM, "Track finished, restarting loop with 0x08");
        // Ponownie wyślij komendę loop
        clock->getPlayer()->loopTrack(clock->settings.melody_nr);
    }
}

void AlarmState::timerExpired(ClockMachine* clock) {
    if (crescendo_counter == clock->settings.crescendo_factor) {
        crescendo_counter = 0;
        if (alarm_volume < clock->settings.max_alarm_volume) {   //dodane - ograniczenie do zdefiniowanej w hpp wartosci max
            alarm_volume++;
            clock->getPlayer()->setVolume(alarm_volume);
        }
    }
    crescendo_counter++;
    clock->triggerTimer(500);

    display_action_t action;
    action = alarm_symbol_direction ? D_A_OFF : D_A_ON;
    alarm_symbol_direction = !alarm_symbol_direction;
    clock->getDisplay()->updateContent(D_E_ALARM_ACTIVE, action);
}

void AlarmState::buttonShortPressed(ClockMachine* clock) {
    clock->setState(SnoozeState::getInstance());
}

void AlarmState::buttonLongPressed(ClockMachine* clock) {
}

void AlarmState::encoderRotated(ClockMachine* clock, rotary_encoder_pos_t position, rotary_encoder_dir_t direction) {
}

void AlarmState::exit(ClockMachine* clock) {
    clock->getPlayer()->stopTrack();
    clock->getDisplay()->setMaxBrightness(false);
    // Po drzemce budzik zostaje włączony, po 20 minutach bez reakcji jest wyłączony
    clock->getDisplay()->updateContent(D_E_ALARM_TIME, &clock->alarm_time, clock->is_alarm_set ? D_A_ON : D_A_OFF);
}

AlarmState::~AlarmState() {}


//----------------//
//  SNOOZE STATE  //
//----------------//

ClockState& SnoozeState::getInstance() {
    static SnoozeState singleton;
    return singleton;
}

void SnoozeState::enter(ClockMachine* clock) {
    snooze_leaving_step = SNOOZE_WAITING;
    clock->getDisplay()->setIncreasedBrightness(true);
    clock->triggerTimer(3000);  // To show the time 3 seconds after snoozing
    snooze_start_timer_s = (int64_t)(esp_timer_get_time() / 1000000);
    // Długość z ustawień (strona, 5..9 min) czytana raz, przy starcie drzemki
    snooze_total_s = (uint16_t)Settings::getSnoozeMinutes() * 60;
    remaining_snooze_time_s = snooze_total_s;
}

void SnoozeState::run(ClockMachine* clock) {
    int64_t now_s = (int64_t)(esp_timer_get_time() / 1000000);
    // Liczone w int64_t i sprawdzane "<= 0": przy uint16_t opóźnienie pętli o ponad 1 s na końcu drzemki
    // dawało 65535 zamiast 0 i alarm po drzemce już nie wracał
    int64_t snooze_time_left_s = snooze_total_s - now_s + snooze_start_timer_s;
    if (snooze_time_left_s <= 0) {
        clock->setState(AlarmState::getInstance());
    } else if (snooze_time_left_s < remaining_snooze_time_s) {
        remaining_snooze_time_s = (uint16_t)snooze_time_left_s;
        clock->getDisplay()->updateContent(D_E_SNOOZE_TIME, &remaining_snooze_time_s, D_A_ON);
    }
}

void SnoozeState::timerExpired(ClockMachine* clock) {
    clock->getDisplay()->setIncreasedBrightness(false);
    snooze_leaving_step = SNOOZE_WAITING;
    clock->getDisplay()->updateContent(D_E_SNOOZE_CANCEL, D_A_OFF);
    // Back to the start position for the snooze cancel sequence
}

void SnoozeState::buttonShortPressed(ClockMachine* clock) {
    clock->getDisplay()->setIncreasedBrightness(true);
    clock->triggerTimer(3000);
}

void SnoozeState::buttonLongPressed(ClockMachine* clock) {
    if (snooze_leaving_step == SNOOZE_FIRST_ROTATION) {
        snooze_leaving_step = SNOOZE_LONG_PRESS;
        clock->getDisplay()->updateContent(D_E_SNOOZE_CANCEL, D_A_TWO_BARS);
        //Yes! Now a rotation in the other direction!!!
    }
    // No matter in what state are we, 3 seconds more light.
    clock->getDisplay()->setIncreasedBrightness(true);
    clock->triggerTimer(3000);
}

void SnoozeState::encoderRotated(ClockMachine* clock, rotary_encoder_pos_t position, rotary_encoder_dir_t direction) {
    if (snooze_leaving_step == SNOOZE_WAITING) {
        snooze_leaving_step = SNOOZE_FIRST_ROTATION;
        first_rotation_dir = direction;
        clock->getDisplay()->updateContent(D_E_SNOOZE_CANCEL, D_A_ONE_BAR);
        // Now a long press...
    } else if (snooze_leaving_step == SNOOZE_LONG_PRESS and direction != first_rotation_dir) {
        // Yes! Snooze cancellation sequence complete!
        clock->getDisplay()->updateContent(D_E_SNOOZE_CANCEL, D_A_OFF);
        clock->is_alarm_set = false;
        clock->saveAlarmOnInNVS();
        clock->getDisplay()->updateContent(D_E_ALARM_TIME, &clock->alarm_time, D_A_OFF);
        clock->setState(TimeState::getInstance());
    }

    // Otherwise we are in the first rotation step, keep the window alive.
    // In any case, even if we leave the snooze state, 3 seconds more light
    clock->getDisplay()->setIncreasedBrightness(true);
    clock->triggerTimer(3000);
}

void SnoozeState::exit(ClockMachine* clock) {
    clock->getDisplay()->updateContent(D_E_SNOOZE_TIME, NULL, D_A_OFF);
}

SnoozeState::~SnoozeState() {}

//-------------------//
//  SET ALARM STATE  //
//-------------------//

ClockState& SetAlarmState::getInstance() {
    static SetAlarmState singleton;
    return singleton;
}

void SetAlarmState::enter(ClockMachine* clock) {
    clock->getDisplay()->setIncreasedBrightness(true);
    clock->getDisplay()->updateContent(D_E_ALARM_TIME, &clock->alarm_time, D_A_HIDE_HOURS);
    hours_hidden = true;
    setting_minutes = false;  // We begin with the hours
    clock->triggerTimer(500);
    clock->getEncoder()->setRange(0, 23, true, true);
    clock->getEncoder()->setPosition(clock->alarm_time.hour);
    original_alarm_time.hour = clock->alarm_time.hour;
    original_alarm_time.minute = clock->alarm_time.minute;
    original_alarm_set = clock->is_alarm_set;
    cancelled = false;
    last_activity_us = esp_timer_get_time();
}

void SetAlarmState::run(ClockMachine* clock) {
    // Bez ruchu pokrętłem i kliknięcia przez 30 s ustawianie się anuluje (bo w tym stanie budzik nie dzwoni)
    if (esp_timer_get_time() - last_activity_us > SET_ALARM_TIMEOUT_US) {
        ESP_LOGI("SetAlarmState", "30 s bez ruchu: anuluję ustawianie budzika");
        cancel(clock);
    }
}

void SetAlarmState::cancel(ClockMachine* clock) {
    // Przywraca godzinę i to, czy budzik był włączony; nic nie trafia do NVS (exit() pomija zapis)
    clock->alarm_time.hour = original_alarm_time.hour;
    clock->alarm_time.minute = original_alarm_time.minute;
    clock->is_alarm_set = original_alarm_set;
    cancelled = true;
    clock->setState(TimeState::getInstance());
}

void SetAlarmState::timerExpired(ClockMachine* clock) {
    display_action_t action;

    if (!setting_minutes) {
        // Setting the hours yet
        action = hours_hidden ? D_A_ON : D_A_HIDE_HOURS;
        hours_hidden = !hours_hidden;
    } else {
        // We are setting the minutes now
        action = minutes_hidden ? D_A_ON : D_A_HIDE_MINUTES;
        minutes_hidden = !minutes_hidden;
    }
    clock->getDisplay()->updateContent(D_E_ALARM_TIME, &clock->alarm_time, action);
    clock->triggerTimer(500);
}

void SetAlarmState::buttonShortPressed(ClockMachine* clock) {
    last_activity_us = esp_timer_get_time();
    if (!setting_minutes) {
        clock->getDisplay()->updateContent(D_E_ALARM_TIME, &clock->alarm_time, D_A_HIDE_MINUTES);
        minutes_hidden = true;
        setting_minutes = true;  // It's turn for the minutes now
        clock->triggerTimer(500);
        clock->getEncoder()->setRange(0, 59, true, true);
        clock->getEncoder()->setPosition(clock->alarm_time.minute);
    } else {
        clock->setState(TimeState::getInstance());
    }
}

void SetAlarmState::buttonLongPressed(ClockMachine* clock) {
    // Cancel the alarm setting and restore the original time and on/off state
    cancel(clock);
}

void SetAlarmState::encoderRotated(ClockMachine* clock, rotary_encoder_pos_t position, rotary_encoder_dir_t direction) {
    last_activity_us = esp_timer_get_time();
    if (!setting_minutes) {
        clock->alarm_time.hour = clock->getEncoder()->getPosition();
        hours_hidden = false;

    } else {
        clock->alarm_time.minute = clock->getEncoder()->getPosition();
        minutes_hidden = false;
    }
    clock->getDisplay()->updateContent(D_E_ALARM_TIME, &clock->alarm_time, D_A_ON);
    clock_time_t bed_time = clock->getTimeToAlarm(clock->stored_time, clock->alarm_time);
    clock->getDisplay()->updateContent(D_E_BED_TIME, &bed_time, D_A_ON);

    clock->triggerTimer(500);
}

void SetAlarmState::exit(ClockMachine* clock) {
    if (cancelled) {
        // Anulowanie: godzina i włączenie budzika już przywrócone w cancel(); tylko rysujemy je od nowa
        // (podczas ustawiania godzina i czas do budzika były rysowane jak dla włączonego budzika). Bez zapisu do NVS.
        display_action_t action = clock->is_alarm_set ? D_A_ON : D_A_OFF;
        clock->getDisplay()->updateContent(D_E_ALARM_TIME, &clock->alarm_time, action);
        clock_time_t bed_time = clock->getTimeToAlarm(clock->stored_time, clock->alarm_time);
        clock->getDisplay()->updateContent(D_E_BED_TIME, &bed_time, action);
        clock->triggerTimer(3000);  // jak przy zatwierdzeniu: jaśniej jeszcze 3 s, potem TimeState::timerExpired
        return;
    }
    clock->getDisplay()->updateContent(D_E_ALARM_TIME, &clock->alarm_time, D_A_ON);
    clock->is_alarm_set = true;  // After setting the new alarm time alarm is set
    clock->saveAlarmTimeInNVS();
    clock->saveAlarmOnInNVS();

    // don't need confirmation / no confirmation mp3 on sdcard
    // if (clock->settings.alarm_set_confirmation_sound) {
    //     clock->getPlayer()->setVolume(10);
    //     clock->getPlayer()->playTrack(CONFIRMATION_TRACK);
    //}
    clock->triggerTimer(3000);  // To show the new alarm time at least 3 seconds
}

SetAlarmState::~SetAlarmState() {}


//---------------//
//  SETUP STATE  //
//---------------//

ClockState& SetupState::getInstance() {
    static SetupState singleton;
    return singleton;
}

void SetupState::enter(ClockMachine* clock) {
    // Ekran z kodem QR rysuje ClockMachine::enterSetup()
}

void SetupState::run(ClockMachine* clock) {
    // Budzik ma pierwszeństwo: tryb ustawień się kończy, a TimeState zaraz włączy alarm
    if (clock->is_alarm_set && clock->getWifiTime()->isTimeSet() &&
        (clock->stored_time.hour == clock->alarm_time.hour) &&
        (clock->stored_time.minute == clock->alarm_time.minute)) {
        clock->leaveSetup();
        return;
    }
    if (Web::takeRestartRequest()) {
        // Nowa sieć zapisana ze strony: chwila na wysłanie odpowiedzi do telefonu i restart
        clock->getDisplay()->updateContent(D_E_SETUP_STATUS, (void *)TXT_ZAPISANO_RESTART, D_A_ON);
        ESP_LOGI("setup", "Nowa sieć Wi-Fi zapisana ze strony: restart");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        esp_restart();
    }
    // Bez zapisanej sieci nie ma do czego wracać: wtedy bez limitu czasu
    if (Settings::hasWifi() && esp_timer_get_time() - Web::getLastSetupActivityUs() > SETUP_IDLE_TIMEOUT_US) {
        clock->leaveSetup();
        return;
    }
}

void SetupState::timerExpired(ClockMachine* clock) {
}

void SetupState::buttonShortPressed(ClockMachine* clock) {
}

void SetupState::buttonLongPressed(ClockMachine* clock) {
    clock->leaveSetup();
}

void SetupState::encoderRotated(ClockMachine* clock, rotary_encoder_pos_t position, rotary_encoder_dir_t direction) {
}

void SetupState::exit(ClockMachine* clock) {
}

SetupState::~SetupState() {}
