#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include <string.h>
#include <wifi_time.hpp>

#include "settings.hpp"  // sieć Wi-Fi z NVS (domyślnie z secrets.h)

static const char *TAG = "wifi_time";

// Skanowanie sieci (strona w trybie ustawień): wtedy zgubione połączenie nie jest od razu ponawiane,
// bo próby łączenia blokują skanowanie
static volatile bool scanning = false;

void WifiTime::wifiEventHandler(void *pvParameter, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    WifiTime *pThis = (WifiTime *)pvParameter;
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                if (Settings::hasWifi())
                    esp_wifi_connect();
                else
                    ESP_LOGW(TAG, "Brak zapisanej sieci Wi-Fi: ustawienie przez sieć zegara");
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                if (pThis->wifi_is_connected)
                    pThis->disconnected_since_s = (int32_t)(esp_timer_get_time() / 1000000LL);
                pThis->wifi_is_connected = false;
                if (scanning)
                    break;
                // W trybie ustawień bez ponawiania: każda próba łączenia przeszukuje wszystkie kanały, a sieć
                // zegara skacze z nią i telefon traci stronę. Łączy dopiero stopSetupAP() po wyjściu z trybu.
                if (pThis->setup_ap_active) {
                    pThis->retry_num = 0;
                    break;
                }
                if (pThis->retry_num < WIFI_NR_RETRIES) {
                    esp_wifi_connect();
                    pThis->retry_num++;
                    ESP_LOGI(TAG, "retry number %d for connection to WiFi", pThis->retry_num);
                } else {
                    pThis->retry_num = 0;
                    xEventGroupSetBits(pThis->wifi_event_group, WIFI_FAIL_BIT);
                }
                break;
            case WIFI_EVENT_AP_STACONNECTED:
                ESP_LOGI(TAG, "Ktoś połączył się z siecią zegara");
                break;
            default:
                break;
        }

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        pThis->retry_num = 0;
        pThis->wifi_is_connected = true;  // od razu, a nie dopiero w monitorWifi() (ten może spać 15 min)
        pThis->ever_connected = true;
        // Od razu zapytaj o godzinę: SNTP ponawia próby co 15, 30, 60... do 150 s, więc bez tego godzina
        // przychodziła nawet kilka minut po połączeniu (pierwsza próba zwykle przed połączeniem z Wi-Fi).
        // Restart robi pierwsze zapytanie po losowych 0-5 s (CONFIG_LWIP_SNTP_STARTUP_DELAY).
        if (esp_sntp_restart())
            ESP_LOGI(TAG, "Got IP, SNTP restarted");
        xEventGroupSetBits(pThis->wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void WifiTime::monitorWifiTask(void *pvParameter) {
    WifiTime *pThis = (WifiTime *)pvParameter;
    ESP_ERROR_CHECK(esp_wifi_start());

    while (1) {
        pThis->monitorWifi();
    }
}

void WifiTime::monitorWifi(void) {
    // Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
    // number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above)
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    char ssid[SETTINGS_SSID_LEN], pass[SETTINGS_PASS_LEN];
    Settings::getWifi(ssid, sizeof(ssid), pass, sizeof(pass));
    // xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually happened.
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to WiFi: %s", ssid);
        wifi_is_connected = true;
        // Tu, a nie w obsłudze zdarzeń: zapis do NVS potrzebuje więcej stosu
        Settings::setWifiOk();
    }
    else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to WiFi: %s", ssid);
    } else {
        ESP_LOGE(TAG, "Unexpected event");
    }
    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    // Bez godziny: następna próba po 15 s. Z godziną i połączeniem: sprawdzenie za 15 min, ale zerwane
    // połączenie (3 nieudane próby w obsłudze zdarzeń) budzi od razu. Z godziną bez połączenia: próba co minutę
    // (do 2026-11-08 też co 15 min: po restarcie routera albo po aktualizacji przy słabym sygnale zegar
    // wracał do sieci dopiero po kwadransie; godzina zostaje po restarcie programowym).
    if (!isTimeSet())
        vTaskDelay(WIFI_RECHECK_PERIOD_NO_TIME_SYNC / portTICK_PERIOD_MS);
    else if (bits & WIFI_CONNECTED_BIT) {
        if (xEventGroupWaitBits(wifi_event_group, WIFI_FAIL_BIT, pdFALSE, pdFALSE,
                                WIFI_RECHECK_PERIOD_WITH_TIME_SYNC / portTICK_PERIOD_MS) & WIFI_FAIL_BIT)
            return;  // zerwane: następny obieg zapisze to w logu i spróbuje za minutę
    } else
        vTaskDelay(WIFI_RECHECK_PERIOD_NO_CONNECTION / portTICK_PERIOD_MS);

    // Bez zapisanej sieci nie ma z czym się łączyć
    if (!scanning && !setup_ap_active && Settings::hasWifi())
        esp_wifi_connect();
}

esp_err_t WifiTime::applyStaConfig(const char *ssid, const char *pass) {
    wifi_config_t wifi_config = {};
    // Z limitem długości pola (SSID do 32 bajtów, hasło do 64)
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
    // Sieć bez hasła też dozwolona (zwykle jednak WPA2)
    wifi_config.sta.threshold.authmode = pass[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    // Bez ESP_ERROR_CHECK: w trakcie łączenia sterownik odmawia (ESP_ERR_WIFI_STATE), a to nie powód do restartu
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "Ustawienie sieci: %s", esp_err_to_name(err));
    return err;
}

void WifiTime::initSTA(void) {
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    sta_netif = esp_netif_create_default_wifi_sta();
    // Nazwa w DHCP: router pokazuje zegar jako "budzik" zamiast "espressif" (przed startem Wi-Fi)
    if (esp_netif_set_hostname(sta_netif, WEB_HOSTNAME) != ESP_OK)
        ESP_LOGW(TAG, "Nie ustawiono nazwy zegara w DHCP");
    ap_netif = esp_netif_create_default_wifi_ap();  // używana tylko w trybie ustawień

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, this->wifiEventHandler, this, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, this->wifiEventHandler, this, &instance_got_ip));

    // Sieć trzymamy sami w ustawieniach (settings.hpp), sterownik nie zapisuje jej osobno
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    char ssid[SETTINGS_SSID_LEN], pass[SETTINGS_PASS_LEN];
    Settings::getWifi(ssid, sizeof(ssid), pass, sizeof(pass));
    ESP_ERROR_CHECK(applyStaConfig(ssid, pass));

    xTaskCreate(this->monitorWifiTask, "monitor_wifi_task", 4096, this, 10, NULL);
}

void WifiTime::initSNTP(void) {
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
    esp_sntp_init();
    // Timezone Berlin: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
}

void WifiTime::init(void) {
    // Sieć Wi-Fi z ustawień w NVS (domyślnie z secrets.h), zmieniana przez sieć zegara (tryb ustawień)
    sntp_servermode_dhcp(0);
    initSTA();
    initSNTP();
}

void WifiTime::getIP(char *buf, size_t len) {
    buf[0] = '\0';
    esp_netif_ip_info_t ip_info;
    if (wifi_is_connected && esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK)
        snprintf(buf, len, IPSTR, IP2STR(&ip_info.ip));
}

bool WifiTime::startSetupAP(char *password_out) {
    // Hasło: 8 losowych cyfr (WPA2 wymaga co najmniej 8 znaków), nowe przy każdym włączeniu
    for (int i = 0; i < SETUP_AP_PASS_LEN - 1; i++)
        password_out[i] = '0' + (esp_random() % 10);
    password_out[SETUP_AP_PASS_LEN - 1] = '\0';

    wifi_config_t ap_config = {};
    strncpy((char *)ap_config.ap.ssid, SETUP_AP_SSID, sizeof(ap_config.ap.ssid));
    ap_config.ap.ssid_len = strlen(SETUP_AP_SSID);
    strncpy((char *)ap_config.ap.password, password_out, sizeof(ap_config.ap.password));
    ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ap_config.ap.max_connection = 2;
    ap_config.ap.channel = 1;  // przy połączeniu z siecią domową i tak przechodzi na jej kanał

    if (esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK) return false;
    if (esp_wifi_set_config(WIFI_IF_AP, &ap_config) != ESP_OK) return false;

    // Bez captive portalu: zegar nie udaje serwera DNS, strona tylko pod http://192.168.4.1 (wpisane ręcznie)

    setup_ap_active = true;
    ESP_LOGI(TAG, "Sieć zegara %s włączona", SETUP_AP_SSID);
    return true;
}

void WifiTime::stopSetupAP(void) {
    if (!setup_ap_active) return;
    esp_wifi_set_mode(WIFI_MODE_STA);
    setup_ap_active = false;
    ESP_LOGI(TAG, "Sieć zegara wyłączona");
    // W trybie ustawień zegar nie ponawiał łączenia: wracamy do zwykłych prób
    if (!wifi_is_connected && Settings::hasWifi()) {
        retry_num = 0;
        esp_wifi_connect();
    }
}

int WifiTime::scanNetworks(wifi_ap_record_t *records, uint16_t max_records) {
    scanning = true;
    // Trwające próby łączenia blokują skanowanie (ESP_ERR_WIFI_STATE): przerywamy je na czas skanowania
    if (!wifi_is_connected) {
        esp_wifi_disconnect();
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
    esp_err_t err = esp_wifi_scan_start(NULL, true);
    int count = -1;
    if (err == ESP_OK) {
        uint16_t n = max_records;
        if (esp_wifi_scan_get_ap_records(&n, records) == ESP_OK)
            count = n;
    } else {
        ESP_LOGE(TAG, "Skanowanie: %s", esp_err_to_name(err));
    }
    scanning = false;
    // W trybie ustawień (jedyne miejsce skanowania) bez ponownych prób: patrz obsługa rozłączenia
    if (!wifi_is_connected && !setup_ap_active && Settings::hasWifi()) {
        retry_num = 0;
        esp_wifi_connect();
    }
    return count;
}

bool WifiTime::isTimeSet(void) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).
    return (timeinfo.tm_year > (1970 - 1900));
}

bool WifiTime::isWifiConnected(void) {
    return wifi_is_connected;
}

void WifiTime::setTime(struct tm *timeinfo) {
    time_t t = mktime(timeinfo);
    struct timeval now_set = {.tv_sec = t, .tv_usec = 0};
    settimeofday(&now_set, NULL);
}

void WifiTime::getTime(clock_time_t *t) {
    // Get current RTC time from the ESP32
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    t->hour = (uint8_t)timeinfo.tm_hour;
    t->minute = (uint8_t)timeinfo.tm_min;
}
