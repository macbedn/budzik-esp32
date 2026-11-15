#ifndef _INCLUDE_CLOCK_COMMON_HPP_
#define _INCLUDE_CLOCK_COMMON_HPP_

#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"

// Rotary Encoder (enkoder obrotowy z hardware pull-up)
#define ROT_ENC_A_GPIO      GPIO_NUM_35      // S1 (CLK)
#define ROT_ENC_B_GPIO      GPIO_NUM_33      // S2 (DT)
#define ROT_ENC_BUTTON_GPIO GPIO_NUM_32      // Button (SW)
#define ROT_ENC_BUTTON_INVERTED      1

// Display SPI (wyświetlacz)
// MOSI = GPIO23, SCK = GPIO18 (hardware SPI - domyslne)
#define DISPLAY_MOSI_GPIO   GPIO_NUM_23      // hardware SPI MOSI
#define DISPLAY_MISO_GPIO   GPIO_NUM_19      // dla SD
#define DISPLAY_SCLK_GPIO   GPIO_NUM_18      // hardware SPI SCK
#define DISPLAY_DC_GPIO     GPIO_NUM_4       // DC
#define DISPLAY_CS_GPIO     GPIO_NUM_5       // CS (Chip Select)
#define DISPLAY_RST_GPIO    GPIO_NUM_NC      // RST nie podlaczony (-1)
#define DISPLAY_BL_GPIO     GPIO_NUM_14      // Backlight

// SD Card (współdzielone z ekranem)
// #define SD_CS_GPIO          GPIO_NUM_13   // CS karty SD

// I2S Audio (MAX98357)
// #define I2S_BCK_GPIO        GPIO_NUM_27   // Bit Clock
// #define I2S_LRCK_GPIO       GPIO_NUM_26   // Left/Right Clock (WS)
// #define I2S_DOUT_GPIO       GPIO_NUM_25   // Data Out

// Buzzer piezo (tymczasowo do testów zamiast I2S)
// #define BUZZER_GPIO         GPIO_NUM_21

// MP3 Player UART (modul audio)
#define MP3_PLAYER_UART_PORT_NUM UART_NUM_2
#define MP3_PLAYER_TX       GPIO_NUM_17
#define MP3_PLAYER_RX       GPIO_NUM_16

// Light sensor ADC (czujnik swiatla) - WYLACZONE
// #define LIGHT_ADC_CHANNEL   ADC_CHANNEL_6
// #define LIGHT_ADC_ATTEN     ADC_ATTEN_DB_0

typedef struct {
    uint8_t hour;
    uint8_t minute;
} clock_time_t;

typedef struct {
    float temperature;       // z pomiaru (Open-Meteo: temperature_2m)
    float feels_like;        // odczuwalna (apparent_temperature)
    int16_t weather_id;      // Kod pogody WMO z Open-Meteo (0..99)
    uint8_t humidity;
    uint16_t pressure;
    bool is_valid;           // Czy dane są (pobrane przynajmniej raz); aktualność: Weather::isStale()
} weather_data_t;

// Nazwa zegara w sieci: mDNS (http://budzik.local) i nazwa na liście urządzeń routera (DHCP)
#define WEB_HOSTNAME "budzik"

#endif // _INCLUDE_CLOCK_COMMON_HPP_