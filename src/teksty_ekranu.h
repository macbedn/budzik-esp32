// Napisy na ekranie zegara, w jednym miejscu (łatwiej poprawić albo przetłumaczyć).
// Czcionka Lato 11,25 pt (FONT_WEATHER), ekran ma 320 px szerokości: w nawiasie zmierzona szerokość
// z tuszem (skrypt: szerokości znaków z src/Lato11_25pt7b.h). Po zmianie napisu zmierzyć go ponownie.
// Na ekranie mówi sam zegar, więc pierwsza osoba („czekam”, „pobieram”). Bez kropek na końcu.
// Opisy pogody (getWeatherCondition) są w weather.cpp: muszą zmieścić się w całej linii pogody (najwyżej 315 px).
#ifndef _INCLUDE_TEKSTY_EKRANU_H_
#define _INCLUDE_TEKSTY_EKRANU_H_

// Zegar nie zna godziny (linia pogody)
#define TXT_BRAK_SIECI_ALARM   "brak sieci, alarm nie zadzwoni"    // 282 px, na czerwono
#define TXT_CZEKAM_NA_SIEC     "czekam na sieć Wi-Fi"
#define TXT_POBIERAM_GODZINE   "pobieram godzinę z sieci..."

// Linia pogody: komunikaty
#define TXT_JAK_USTAWIC_WIFI   "trzymaj pokrętło przy włączaniu"   // 307 px, po dwukliku bez adresu IP
#define TXT_BRAK_WIFI_DLUGO    "brak Wi-Fi od kilku godzin"
#define TXT_POGODA_STARA       "brak danych pogodowych"            // 242 px, szary; po 2 h bez udanego pobrania pogody
#define TXT_BRAK_DANYCH        "brak danych"                     // pogoda: nieznany kod albo jeszcze nie pobrana

// Tryb ustawień (ekran z kodem QR)
#define TXT_USTAWIENIA_TYTUL   "Ustawienia zegara"
#define TXT_USTAWIENIA_SIEC    "sieć Wi-Fi:"
#define TXT_USTAWIENIA_HASLO   "hasło:"
#define TXT_USTAWIENIA_STRONA  "strona: %s"
#define TXT_WYJSCIE            "przytrzymaj pokrętło, aby wyjść"   // 302 px
#define TXT_ZLA_SIEC           "błędna nazwa sieci lub hasło"      // 273 px, na czerwono; ten sam napis cytuje strona
#define TXT_ZAPISANO_RESTART   "zapisano, uruchamiam ponownie"     // 310 px

#endif // _INCLUDE_TEKSTY_EKRANU_H_
