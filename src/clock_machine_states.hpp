#ifndef _INCLUDE_CLOCK_MACHINE_STATES_HPP_
#define _INCLUDE_CLOCK_MACHINE_STATES_HPP_

#include <rotary_encoder.hpp>
#include <wifi_time.hpp>
#include "clock_machine.hpp"

#define CONFIRMATION_TRACK  101
#define ALARM_MAX_DURATION_S  1200  // 20 min: dłużej alarm nie gra, jeśli nikt go nie wyłączy (budzik się wtedy wyłącza)
#define SET_ALARM_TIMEOUT_US  (30 * 1000000LL)  // ustawianie budzika kończy się samo po 30 s bez ruchu pokrętłem i kliknięcia

// Forward declaration to resolve circular dependency/include
class ClockMachine;

class ClockState {
   public:
    virtual void enter(ClockMachine* clock) = 0;
    virtual void run(ClockMachine* clock) = 0;
    virtual void timerExpired(ClockMachine* clock) = 0;
    virtual void buttonShortPressed(ClockMachine* clock) = 0;
    virtual void buttonLongPressed(ClockMachine* clock) = 0;
    virtual void buttonDoublePressed(ClockMachine* clock) {}  // dwuklik (włączony tylko w TimeState)
    virtual void encoderRotated(ClockMachine* clock, rotary_encoder_pos_t position, rotary_encoder_dir_t direction = DIR_RIGHT) = 0;
    virtual void exit(ClockMachine* clock) = 0;
    virtual ~ClockState() {}
};

class TimeState : public ClockState {
   public:
    virtual void enter(ClockMachine* clock);
    virtual void run(ClockMachine* clock);
    virtual void timerExpired(ClockMachine* clock);
    virtual void buttonShortPressed(ClockMachine* clock);
    virtual void buttonLongPressed(ClockMachine* clock);
    virtual void buttonDoublePressed(ClockMachine* clock);
    virtual void encoderRotated(ClockMachine* clock, rotary_encoder_pos_t position, rotary_encoder_dir_t direction = DIR_RIGHT);
    virtual void exit(ClockMachine* clock);
    static ClockState& getInstance();
    virtual ~TimeState();
};

class AlarmState : public ClockState {
   public:
    virtual void enter(ClockMachine* clock);
    virtual void run(ClockMachine* clock);
    virtual void timerExpired(ClockMachine* clock);
    virtual void buttonShortPressed(ClockMachine* clock);
    virtual void buttonLongPressed(ClockMachine* clock);
    virtual void encoderRotated(ClockMachine* clock, rotary_encoder_pos_t position, rotary_encoder_dir_t direction = DIR_RIGHT);
    virtual void exit(ClockMachine* clock);
    static ClockState& getInstance();
    virtual ~AlarmState();

   private:
    uint8_t alarm_volume;
    uint8_t crescendo_counter;
    bool alarm_symbol_direction = false;
    int64_t alarm_start_s;
};

class SnoozeState : public ClockState {
   public:
    virtual void enter(ClockMachine* clock);
    virtual void run(ClockMachine* clock);
    virtual void timerExpired(ClockMachine* clock);
    virtual void buttonShortPressed(ClockMachine* clock);
    virtual void buttonLongPressed(ClockMachine* clock);
    virtual void encoderRotated(ClockMachine* clock, rotary_encoder_pos_t position, rotary_encoder_dir_t direction = DIR_RIGHT);
    virtual void exit(ClockMachine* clock);
    static ClockState& getInstance();
    int64_t getStartS() const { return snooze_start_timer_s; }  // początek drzemki (s od startu), dla strony
    uint16_t getTotalS() const { return snooze_total_s; }        // długość tej drzemki (s), dla strony
    virtual ~SnoozeState();

   private:
    enum {
        SNOOZE_WAITING,
        SNOOZE_FIRST_ROTATION,
        SNOOZE_LONG_PRESS,
    } snooze_leaving_step;
    rotary_encoder_dir_t first_rotation_dir;
    int64_t snooze_start_timer_s;
    uint16_t snooze_total_s;   // długość drzemki odczytana przy jej starcie: zmiana na stronie jej nie rusza
    uint16_t remaining_snooze_time_s;
};

class SetAlarmState : public ClockState {
   public:
    virtual void enter(ClockMachine* clock);
    virtual void run(ClockMachine* clock);
    virtual void timerExpired(ClockMachine* clock);
    virtual void buttonShortPressed(ClockMachine* clock);
    virtual void buttonLongPressed(ClockMachine* clock);
    virtual void encoderRotated(ClockMachine* clock, rotary_encoder_pos_t position, rotary_encoder_dir_t direction = DIR_RIGHT);
    virtual void exit(ClockMachine* clock);
    static ClockState& getInstance();
    virtual ~SetAlarmState();

   private:
    bool hours_hidden = false;
    bool minutes_hidden = false;
    bool setting_minutes = false;
    clock_time_t original_alarm_time;
    bool original_alarm_set = false;   // czy budzik był włączony przed wejściem w ustawianie
    bool cancelled = false;            // wyjście przez anulowanie (przytrzymanie albo 30 s bez ruchu)
    int64_t last_activity_us = 0;      // ostatnie obrócenie pokrętła lub kliknięcie
    void cancel(ClockMachine* clock);  // przywraca godzinę i włączenie budzika sprzed ustawiania
};

// Tryb ustawień: na ekranie kod QR sieci zegara, strona pod 192.168.4.1. Zegar dalej liczy czas,
// a budzik dzwoni (wtedy tryb się kończy). Wyjście: przytrzymanie pokrętła, przycisk na stronie
// albo 10 min bez użycia strony (jeśli jest zapisana sieć Wi-Fi).
class SetupState : public ClockState {
   public:
    virtual void enter(ClockMachine* clock);
    virtual void run(ClockMachine* clock);
    virtual void timerExpired(ClockMachine* clock);
    virtual void buttonShortPressed(ClockMachine* clock);
    virtual void buttonLongPressed(ClockMachine* clock);
    virtual void encoderRotated(ClockMachine* clock, rotary_encoder_pos_t position, rotary_encoder_dir_t direction = DIR_RIGHT);
    virtual void exit(ClockMachine* clock);
    static ClockState& getInstance();
    virtual ~SetupState();
};

#endif // _INCLUDE_CLOCK_MACHINE_STATES_HPP_