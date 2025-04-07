/*
tasks.c
File contains the tasks that run on the device. Tasks are called by interrupts at their specified rates.
Tasks are:
    task_BG: Background task that runs as often as processor time is available.
    task_1ms: Task that runs every 1ms.
    task_10ms: Task that runs every 10ms.
    task_100ms: Task that runs every 100ms.

Written by Cole Perera for Sheffield Formula Racing 2025
*/

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_err.h"
#include "main.h"
#include "pin.h"
#include "tasks.h"

/* --------------------------- Local Types ----------------------------- */
typedef enum {
    eTASK_BG = 0,
    eTASK_1MS,
    eTASK_100MS,
    eTASK_TOTAL,
} eTasks_t;

typedef enum {
    eTASK_ACTIVE = 0,
    eTASK_INACTIVE,
} eTaskState_t;

/* --------------------------- Global Variables ----------------------------- */
dword adwMaxTaskTime[eTASK_TOTAL];
eTaskState_t astTaskState[eTASK_TOTAL];

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
    static qword qwtTaskTimer;
    qwtTaskTimer = esp_timer_get_time();
    astTaskState[eTASK_BG] = eTASK_ACTIVE;

    //service the watchdog if all task have been completed at least once
    word wNTaskCounter = 0;
    boolean bTasksComplete = TRUE;
    for (wNTaskCounter = 0; wNTaskCounter < eTASK_TOTAL; wNTaskCounter++)
    {
        if (astTaskState[wNTaskCounter] == eTASK_INACTIVE) {
            bTasksComplete = FALSE;
            break;
        }
    }
    if (bTasksComplete) {
        // Reset the watchdog timer
        esp_task_wdt_reset();
        for (wNTaskCounter = 0; wNTaskCounter < eTASK_TOTAL; wNTaskCounter++)
        {
            astTaskState[wNTaskCounter] = eTASK_INACTIVE;
        }
    }

    //update max task time
    qwtTaskTimer = esp_timer_get_time() - qwtTaskTimer;
    if (qwtTaskTimer > adwMaxTaskTime[eTASK_BG]) {
        adwMaxTaskTime[eTASK_BG] = (dword)qwtTaskTimer;
    }
}

void task_1ms(void)
{
    /* Task that runs every 1ms. */
    static qword qwtTaskTimer;
    qwtTaskTimer = esp_timer_get_time();
    astTaskState[eTASK_1MS] = eTASK_ACTIVE;

    static word wNTaskCounter = 0;

    if ( wNTaskCounter % 1000 == 0 ) 
    {
        pin_toggle(GPIO_ONBOARD_LED); // Toggle the LED every second
    }
    wNTaskCounter++;

    //update max task time
    qwtTaskTimer = esp_timer_get_time() - qwtTaskTimer;
    if (qwtTaskTimer > adwMaxTaskTime[eTASK_1MS]) 
    {
        adwMaxTaskTime[eTASK_1MS] = (dword)qwtTaskTimer;
    }
}

void task_100ms(void)
{
    /* Task that runs every 100ms. */
    static qword qwtTaskTimer;
    qwtTaskTimer = esp_timer_get_time();
    astTaskState[eTASK_100MS] = eTASK_ACTIVE;

    ESP_LOGI(TAG, "Max Task Time: %5d BG %5d 1ms %5d 100ms", 
        (int)adwMaxTaskTime[eTASK_BG], 
        (int)adwMaxTaskTime[eTASK_1MS], 
        (int)adwMaxTaskTime[eTASK_100MS]);

    //update max task time
    qwtTaskTimer = esp_timer_get_time() - qwtTaskTimer;
    if (qwtTaskTimer > adwMaxTaskTime[eTASK_100MS]) 
    {
        adwMaxTaskTime[eTASK_100MS] = (dword)qwtTaskTimer;
    }
}


void pin_toggle(gpio_num_t pin)
{
    static boolean BLEDState = false;
    BLEDState = !BLEDState;
    gpio_set_level(pin, BLEDState);
}

