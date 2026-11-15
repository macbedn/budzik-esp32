#ifndef _INCLUDE_WEB_HPP_
#define _INCLUDE_WEB_HPP_

// Strona ustawień: w sieci domowej stale pod http://budzik.local (mDNS) i adresem z DHCP, bez hasła.
// Zmiana sieci Wi-Fi tylko z sieci zegara (tryb ustawień, zwykła strona http://192.168.4.1, bez captive
// portalu): tam strona ma tylko formularz Wi-Fi, a zapis kończy się restartem zegara.

#include <stdint.h>
#include "wifi_time.hpp"
#include "weather.hpp"

// WEB_HOSTNAME ("budzik", http://budzik.local) jest w clock_common.hpp: używa go też wifi_time.cpp

namespace Web {
    void start(WifiTime *wifi, Weather *weather);
    void setupStarted(void);               // razem z siecią zegara: liczenie czasu bez użycia od nowa
    int64_t getLastSetupActivityUs(void);  // ostatnie zapytanie z sieci zegara (esp_timer)
    bool takeRestartRequest(void);         // sieć zapisana przyciskiem na stronie: zegar ma się uruchomić ponownie
    // Alarm do podglądu na stronie (ustawia się tylko pokrętłem). Woła go zadanie zegara przy każdym obiegu,
    // strona czyta z zadania serwera: wszystko w jednej 32-bitowej liczbie, więc odczyt jest niepodzielny.
    enum { ALARM_NORMAL = 0, ALARM_RINGING = 1, ALARM_SNOOZE = 2 };
    void setAlarm(uint8_t hour, uint8_t minute, bool on, uint8_t mode, uint16_t snooze_left_s);
    // Czy moduł MP3 z kartą SD odpowiada (DFPlayer): podawane z pętli zegara, czytane przez stronę (bool: odczyt niepodzielny)
    void setAudioOk(bool ok);
    // Alarm dzwoni, trwa drzemka albo alarm zadzwoni w ciągu WEB_ALARM_SOON_MIN minut: wtedy bez aktualizacji
    // programu i bez restartu po niej (ota.hpp)
    bool alarmBusy(void);
    bool isRunning(void);                  // serwer strony działa (potwierdzenie nowej wersji po aktualizacji)
}

#define WEB_ALARM_SOON_MIN 3

#endif // _INCLUDE_WEB_HPP_
