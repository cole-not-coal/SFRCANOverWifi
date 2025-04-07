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

#include "freertos/timers.h"
#include "esp_task_wdt.h"
#include "driver/gpio.h"

#include "main.h"
#include "can.h"
#include "tasks.h"
#include "pin.h"

/* --------------------------- Definitions ----------------------------- */
#define TIMER_INTERVAL_WD       100     // in microseconds
#define TIMER_INTERVAL_GP       100
#define TIMER_INTERVAL_1MS      pdMS_TO_TICKS(1)       // in milliseconds
#define TIMER_INTERVAL_100MS    pdMS_TO_TICKS(100)

/* --------------------------- Global Variables ----------------------------- */
static TimerHandle_t timer1ms;
static TimerHandle_t timer100ms;

/* --------------------------- Function prototypes ----------------------------- */
static void timers_init(void);
static void main_init(void);
static void GPIO_init(void);
void timer_callback_1ms(TimerHandle_t xTimer);
void timer_callback_100ms(TimerHandle_t xTimer); 

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

}// 1ms task
void timer_callback_1ms(TimerHandle_t xTimer) 
{
    
    //TODO add task timer
    task_1ms();

}

// 100ms task
void timer_callback_100ms(TimerHandle_t xTimer)  
{
    
    //TODO add task timer
    task_100ms();

}

static void main_init(void)
{
    timers_init();
    CAN_init();
    GPIO_init();

    // Start the timers
    xTimerStart(timer1ms, 0);
    xTimerStart(timer100ms, 0);
}

static void timers_init(void)
{
    // FreeRTOS timers that do not support sub ms timing, but are more suitable for the main tasks
    timer1ms = xTimerCreate("1ms", TIMER_INTERVAL_1MS, pdTRUE, NULL, timer_callback_1ms);
    timer100ms = xTimerCreate("100ms", TIMER_INTERVAL_100MS, pdTRUE, NULL, timer_callback_100ms);

    configASSERT(timer1ms && timer100ms);
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

