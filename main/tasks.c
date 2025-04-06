/*
tasks.c
File contains the tasks that run on the device. Tasks are called by interrupts at their specified rates.
Tasks are:
    task_BG: Background task that runs as often as processor time is available.
    task_1ms: Task that runs every 1ms.
    task_10ms: Task that runs every 10ms.
    task_100ms: Task that runs every 100ms.

Written by Cole Perera for Sheffield Formula Racing
*/

#include <stdint.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_log.h"
#include "esp_err.h"
#include "main.h"
#include "pin.h"
#include "tasks.h"

/* --------------------------- Global Variables ----------------------------- */

/* --------------------------- Local Variables ----------------------------- */

/* --------------------------- Function prototypes ----------------------------- */

/* --------------------------- Definitions ----------------------------- */

/* --------------------------- Function prototypes ----------------------------- */
void pin_toggle(gpio_num_t pin);
void task_BG(void);
void task_1ms(void);
void task_100ms(void);

/* --------------------------- Functions ----------------------------- */
void task_BG(void)
{
    /* Background task that runs as often as processor time is available. */

    //service the watchdog
    esp_task_wdt_reset();
}

void task_1ms(void)
{
    /* Task that runs every 1ms. */
    static uint16_t wNTaskCounter = 0;

    if (wNTaskCounter % 1000 == 0) {
        pin_toggle(GPIO_ONBOARD_LED); // Toggle the LED every second
        ESP_LOGI(TAG, "Here");
    }
    wNTaskCounter++;
}

void task_100ms(void)
{
    /* Task that runs every 100ms. */
    ESP_LOGI(TAG, "Driver installed");
}


void pin_toggle(gpio_num_t pin)
{
    static bool led_state = false;
    led_state = !led_state;
    gpio_set_level(pin, led_state);
}