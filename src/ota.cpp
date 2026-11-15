#include "ota.hpp"

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ota";

static volatile bool pending_verify = false;   // nowa wersja czeka na potwierdzenie
static volatile bool restart_pending = false;
static volatile int64_t restart_requested_us = 0;  // zapisywane przed restart_pending

static int64_t uptimeS(void) {
    return esp_timer_get_time() / 1000000LL;
}

void Ota::init(void) {
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (running != NULL && esp_ota_get_state_partition(running, &state) == ESP_OK)
        pending_verify = (state == ESP_OTA_IMG_PENDING_VERIFY);
    ESP_LOGI(TAG, "Program z %s%s", running ? running->label : "?",
             pending_verify ? ": nowa wersja, do sprawdzenia" : "");
    if (wasRolledBack())
        ESP_LOGW(TAG, "Ostatnia aktualizacja nie zadziałała: działa poprzednia wersja");
}

void Ota::check(bool wifi_connected, bool web_running, bool alarm_idle) {
    if (pending_verify) {
        int64_t up = uptimeS();
        if (up >= OTA_CONFIRM_S && wifi_connected && web_running) {
            if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
                ESP_LOGI(TAG, "Nowa wersja działa: potwierdzona");
                pending_verify = false;
            }
        } else if (up >= OTA_GIVE_UP_S && alarm_idle) {
            ESP_LOGE(TAG, "Nowej wersji nie da się potwierdzić (Wi-Fi albo strona): wracam do poprzedniej");
            vTaskDelay(500 / portTICK_PERIOD_MS);
            esp_ota_mark_app_invalid_rollback_and_reboot();  // wraca tylko przy błędzie
            pending_verify = false;
        }
    }
    // Restart po wgraniu: chwila na wysłanie odpowiedzi do przeglądarki; nigdy w czasie alarmu i drzemki
    if (restart_pending && alarm_idle && esp_timer_get_time() - restart_requested_us > 2000000LL) {
        ESP_LOGI(TAG, "Restart do nowej wersji");
        vTaskDelay(200 / portTICK_PERIOD_MS);
        esp_restart();
    }
}

void Ota::requestRestart(void) {
    restart_requested_us = esp_timer_get_time();
    restart_pending = true;
}

bool Ota::isRestartPending(void) {
    return restart_pending;
}

const char *Ota::runningLabel(void) {
    const esp_partition_t *running = esp_ota_get_running_partition();
    return running ? running->label : "?";
}

bool Ota::isPendingVerify(void) {
    return pending_verify;
}

bool Ota::wasRolledBack(void) {
    return esp_ota_get_last_invalid_partition() != NULL;
}
