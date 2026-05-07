#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BLINK_GPIO GPIO_NUM_2
#define BLINK_DELAY_MS 500

static const char *TAG = "blinky";

void app_main(void)
{
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    bool led_on = false;

    ESP_LOGI(TAG, "Blink task started on GPIO %d", BLINK_GPIO);

    while (1) {
        led_on = !led_on;
        gpio_set_level(BLINK_GPIO, led_on);
        ESP_LOGI(TAG, "LED %s", led_on ? "ON" : "OFF");
        vTaskDelay(pdMS_TO_TICKS(BLINK_DELAY_MS));
    }

}
