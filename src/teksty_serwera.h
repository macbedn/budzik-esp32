// Teksty, które zegar wysyła stronie ustawień (web.cpp), w jednym miejscu (łatwiej poprawić albo przetłumaczyć).
// Komunikaty błędów: pełne zdania, wielką literą, z kropką. Wartości do tabeli „Stan zegara”: małą literą, bez kropki.
// Zegar w trzeciej osobie („Zegar…”), tak jak na całej stronie.
#ifndef _INCLUDE_TEKSTY_SERWERA_H_
#define _INCLUDE_TEKSTY_SERWERA_H_

// Pogoda w pasku stanu (wartość, gdy nie ma pogody)
#define TXT_POGODA_BLAD          "nie udało się pobrać, ponowna próba co minutę"
#define TXT_POGODA_CZEKAM        "jeszcze nie pobrana"
#define TXT_POGODA_STARA         "brak danych pogodowych od ponad 2 godzin"  // strona pokazuje na czerwono

// „Ostatnie uruchomienie” (esp_reset_reason). Te z *bad = true strona pokazuje na czerwono.
#define TXT_START_ZASILANIE      "po włączeniu lub zaniku prądu"
#define TXT_START_RESET          "po wciśnięciu przycisku reset"
#define TXT_START_PROGRAM        "po zapisie Wi-Fi lub aktualizacji"
#define TXT_START_USPIENIE       "po wybudzeniu z uśpienia"
#define TXT_START_AWARIA         "po awarii programu"
#define TXT_START_ZAWIESZENIE    "po zawieszeniu programu"
#define TXT_START_NAPIECIE       "po spadku napięcia (zasilacz?)"
#define TXT_START_NIEZNANA       "z nieznanej przyczyny"

// Błędy ogólne
#define ERR_ZAPIS                "Nie udało się zapisać. Spróbuj jeszcze raz."
#define ERR_TEMPERATURA          "Nie wybrano temperatury."
#define ERR_JASNOSC              "Błędne ustawienia jasności. Odśwież stronę i spróbuj jeszcze raz."
#define ERR_DRZEMKA              "Drzemka może trwać od 5 do 9 minut."
#define ERR_NAGLOWEK             "Zapytanie odrzucone: brakuje nagłówka X-Budzik. Zmieniaj ustawienia na stronie zegara albo dodaj do polecenia -H \"X-Budzik: 1\"."

// Miejscowość
#define ERR_NAZWA_MIASTA         "Wpisz nazwę miejscowości (najwyżej 40 znaków)."
#define ERR_SZEROKOSC            "Szerokość geograficzna musi być liczbą od -90 do 90, z kropką (np. 52.2297)."
#define ERR_DLUGOSC              "Długość geograficzna musi być liczbą od -180 do 180, z kropką (np. 21.0122)."
#define ERR_SZUKAJ_PUSTE         "Wpisz nazwę miejscowości."
#define ERR_SZUKAJ_BEZ_INTERNETU "Zegar nie ma internetu. Najpierw ustaw Wi-Fi albo wpisz współrzędne ręcznie."
#define ERR_SZUKAJ_SERWIS        "Nie udało się połączyć z serwisem Open-Meteo. Spróbuj za chwilę."

// Aktualizacja programu
#define ERR_OTA_JUZ_WGRANA       "Nowa wersja jest już wgrana. Zegar zaraz uruchomi się ponownie."
#define ERR_OTA_ALARM            "Alarm dzwoni, trwa drzemka albo alarm zadzwoni w ciągu 3 minut. Wgraj program później."
#define ERR_OTA_BRAK_MIEJSCA     "Zegar nie ma miejsca na drugą wersję programu. Wgraj program jeden raz przez kabel USB, potem aktualizacja przez Wi-Fi będzie działać."
#define ERR_OTA_NIE_TEN_PLIK     "To nie jest program tego zegara. Wybierz plik firmware.bin z folderu projektu."
#define ERR_OTA_PRZERWANE        "Przesyłanie zostało przerwane. Sprawdź połączenie i spróbuj jeszcze raz."
#define ERR_OTA_SPRAWDZANA       "Zegar jeszcze sprawdza poprzednią aktualizację. Spróbuj za minutę."
#define ERR_OTA_PAMIEC           "Nie udało się przygotować pamięci na nową wersję."
#define ERR_OTA_ZAPIS            "Błąd zapisu do pamięci zegara."
#define ERR_OTA_USZKODZONY       "Plik jest uszkodzony albo nie jest programem tego zegara."
#define ERR_OTA_KONIEC           "Nie udało się zakończyć zapisu nowej wersji."
#define ERR_OTA_USTAW            "Nie udało się ustawić nowej wersji jako startowej."

// Wi-Fi (tylko w sieci zegara „Budzik-ustawienia”)
#define ERR_WIFI_TYLKO_AP        "Sieć Wi-Fi można zmienić tylko po połączeniu z siecią zegara „Budzik-ustawienia”."
#define ERR_WIFI_SKAN            "Nie udało się wyszukać sieci. Spróbuj za chwilę."
#define ERR_WIFI_NAZWA           "Nazwa sieci musi mieć od 1 do 32 znaków."
#define ERR_WIFI_HASLO           "Hasło musi mieć od 8 do 64 znaków. Dla sieci bez hasła zostaw pole puste."

#endif // _INCLUDE_TEKSTY_SERWERA_H_
