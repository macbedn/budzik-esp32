
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <display.hpp>
#include <math.h>
#include "settings.hpp"  // harmonogram jasności
#include "teksty_ekranu.h"
#include <Lato63pt7b.h> // główny zegar
#include <Lato19pt7b.h> // cyfry dolnego rzędu (godziny budzika, drzemki i snu)
#include <Ikony26pt7b.h> // ikony (Wi-Fi, budzik, drzemka, łóżko, brak dźwięku)
#include <Lato11_25pt7b.h> // pogoda


// SMOOTH_FONTS 1: wszystkie napisy i ikony wygładzone (VLW, 8 bitów na piksel, od 2026-11-07), 0: zwykłe 1-bitowe.
// Wygładzone potrzebują większego stosu głównego zadania (rysowanie kładzie cały znak na stosie, do 5,9 KB
// przy dużej godzinie): CONFIG_ESP_MAIN_TASK_STACK_SIZE=12288 w sdkconfig.esp32dev.
// Wygładzone mają te same szerokości znaków i linie bazowe co 1-bitowe, więc układ ekranu jest ten sam.
#define SMOOTH_FONTS      1

// Duża godzina (Lato 63 pt, cyfry 90 px wysokości): dół cyfr na y = 112, linia bazowa 111.
// FONT_CLOCK (1-bitowa) zawsze daje położenie cyfr (xAdvance).
#define FONT_CLOCK        (&Lato63pt7b)
#if SMOOTH_FONTS
#include <Lato63vlw.h>
#include <Lato19vlw.h>
#include <Lato11_25vlw.h>
#include <Ikony31vlw.h>
static lgfx::PointerWrapper clock_font_data(Lato63vlw, sizeof(Lato63vlw));
static lgfx::PointerWrapper lower_font_data(Lato19vlw, sizeof(Lato19vlw));
static lgfx::PointerWrapper weather_font_data(Lato11_25vlw, sizeof(Lato11_25vlw));
static lgfx::PointerWrapper icons_font_data(Ikony31vlw, sizeof(Ikony31vlw));
static lgfx::VLWfont clock_font_smooth, lower_font_smooth, weather_font_smooth, icons_font_smooth;
#define FONT_CLOCK_DRAW   (&clock_font_smooth)
#define FONT_LOWER_DIGITS (&lower_font_smooth)
#define FONT_WEATHER      (&weather_font_smooth)
#define FONT_ICONS        (&icons_font_smooth)
// Punkty rysowania liczone tak, żeby linie bazowe były jak w 1-bitowych: godzina 111, pogoda 159,
// cyfry dolnego rzędu 223 (środek cyfr y = 211), ikony 231 (środek ikon y = 211).
// Napis "middle": biblioteka odejmuje połowę wysokości (ascent + descent) czcionki.
#define CLOCK_Y           (111 - Lato63vlw_ASCENT)
#define WEATHER_Y         (159 - Lato11_25vlw_ASCENT)
#define LOWER_DIGITS_Y    (223 - Lato19vlw_ASCENT + ((Lato19vlw_ASCENT + Lato19vlw_DESCENT) >> 1))
#define ICONS_Y           (231 - Ikony31vlw_ASCENT + ((Ikony31vlw_ASCENT + Ikony31vlw_DESCENT) >> 1))
#else
// Czcionki dolnego rzędu: cyfry i ikony osobno, żeby zmiana cyfr nie ruszała ikon.
#define FONT_CLOCK_DRAW   FONT_CLOCK
#define FONT_LOWER_DIGITS (&Lato19pt7b)
#define FONT_WEATHER      (&Lato11_25pt7b)
#define FONT_ICONS        (&Ikony26pt7b)
#define CLOCK_Y           23
// Pogoda (Lato 11,25 pt): górny punkt 139, linia bazowa na y = 159.
#define WEATHER_Y         139
// Środek cyfr dolnego rzędu (biblioteka liczy wysokość z samych cyfr, bez wysokich znaków czcionki).
// Lato 19 pt: cyfry 26 px, y 198..223.
#define LOWER_DIGITS_Y    211
// Ikony: punkt rysowania, przy którym środek każdej ikony wypada na y = 211, czyli na środku cyfr.
// Biblioteka liczy wysokość 35 px, linię bazową 34 px, a środek ikon jest 20 px nad linią bazową (Ikony26pt7b.h).
#define ICONS_Y           214
#endif
// Położenie cyfr: ok. 8-9 px odstępu od ikon po obu stronach (dzwonek do x 46, ikony drzemki/łóżka do x 186, Wi-Fi od x 272).
#define ALARM_DIGITS_X    53   // lewa krawędź "07:00" (94 px szerokości)
#define ALARM_DIGITS_W    94
#define BED_DIGITS_X      228  // środek "6:30"
#define SNOOZE_DIGITS_X   223  // środek "5:00"
// Pole ikony drzemki: x 146..180 (ikona 153..179). Z lewej cyfry alarmu kończą się na x 145, z prawej
// tło czasu drzemki zaczyna się na x 181.
#define SNOOZE_ICON_AREA_X 146
#define SNOOZE_ICON_AREA_W 35

// Układ ekranu (2026-11-07, czcionka Lato), piksele z tuszem (1-bitowe; wygładzone mają do 1 px miękkiego brzegu): godzina y 23..112, pogoda 139..163 (cyfry 145..159),
// paski drzemki 175..179, dolny rząd: cyfry 198..223, ikony (zależnie od wysokości) najwyżej 197..225, środek 211.
// Godzina jest niższa niż w Roboto Condensed (90 zamiast 102 px), bo Lato jest szersza; dół w tym samym miejscu.
// Względem wersji 033: pogoda +5, paski +10, dolny rząd +15 w dół i 13 px w lewo (margines 21 px z lewej, 20 px z prawej).
// Pole dzwonka budzika: x 12..46, y 194..229, z zapasem (czerwona ikona "brak dźwięku" pokazywana w tym samym miejscu).
#define ALARM_SYMBOL_X      12
#define ALARM_SYMBOL_AREA_W 37  // z zapasem na wygładzony brzeg dzwonka (do x 47)

// Zegar nie zna godziny: "--:--". Kreski rysowane jako prostokąty (czcionki mają tylko cyfry i ":"),
// wymiary jak myślnik Lato 63 pt (31 x 9 px), na środku pola cyfry (71 px) i na wysokości środka dwukropka (y 67,5).
#define NO_TIME_DASH_W   31
#define NO_TIME_DASH_H   9
#define NO_TIME_DASH_Y   63
#define NO_TIME_WAKE_US  (60 * 1000000LL)  // ekran świeci 60 s od włączenia lub ostatniego dotknięcia, potem 0
#define NO_TIME_MAX_LEVEL 3                 // dotknięcia rozjaśniają najwyżej do poziomu dziennego

void Display::drawAlarmSymbol(const char *symbol, uint16_t color) {
    alarm_symbol[0] = symbol[0];
    alarm_symbol[1] = '\0';
    alarm_symbol_color = color;
    if (audio_offline)
        return;  // w tym miejscu jest teraz ikona "brak dźwięku"
    // Dzwonki "A", "B" i "@" sięgają do x 47, a tło "?" tylko do x 46: po dzwonieniu zostawały słabe piksele
    lcd.fillRect(ALARM_SYMBOL_X + ALARM_SYMBOL_AREA_W - 2, 194, 2, 36, TFT_BLACK);
    lcd.setTextColor(color, TFT_BLACK);
    lcd.setTextDatum(middle_left);
    lcd.drawString(alarm_symbol, ALARM_SYMBOL_X, ICONS_Y, FONT_ICONS);
}

void Display::monitorBrightnessTask(void *pvParameter) {
    Display *pThis = (Display *)pvParameter;
    bool event;
    while (1) {
        xQueueReceive(pThis->queue, &event, 1000 / portTICK_PERIOD_MS);
        pThis->controlBrightness();
    }
}

void Display::init(void) {
    lcd.init();
    lcd.setRotation(0);
    lcd.setColorDepth(16);
#if SMOOTH_FONTS
    if (!clock_font_smooth.loadFont(&clock_font_data) || !lower_font_smooth.loadFont(&lower_font_data)
        || !weather_font_smooth.loadFont(&weather_font_data) || !icons_font_smooth.loadFont(&icons_font_data))
        ESP_LOGE("display", "Nie wczytano wygładzonych czcionek");
#endif

    // Czujnik światła (ADC) odpuszczony 2026-11-03: obudowa nie ma na niego otworu, jasność wg harmonogramu

    // Ustaw początkowy poziom
    display_brightness_level = 2;
    setBrightness(display_brightness_level);

    // Kolejka przed zadaniem, które z niej czyta
    queue = xQueueCreate(1, sizeof(bool));
    xTaskCreate(this->monitorBrightnessTask, "monitor_brightness_task", 3072, this, 1, NULL);  // 3072: raz na dobę wschód/zachód z wpisem do logu (sun.cpp)
}

// Ekran trybu ustawień: na górze tytuł, z lewej kod QR
// sieci zegara (120 px), z prawej jej nazwa i hasło, niżej adres strony, na dole stan (np. "sprawdzam sieć...").
#define SETUP_QR_X      8
#define SETUP_QR_Y      38
#define SETUP_QR_W      120
#define SETUP_TEXT_X    138
#define SETUP_STATUS_Y  206  // górny brzeg napisu

void Display::clearScreen(void) {
    lcd.fillScreen(TFT_BLACK);
}

void Display::updateContent(display_element_t element, void *value, display_action_t action) {
    // W trybie ustawień zwykłe elementy (godzina co minutę, pogoda...) nie przykrywają ekranu z kodem QR
    if (setup_screen && element != D_E_SETUP && element != D_E_SETUP_STATUS)
        return;
    switch (element) {
        case D_E_SETUP: {
            if (action == D_A_OFF) {
                setup_screen = false;
                lcd.fillScreen(TFT_BLACK);  // zwykły ekran rysuje potem ClockMachine::redrawScreen()
                bool queue_event = true;
                xQueueSend(queue, &queue_event, 0);
                break;
            }
            const setup_screen_t *info = static_cast<const setup_screen_t *>(value);
            setup_screen = true;
            lcd.fillScreen(TFT_BLACK);
            lcd.setTextDatum(top_center);
            lcd.setTextColor(TFT_WHITE, TFT_BLACK);
            lcd.drawString(TXT_USTAWIENIA_TYTUL, lcd.width() / 2, 6, FONT_WEATHER);
            // Kod QR sieci Wi-Fi: telefon po zeskanowaniu sam się łączy
            char qr[96];
            snprintf(qr, sizeof(qr), "WIFI:T:WPA;S:%s;P:%s;;", info->ssid, info->password);
            lcd.qrcode(qr, SETUP_QR_X, SETUP_QR_Y, SETUP_QR_W, 1, true);
            lcd.setTextDatum(top_left);
            lcd.setTextColor(TFT_DARKGRAY, TFT_BLACK);
            lcd.drawString(TXT_USTAWIENIA_SIEC, SETUP_TEXT_X, SETUP_QR_Y, FONT_WEATHER);
            lcd.drawString(TXT_USTAWIENIA_HASLO, SETUP_TEXT_X, SETUP_QR_Y + 62, FONT_WEATHER);
            lcd.setTextColor(TFT_WHITE, TFT_BLACK);
            lcd.drawString(info->ssid, SETUP_TEXT_X, SETUP_QR_Y + 26, FONT_WEATHER);
            lcd.drawString(info->password, SETUP_TEXT_X, SETUP_QR_Y + 88, FONT_WEATHER);
            char address[48];
            snprintf(address, sizeof(address), TXT_USTAWIENIA_STRONA, info->address);
            lcd.setTextDatum(top_center);
            lcd.drawString(address, lcd.width() / 2, SETUP_QR_Y + SETUP_QR_W + 12, FONT_WEATHER);
            if (info->status)
                updateContent(D_E_SETUP_STATUS, (void *)info->status, D_A_ON);
            bool queue_event = true;
            xQueueSend(queue, &queue_event, 0);  // od razu jasno
            break;
        }

        case D_E_SETUP_STATUS:
            if (!setup_screen) break;
            lcd.fillRect(0, SETUP_STATUS_Y - 2, lcd.width(), 30, TFT_BLACK);
            lcd.setTextDatum(top_center);
            lcd.setTextColor(action == D_A_WARNING ? TFT_RED : TFT_DARKGRAY, TFT_BLACK);
            lcd.drawString(static_cast<char *>(value), lcd.width() / 2, SETUP_STATUS_Y, FONT_WEATHER);
            break;

        case D_E_TIME:
            char time_buf[8];
            snprintf(time_buf, sizeof(time_buf), "%02d:%02d", static_cast<clock_time_t *>(value)->hour, static_cast<clock_time_t *>(value)->minute);
            // Godzinę rysujemy w trzech kawałkach ("HH", ":", "MM"), żeby migający dwukropek
            // trafiał dokładnie w to samo miejsce co przy pełnym przerysowaniu.
            char hours_buf[3], minutes_buf[3];
            hours_buf[0] = time_buf[0]; hours_buf[1] = time_buf[1]; hours_buf[2] = '\0';
            minutes_buf[0] = time_buf[3]; minutes_buf[1] = time_buf[4]; minutes_buf[2] = '\0';
            // Położenie stałe, liczone z xAdvance znaków (cyfry 71 px, dwukropek 31 px, razem 315 px), na środku ekranu.
            // textWidth() się nie nadaje: wlicza piksele wystające za ostatni znak (np. "20" = 146, nie 144).
            int32_t hours_x, colon_x, minutes_x, minutes_end_x;
            {
                const GFXglyph *glyphs = FONT_CLOCK->glyph;
                const uint8_t first = FONT_CLOCK->first;
                int32_t digit_advance = glyphs['0' - first].xAdvance;
                int32_t colon_advance = glyphs[':' - first].xAdvance;
                hours_x = (lcd.width() - (4 * digit_advance + colon_advance)) / 2;
                colon_x = hours_x + 2 * digit_advance;
                minutes_x = colon_x + colon_advance;
                minutes_end_x = minutes_x + 2 * digit_advance;
            }
            lcd.setTextDatum(top_left);

            if (time_unknown) {
                // "--:--" w kolorze ciemnoszarym, dwukropek nie miga
                if (action == D_A_SHOW_COLON || action == D_A_HIDE_COLON)
                    return;
                const GFXglyph *glyphs = FONT_CLOCK->glyph;
                int32_t digit_advance = glyphs['0' - FONT_CLOCK->first].xAdvance;
                lcd.fillRect(0, 0, lcd.width(), 130, TFT_BLACK);
                int32_t dash_offset = (digit_advance - NO_TIME_DASH_W) / 2;
                int32_t cells[4] = {hours_x, hours_x + digit_advance, minutes_x, minutes_x + digit_advance};
                for (int i = 0; i < 4; i++)
                    lcd.fillRect(cells[i] + dash_offset, NO_TIME_DASH_Y, NO_TIME_DASH_W, NO_TIME_DASH_H, TFT_DARKGRAY);
                lcd.setTextColor(TFT_DARKGRAY);
                lcd.drawString(":", colon_x, CLOCK_Y, FONT_CLOCK_DRAW);
                return;
            }

            switch (action) {
                case D_A_ON:
                    if (display_brightness_level == 1) {
                         lcd.setTextColor(TFT_DARKGRAY, TFT_BLACK); // Ciemnoszary dla poziomu nocnego (1: rano przed rozjaśnieniem, wieczorem przed snem)
                    } else {
                         lcd.setTextColor(TFT_WHITE, TFT_BLACK);    // Biały dla reszty dnia
                    }
                //    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
                    break;
                case D_A_SHOW_COLON:
                case D_A_HIDE_COLON: {
                    // Rysujemy sam dwukropek, bez tła (tylko jego piksele), żeby nie ruszać cyfr.
                    uint16_t color = TFT_BLACK;
                    if (action == D_A_SHOW_COLON)
                        color = (display_brightness_level == 1) ? TFT_DARKGRAY : TFT_WHITE;
                    lcd.setTextColor(color);
                    lcd.drawString(":", colon_x, CLOCK_Y, FONT_CLOCK_DRAW);
                    return;
                }
                default:
                    break;
            }
            // Pasek za minutami: tło cyfry zamalowuje tylko jej pole (xAdvance).
            lcd.fillRect(minutes_end_x, 0, lcd.width() - minutes_end_x, 130, TFT_BLACK);
            lcd.drawString(hours_buf, hours_x, CLOCK_Y, FONT_CLOCK_DRAW);
            lcd.drawString(":", colon_x, CLOCK_Y, FONT_CLOCK_DRAW);
            lcd.drawString(minutes_buf, minutes_x, CLOCK_Y, FONT_CLOCK_DRAW);
            break;

        case D_E_WEATHER:
            char weather_buf[64];
            strcpy(weather_buf, static_cast<char *>(value));
    
            // Na środku: cały ekran (320 px) do dyspozycji. Najdłuższy wariant
            // ("-30°C | 1000 hPa | marzn.deszcz", 315 px) mieści się z 2 px zapasu z każdej strony.
            lcd.setTextDatum(top_center);
            
            if (display_brightness_level == 1) {
                     lcd.setTextColor(TFT_DARKGRAY, TFT_BLACK);
            } else {
                         lcd.setTextColor(TFT_WHITE, TFT_BLACK);
            }

            //lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    
            switch (action) {
                case D_A_OFF:
                    lcd.fillRect(0, 138, lcd.width(), 30, TFT_BLACK);
                break;
                case D_A_ON:
                    lcd.fillRect(0, 138, lcd.width(), 30, TFT_BLACK);
                    lcd.drawString(weather_buf, lcd.width() / 2, WEATHER_Y, FONT_WEATHER);
                break;
            default:
                break;
    }
    break;

        case D_E_MESSAGE:
            // Komunikat w miejscu pogody, na środku: D_A_ON szary, D_A_WARNING czerwony, D_A_OFF czyści wiersz
            lcd.fillRect(0, 138, lcd.width(), 30, TFT_BLACK);
            if (action == D_A_ON || action == D_A_WARNING) {
                lcd.setTextDatum(top_center);
                lcd.setTextColor(action == D_A_WARNING ? TFT_RED : TFT_DARKGRAY, TFT_BLACK);
                lcd.drawString(static_cast<char *>(value), lcd.width() / 2, WEATHER_Y, FONT_WEATHER);
            }
            break;

        case D_E_ALARM_TIME:
            char alarm_buf[8];
            snprintf(alarm_buf, sizeof(alarm_buf), "%02d:%02d", static_cast<clock_time_t *>(value)->hour, static_cast<clock_time_t *>(value)->minute);
            char alarm_symbol_buf[2];
            snprintf(alarm_symbol_buf, sizeof(alarm_symbol_buf), DISPLAY_SYMBOL_ALARM_ON);
            lcd.setTextColor(TFT_WHITE, TFT_BLACK);  // Normal case
			lcd.setTextDatum(middle_left); // zmiana
            switch (action) {
                case D_A_OFF:
                    // lcd.fillRect(25, 180, 35, 40, TFT_WHITE); // wyczyść białym
                    lcd.setTextColor(TFT_DARKGRAY, TFT_BLACK); 
                    snprintf(alarm_symbol_buf, sizeof(alarm_symbol_buf), DISPLAY_SYMBOL_ALARM_OFF);
                    // Pięć spacji byłoby szersze od "07:00" i ucięłoby ikonę drzemki lub łóżka.
                    lcd.fillRect(ALARM_DIGITS_X, 194, ALARM_DIGITS_W, 36, TFT_BLACK);
                    alarm_buf[0] = '\0';
                    break;
                case D_A_HIDE_HOURS:
                    alarm_buf[0] = alarm_buf[1] = ' ';
                    break;
                case D_A_HIDE_MINUTES:
                    alarm_buf[3] = alarm_buf[4] = ' ';
                    break;
                default:
                    break;
            }
            {
                uint16_t alarm_color = (action == D_A_OFF) ? TFT_DARKGRAY : TFT_WHITE;
                drawAlarmSymbol(alarm_symbol_buf, alarm_color);
                lcd.setTextColor(alarm_color, TFT_BLACK);
                lcd.setTextDatum(middle_left);
            }
            lcd.drawString(alarm_buf, ALARM_DIGITS_X, LOWER_DIGITS_Y, FONT_LOWER_DIGITS);
            break;

        case D_E_BED_TIME:
            char bed_time_buf[8];
            snprintf(bed_time_buf, sizeof(bed_time_buf), "%01d:%02d", static_cast<clock_time_t *>(value)->hour, static_cast<clock_time_t *>(value)->minute);
            char bed_time_symbol_buf[3];
            snprintf(bed_time_symbol_buf, sizeof(bed_time_symbol_buf), DISPLAY_SYMBOL_BED);
            lcd.setTextColor(TFT_LIGHTGRAY, TFT_BLACK);  // Normal case
			lcd.setTextDatum(middle_center);
            // We show the remaining bed time only when less than 9 hours
            // Bez godziny z internetu czas do budzika byłby błędny: ukryty.
            if (static_cast<clock_time_t *>(value)->hour >= 9 || time_unknown)
                action = D_A_OFF;

            switch (action) {
                case D_A_OFF:
                    snprintf(bed_time_buf, sizeof(bed_time_buf), "    ");
                    snprintf(bed_time_symbol_buf, sizeof(bed_time_symbol_buf), "  ");
                    break;
                default:
                    break;
            }
            lcd.drawString(bed_time_symbol_buf, 167, ICONS_Y, FONT_ICONS);
            lcd.drawString(bed_time_buf, BED_DIGITS_X, LOWER_DIGITS_Y, FONT_LOWER_DIGITS);
            break;

        case D_E_SNOOZE_TIME:
            char snooze_buf[8];
            lcd.setTextDatum(middle_center);
            lcd.setTextColor(TFT_ORANGE, TFT_BLACK);
            switch (action) {
                case D_A_OFF:
                    snprintf(snooze_buf, sizeof(snooze_buf), "    ");  // cztery spacje: piąta ucięłaby ikonę Wi-Fi
                    // Ikona drzemki czyszczona prostokątem: dwie spacje miały tło od x 141 i ucinały
                    // ostatnią cyfrę alarmu (kończy się na x 145), gdy po drzemce budzik dzwonił znowu.
                    lcd.fillRect(SNOOZE_ICON_AREA_X, 194, SNOOZE_ICON_AREA_W, 36, TFT_BLACK);
                    break;
                case D_A_ON:
                    uint8_t minutes;
                    uint8_t seconds;
                    uint16_t remaining_seconds;
                    remaining_seconds = *(static_cast<uint16_t *>(value));
                    minutes = remaining_seconds / 60;
                    seconds = remaining_seconds % 60;
                    snprintf(snooze_buf, sizeof(snooze_buf), "%01d:%02d", minutes, seconds);
                    // Tło ikony zaczyna się na x 145 i zjadało prawą kolumnę "4" w minutach alarmu: obcięte od x 146
                    lcd.setClipRect(SNOOZE_ICON_AREA_X, 0, lcd.width() - SNOOZE_ICON_AREA_X, lcd.height());
                    lcd.drawString(DISPLAY_SYMBOL_SNOOZE, 162, ICONS_Y, FONT_ICONS);
                    lcd.clearClipRect();
                    break;
                default:
                    break;
            }
            lcd.drawString(snooze_buf, SNOOZE_DIGITS_X, LOWER_DIGITS_Y, FONT_LOWER_DIGITS);
            break;

        default:
            break;
    }
}

void Display::updateContent(display_element_t element, display_action_t action) {
    if (setup_screen)
        return;
    switch (element) {
        case D_E_ALARM_ACTIVE:
            char alarm_active_symbol_buf[2];
            switch (action) {
                case D_A_OFF:
                    snprintf(alarm_active_symbol_buf, sizeof(alarm_active_symbol_buf), DISPLAY_SYMBOL_ALARM_L);
                    break;
                case D_A_ON:
                    snprintf(alarm_active_symbol_buf, sizeof(alarm_active_symbol_buf), DISPLAY_SYMBOL_ALARM_R);
                    break;
                default:
                    break;
            }
            drawAlarmSymbol(alarm_active_symbol_buf, TFT_WHITE);
            break;

        case D_E_SNOOZE_CANCEL:
            switch (action) {
                case D_A_OFF:
                    // Clear all the bars
                    lcd.setColor(TFT_BLACK);
                    // lcd.fillRect(160, 170, 75, 5);
                    lcd.fillRect(0, 175, 320, 5);  // cały ekran
                    break;
                case D_A_ONE_BAR:
                    // Draw only the first bar
                    lcd.setColor(TFT_ORANGE);
                    // lcd.fillRect(160, 170, 35, 5);
                    lcd.fillRect(0, 175, (lcd.width() / 2) - 5, 5);
                    break;
                case D_A_TWO_BARS:
                    // Draw only the second bar
                    lcd.setColor(TFT_ORANGE);
                    // lcd.fillRect(200, 170, 35, 5);
                    lcd.fillRect((lcd.width() / 2) + 5, 175, (lcd.width() / 2) - 5, 5);
                    break;
                default:
                    // There is no "case 3" where all 3 bars are shown
                    break;
            }
            break;

        case D_E_WIFI_STATUS:
            lcd.setTextDatum(middle_center);
            switch (action) {
                case D_A_OFF:
                    lcd.setTextColor(TFT_RED, TFT_BLACK);
                    lcd.drawString(DISPLAY_SYMBOL_WIFI_OFF, 282, ICONS_Y, FONT_ICONS);
                    break;
                case D_A_ON:
                    lcd.setTextColor(TFT_DARKGRAY, TFT_BLACK);
                    lcd.drawString(DISPLAY_SYMBOL_WIFI_ON, 282, ICONS_Y, FONT_ICONS);
                    break;
                default:
                    break;
            }
            break;

        case D_E_AUDIO:
            // Brak dźwięku: czerwona ikona w miejscu dzwonka (budzik i tak by nie zadzwonił).
            // Po powrocie dźwięku przywracamy ostatni narysowany dzwonek.
            lcd.setColor(TFT_BLACK);
            lcd.fillRect(ALARM_SYMBOL_X, 194, ALARM_SYMBOL_AREA_W, 36);
            switch (action) {
                case D_A_OFF:
                    audio_offline = true;
                    lcd.setTextDatum(middle_center);
                    lcd.setTextColor(TFT_RED, TFT_BLACK);
                    lcd.drawString(DISPLAY_SYMBOL_AUDIO_OFF, 29, ICONS_Y, FONT_ICONS);
                    break;
                default:
                    audio_offline = false;
                    if (alarm_symbol[0] != '\0')
                        drawAlarmSymbol(alarm_symbol, alarm_symbol_color);
                    break;
            }
            break;

        default:
            break;
    }
}

void Display::controlBrightness(void) {
    if (setup_screen) {
        // Ekran z kodem QR ma być dobrze widoczny o każdej porze, także bez godziny (pierwsze ustawienie)
        setBrightness(3);
        return;
    }

    if (time_unknown) {
        // Bez godziny nie wiadomo, czy jest noc: po włączeniu poziom 1, każde dotknięcie rozjaśnia o poziom
        // (najwyżej do 3), 60 s po ostatnim dotknięciu ekran gaśnie
        display_brightness_level = (esp_timer_get_time() - wake_timestamp_us < NO_TIME_WAKE_US) ? wake_level : 0;
        setBrightness(display_brightness_level);
        return;
    }

    // Jasność wg przepisu ze strony ustawień (osobno dni robocze i wolne, settings.hpp): rano i wieczorem płynnie
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    float minute = timeinfo.tm_hour * 60 + timeinfo.tm_min + timeinfo.tm_sec / 60.0f;
    uint8_t raw = rawFromPoint(Settings::brightnessAt(timeinfo.tm_wday, minute));
    // Poziom: najwyższy, którego jasność nie przekracza obecnej (w trakcie rozjaśniania np. 1, potem 2, potem 3).
    // Od niego zależy kolor napisów (nocny: szare) i rozjaśnienie po dotknięciu (o poziom w górę).
    uint8_t level = 0;
    for (uint8_t l = 1; l < DISPLAY_BRIGHTNESS_LEVELS_NR; l++)
        if (raw >= display_light_brightness[l]) level = l;
    display_brightness_level = level;

    // Obsłuż żądania ręcznej zmiany (jeśli są)
    if (max_brightness_requested) {
        setBrightness(DISPLAY_BRIGHTNESS_LEVELS_NR);
    } else if (increased_brightness_requested) {
        setBrightness(display_brightness_level + 1);
    } else {
        setBrightnessRaw(raw);
    }
}

// Jasność widziana okiem (CIE L*, 0..100) dla wartości LovyanGFX: PWM podświetlenia = 2 x wartość z 512
static float lstarFromRaw(uint8_t raw) {
    float y = raw / 256.0f;
    return (y > 0.008856f) ? 116.0f * cbrtf(y) - 16.0f : 903.3f * y;
}

// Między poziomami płynnie wg L* (tak samo rosną słupki wykresu na stronie), zaokrąglone do wartości LovyanGFX
uint8_t Display::rawFromPoint(bright_point_t point) {
    if (point.from == point.to || point.from > DISPLAY_BRIGHTNESS_LEVELS_NR || point.to > DISPLAY_BRIGHTNESS_LEVELS_NR)
        return display_light_brightness[point.to <= DISPLAY_BRIGHTNESS_LEVELS_NR ? point.to : 0];
    uint8_t a = display_light_brightness[point.from], b = display_light_brightness[point.to];
    float l = lstarFromRaw(a) + (lstarFromRaw(b) - lstarFromRaw(a)) * point.t;
    float y = (l > 8.0f) ? powf((l + 16.0f) / 116.0f, 3) : l / 903.3f;
    int raw = (int)lroundf(y * 256.0f);
    uint8_t lo = a < b ? a : b, hi = a < b ? b : a;  // nie wychodzimy poza dwa poziomy (zaokrąglenia)
    if (raw < lo) raw = lo;
    if (raw > hi) raw = hi;
    return (uint8_t)raw;
}

void Display::setBrightness(uint8_t brightness_level) {
    // Ogranicz poziom do dostępnego zakresu
    if (brightness_level > DISPLAY_BRIGHTNESS_LEVELS_NR)
        brightness_level = DISPLAY_BRIGHTNESS_LEVELS_NR;
    setBrightnessRaw(display_light_brightness[brightness_level]);
}

void Display::setBrightnessRaw(uint8_t target_brightness) {
    uint8_t current_brightness = lcd.getBrightness();
    
    // Płynne przejście między poziomami jasności
    if (target_brightness > current_brightness) {
        // Rozjaśnianie
        for (uint8_t b = current_brightness; b < target_brightness; b += 1) {
            lcd.setBrightness(b);
            vTaskDelay(40 / portTICK_PERIOD_MS);
        }
    } else if (target_brightness < current_brightness) {
        // Ściemnianie
        for (int16_t b = current_brightness; b > target_brightness; b -= 1) {
            lcd.setBrightness(b);
            vTaskDelay(40 / portTICK_PERIOD_MS);
        }
    }
    
    // Ustaw dokładną wartość docelową
    lcd.setBrightness(target_brightness);
}

void Display::setMaxBrightness(bool request_max_brightness) {
    max_brightness_requested = request_max_brightness;
    increased_brightness_requested = false;
    // This is to trigger an immediate change of brightness in monitorBrightnessTask
    bool queue_event = true;
    xQueueSend(queue, &queue_event, portMAX_DELAY);
}

void Display::setIncreasedBrightness(bool request_inc_brightness) {
    if (request_inc_brightness && time_unknown) {
        // Dotknięcie: ekran zgaszony -> poziom 1, świeci -> o poziom jaśniej (najwyżej 3), znowu 60 s
        int64_t now = esp_timer_get_time();
        if (now - wake_timestamp_us >= NO_TIME_WAKE_US)
            wake_level = 1;
        else if (wake_level < NO_TIME_MAX_LEVEL)
            wake_level = wake_level + 1;
        wake_timestamp_us = now;
        bool queue_event = true;
        xQueueSend(queue, &queue_event, 0);  // bez czekania: pełna kolejka znaczy, że zmiana i tak jest w drodze
    }
     // This is to trigger an immediate change of brightness in monitorBrightnessTask
    if (increased_brightness_requested != request_inc_brightness) {
        bool queue_event = true;
        xQueueSend(queue, &queue_event, portMAX_DELAY);
    }
    increased_brightness_requested = request_inc_brightness;
    max_brightness_requested = false;
}

bool Display::isDisplayOn(void) {
    return (display_brightness_level > 0 || increased_brightness_requested);
}

bool Display::isBrightEnoughForClick(void) {
    return (display_brightness_level >= 2 || increased_brightness_requested);
}

void Display::setTimeUnknown(bool unknown) {
    if (unknown) {
        wake_level = 1;
        wake_timestamp_us = esp_timer_get_time();
    }
    time_unknown = unknown;
    bool queue_event = true;
    xQueueSend(queue, &queue_event, portMAX_DELAY);  // od razu nowa jasność
}
