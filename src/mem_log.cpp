#include "mem_log.hpp"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "nvs.h"

#if MEM_LOG

static const char *TAG = "mem";

// Zadania sprawdzane po nazwie. Przy nazwie rozmiar stosu (w bajtach), żeby w logu było widać, ile z niego zostało.
static const struct {
    const char *name;
    uint32_t stack_size;
} tasks[] = {
    {"main",                    CONFIG_ESP_MAIN_TASK_STACK_SIZE},
    {"monitor_wifi_task",       4096},
    {"weather_task",            8192},
    {"monitor_serial_task",     3072},
    {"button_task",             2048},
    {"monitor_brightness_task", 3072},
    {"mem_log_task",            3072},
    {"tiT",                     CONFIG_LWIP_TCPIP_TASK_STACK_SIZE},
    {"sys_evt",                 CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE},
    {"esp_timer",               CONFIG_ESP_TIMER_TASK_STACK_SIZE},
};

static void memLogTask(void *pvParameter) {
    while (1) {
        vTaskDelay(MEM_LOG_PERIOD_MS / portTICK_PERIOD_MS);

        ESP_LOGI(TAG, "sterta: wolne %u B, najmniej od startu %u B, najwiekszy blok %u B",
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

        // Zapas stosu: najmniej wolnego miejsca od startu zadania (w ESP-IDF w bajtach)
        char line[400];
        int pos = snprintf(line, sizeof(line), "stosy (zapas/rozmiar B):");
        for (size_t i = 0; i < sizeof(tasks) / sizeof(tasks[0]) && pos < (int)sizeof(line); i++) {
            // FreeRTOS pamięta tylko 15 znaków nazwy, a dłuższa nazwa w xTaskGetHandle zatrzymuje program (assert)
            char short_name[configMAX_TASK_NAME_LEN];
            strncpy(short_name, tasks[i].name, sizeof(short_name) - 1);
            short_name[sizeof(short_name) - 1] = '\0';
            TaskHandle_t handle = xTaskGetHandle(short_name);
            if (handle == NULL) {
                pos += snprintf(line + pos, sizeof(line) - pos, " %s=brak", tasks[i].name);
            } else {
                pos += snprintf(line + pos, sizeof(line) - pos, " %s=%u/%u", tasks[i].name,
                                (unsigned)uxTaskGetStackHighWaterMark(handle), (unsigned)tasks[i].stack_size);
            }
        }
        ESP_LOGI(TAG, "%s", line);

        nvs_stats_t nvs_stats;
        if (nvs_get_stats(NULL, &nvs_stats) == ESP_OK) {
            ESP_LOGI(TAG, "NVS: zajete %u, wolne %u, razem %u wpisow",
                     (unsigned)nvs_stats.used_entries, (unsigned)nvs_stats.free_entries,
                     (unsigned)nvs_stats.total_entries);
        }
    }
}

void memLogStart(void) {
    xTaskCreate(memLogTask, "mem_log_task", 3072, NULL, 1, NULL);
}

#else

void memLogStart(void) {}

#endif
