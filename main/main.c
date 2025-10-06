/*
main.c | CAN Blaster | ESP32-C6 | Sheffield Formula Racing
File contains the main function for the ESP32-C6 based CAN Blaster.
This program sends set can messages out to the bus to test loading and other devicess recieve functionality.

Written by Cole Perera for Sheffield Formula Racing 2025
*/

/* --------------------------- Includes ----------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "esp_err.h"

#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "driver/gpio.h"

#include "can.h"
#include "tasks.h"
#include "pin.h"
#include "espnow.h"

/* --------------------------- Definitions ----------------------------- */
#define TIMER_INTERVAL_WD       100     // in microseconds
#define TIMER_INTERVAL_GP       100
#define TIMER_INTERVAL_1MS      pdMS_TO_TICKS(1)       // in milliseconds
#define TIMER_INTERVAL_100MS    pdMS_TO_TICKS(100)

/* --------------------------- Global Variables ----------------------------- */
esp_timer_handle_t stTaskInterupt1ms;
esp_timer_handle_t stTaskInterupt100ms;

/* --------------------------- Function prototypes ----------------------------- */
static void timers_init(void);
static void main_init(void);
static void GPIO_init(void);
void IRAM_ATTR callback1ms(void *arg);
void IRAM_ATTR callback100ms(void *arg);

/* --------------------------- Functions ----------------------------- */

void app_main(void)
{
    // Register app_main task with the WDT
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL)); 

    esp_task_wdt_delete(xTaskGetIdleTaskHandleForCPU(0));  // always exists

    #if CONFIG_FREERTOS_UNICORE == 0  // Dual-core only
        esp_task_wdt_delete(xTaskGetIdleTaskHandleForCPU(1));
    #endif

    /* Initialise device */
    main_init();

    /* Run background task all other tasks are called by interupts */
    while(1)
    {
        task_BG();
    }

}

void IRAM_ATTR callback1ms(void *arg)
{
    task_1ms();
}

void IRAM_ATTR callback100ms(void *arg)
{
    task_100ms();
}

static void main_init(void)
{
    esp_err_t stStatus;
    stStatus = CAN_init();
    if (stStatus != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialise CAN: %s", esp_err_to_name(stStatus));
    }

    /* Timers and GPIO cause a hard fault on fail so no error warning */
    GPIO_init();
    timers_init();  

    stStatus = espNow_init();
    if (stStatus != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialise CAN: %s", esp_err_to_name(stStatus));
    }
}

static void timers_init(void)
{
    const esp_timer_create_args_t stTaskInterupt1msArgs = {
        .callback = &callback1ms,
        .arg = NULL,
        .name = "1ms"
    };
    const esp_timer_create_args_t stTaskInterupt100msArgs = {
        .callback = &callback100ms,
        .arg = NULL,
        .name = "100ms"
    };

    ESP_ERROR_CHECK(esp_timer_create(&stTaskInterupt1msArgs, &stTaskInterupt1ms));
    ESP_ERROR_CHECK(esp_timer_start_periodic(stTaskInterupt1ms, 1000)); // 1ms
    ESP_ERROR_CHECK(esp_timer_create(&stTaskInterupt100msArgs, &stTaskInterupt100ms));
    ESP_ERROR_CHECK(esp_timer_start_periodic(stTaskInterupt100ms, 100000)); // 100ms
}

static void GPIO_init(void)
{
    gpio_config_t onboardLEDConfig = {
        .pin_bit_mask = 1ULL << GPIO_ONBOARD_LED,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&onboardLEDConfig);
}

