#ifndef _INCLUDE_OTA_HPP_
#define _INCLUDE_OTA_HPP_

// Aktualizacja programu przez Wi-Fi (OTA), od 2026-11-08.
// Pamięć ma dwa miejsca na program (partycje_ota.csv): app0 i app1. Plik .bin wysłany przez stronę
// (web.cpp, POST /api/aktualizacja) trafia do miejsca, z którego zegar teraz nie działa; potem restart.
// Nowa wersja startuje jako „do sprawdzenia” (rollback w bootloaderze, sdkconfig.esp32dev):
//  - potwierdza się sama, gdy działa OTA_CONFIRM_S sekund, Wi-Fi jest połączone i strona działa,
//  - gdy się wysypie przed potwierdzeniem, bootloader przy następnym starcie wraca do poprzedniej wersji,
//  - gdy przez OTA_GIVE_UP_S nie da się jej potwierdzić (np. nowa wersja nie łączy się z Wi-Fi),
//    zegar sam wraca do poprzedniej wersji.
// Restart po aktualizacji i powrót nigdy w czasie alarmu i drzemki.

#include <stdbool.h>

#define OTA_CONFIRM_S  60
#define OTA_GIVE_UP_S  600

namespace Ota {
    void init(void);                         // przy starcie: stan bieżącej wersji do logu
    // Z pętli zegara: potwierdzenie nowej wersji, powrót do starej, restart po aktualizacji
    void check(bool wifi_connected, bool web_running, bool alarm_idle);
    void requestRestart(void);               // aktualizacja wgrana: restart, gdy alarm pozwoli
    bool isRestartPending(void);
    const char *runningLabel(void);          // "app0" / "app1"
    bool isPendingVerify(void);              // nowa wersja jeszcze niepotwierdzona
    bool wasRolledBack(void);                // ostatnia aktualizacja nie zadziałała, działa poprzednia wersja
}

#endif // _INCLUDE_OTA_HPP_
