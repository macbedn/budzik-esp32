#ifndef _INCLUDE_WIFI_TIME_HPP_
#define _INCLUDE_WIFI_TIME_HPP_

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "clock_common.hpp"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define WIFI_RECHECK_PERIOD_WITH_TIME_SYNC 900000   // 15 Minutes
#define WIFI_RECHECK_PERIOD_NO_TIME_SYNC   15000    // 15 seconds, we need a sync for the time!
#define WIFI_RECHECK_PERIOD_NO_CONNECTION  60000    // godzina jest, połączenia nie: próba co minutę
#define WIFI_NR_RETRIES 3

// Własna sieć zegara (tryb ustawień): nazwa stała, hasło losowe przy każdym włączeniu
#define SETUP_AP_SSID  "Budzik-ustawienia"
#define SETUP_AP_IP    "192.168.4.1"
#define SETUP_AP_PASS_LEN 9   // 8 cyfr + zero

class WifiTime {
    static void wifiEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
    static void monitorWifiTask(void *pvParameter);
    void monitorWifi(void);
    void initSTA(void);
    void initSNTP(void);
    esp_err_t applyStaConfig(const char *ssid, const char *pass);

    EventGroupHandle_t wifi_event_group;
    esp_netif_t *sta_netif = NULL;
    esp_netif_t *ap_netif = NULL;
    volatile bool wifi_is_connected = false;
    volatile bool ever_connected = false;        // od startu
    volatile int32_t disconnected_since_s = 0;   // kiedy zniknęła sieć, sekundy od startu (32 bity: odczyt
                                                 // z innego zadania jest niepodzielny, 64 bity nie)
    uint8_t retry_num = 0;
    volatile bool setup_ap_active = false;

   public:
    void init(void);
    bool isTimeSet(void);
    bool isWifiConnected(void);
    bool wasEverConnected(void) { return ever_connected; }
    int64_t getDisconnectedSinceUs(void) { return (int64_t)disconnected_since_s * 1000000LL; }
    void getIP(char *buf, size_t len);  // adres w sieci domowej albo "" bez połączenia
    void setTime(struct tm *timeinfo);
    void getTime(clock_time_t *time);

    // Tryb ustawień: własna sieć zegara obok połączenia z siecią domową
    bool startSetupAP(char *password_out);   // bufor SETUP_AP_PASS_LEN
    void stopSetupAP(void);
    bool isSetupAPActive(void) { return setup_ap_active; }
    int scanNetworks(wifi_ap_record_t *records, uint16_t max_records);  // -1: teraz się nie da
};

#endif // _INCLUDE_WIFI_TIME_HPP_
