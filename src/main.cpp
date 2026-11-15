#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <wifi_time.hpp>
#include <rotary_encoder.hpp>
#include "clock_machine.hpp"
#include "clock_machine_states.hpp"
#include "clock_common.hpp"
#include "mem_log.hpp"
#include "settings.hpp"
#include "esp_log.h"

// Pokrętło wciśnięte przy włączaniu zasilania: tryb ustawień (sieć zegara z kodem QR).
// Sprawdzane przed startem obsługi pokrętła; czekamy na puszczenie, żeby nie wyszło z tego długie wciśnięcie.
static bool buttonHeldAtStart(void) {
    gpio_reset_pin(ROT_ENC_BUTTON_GPIO);
    gpio_set_direction(ROT_ENC_BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(ROT_ENC_BUTTON_GPIO, GPIO_PULLUP_ONLY);
    vTaskDelay(20 / portTICK_PERIOD_MS);
    const int pressed_level = ROT_ENC_BUTTON_INVERTED ? 0 : 1;
    // 300 ms bez przerwy wciśnięte (drgania styków to nie wciśnięcie)
    for (int i = 0; i < 30; i++) {
        if (gpio_get_level(ROT_ENC_BUTTON_GPIO) != pressed_level)
            return false;
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    ESP_LOGI("main", "Pokrętło wciśnięte przy starcie: tryb ustawień");
    // Puszczenie: najwyżej 30 s, potem i tak dalej
    for (int i = 0; i < 3000 && gpio_get_level(ROT_ENC_BUTTON_GPIO) == pressed_level; i++)
        vTaskDelay(10 / portTICK_PERIOD_MS);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    return true;
}

extern "C" void app_main() {
    // Initialize NVS (needs to be done first thing in main!)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    Settings::init();  // ustawienia ze strony (Wi-Fi, pogoda, jasność) przed Wi-Fi i pogodą
    bool setup_requested = buttonHeldAtStart();

    // Initialise the rotary encoder device with the GPIOs for A and B signals
    RotaryEncoder encoder;
    QueueHandle_t event_queue = encoder.init(ROT_ENC_A_GPIO, ROT_ENC_B_GPIO, ROT_ENC_BUTTON_GPIO, ROT_ENC_BUTTON_INVERTED);

    ClockMachine machine(&encoder);  // By default a clock machine starts in state "TIME"
    rotary_encoder_event_t event;
    if (setup_requested)
        machine.enterSetup();

    memLogStart();  // pomiar pamięci co minutę do logu (mem_log.hpp, MEM_LOG 0 = wyłączone)

    while (1) {
        if (xQueueReceive(event_queue, &event, 10 / portTICK_PERIOD_MS) == pdTRUE) {
            switch (event.type) {
                case ENCODER_ROTATION:
                    machine.encoderRotated(event.position, event.direction);
                    break;
                case BUTTON_SHORT_PRESS:
                    machine.buttonShortPressed();
                    break;
                case BUTTON_LONG_PRESS:
                    machine.buttonLongPressed();
                    break;
                case BUTTON_DOUBLE_PRESS:
                    machine.buttonDoublePressed();
                    break;
            }
        }

        // Perform whatever cyclic activities or checks are needed for this state
        machine.run();
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
