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
#include "can.h"

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

typedef enum {
    eCAN_BYTE0 = 0,
    eCAN_BYTE1,
    eCAN_BYTE2,
    eCAN_BYTE3,
    eCAN_BYTE4,
    eCAN_BYTE5,
    eCAN_BYTE6,
    eCAN_BYTE7,
    eCAN_MAX_LENGTH
} eCANDataLength_t;

/* --------------------------- Local Variables ----------------------------- */
extern twai_handle_t stCANBus0;

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
    static word wNCounter;
    static qword qwNTxData = 0x123456789;
    esp_err_t stState;
    twai_status_info_t stBusStatus;

    qwtTaskTimer = esp_timer_get_time();
    astTaskState[eTASK_100MS] = eTASK_ACTIVE;

    if (wNCounter >= 100)
    {
        // Print the max task time every 10 seconds
        ESP_LOGI(TAG, "Max Task Time: %5d BG %5d 1ms %5d 100ms", 
            (int)adwMaxTaskTime[eTASK_BG], 
            (int)adwMaxTaskTime[eTASK_1MS], 
            (int)adwMaxTaskTime[eTASK_100MS]);
        wNCounter = 0;
    }
    wNCounter++;

    // Check if the CAN bus is in error state and recover
    twai_get_status_info_v2(stCANBus0, &stBusStatus);
    switch (stBusStatus.state){
    case TWAI_STATE_STOPPED:
    {
        ESP_LOGW("CAN", "Bus is stopped. Starting...");
        stState = twai_start_v2(stCANBus0);
        break;
    }
    case TWAI_STATE_BUS_OFF:
    {
        ESP_LOGW("CAN", "Bus is in BUS_OFF. Attempting to recover...");
        stState = twai_initiate_recovery_v2(stCANBus0);
        break;
    } 
    case TWAI_STATE_RECOVERING:
    {
        stState = ESP_OK;
        ESP_LOGW("CAN", "Bus is recovering...");
        break;
    }
    case TWAI_STATE_RUNNING:
    {
        //stState = twai_transmit_v2(stCANBus0, &data_message, FALSE);
        stState = CAN_transmit(&stCANBus0, 0x045, eCAN_MAX_LENGTH, &qwNTxData);
        break;
    }
    default:
    {
        ESP_LOGW("CAN", "Bus is so fucked even it does not know what is wrong.");
        stState = ESP_OK;
        break;
    }
    }
    if ( stState != ESP_OK ) {
        ESP_LOGE("CAN", "Action failed: %s", esp_err_to_name(stState));
        twai_get_status_info_v2(stCANBus0, &stBusStatus);
        ESP_LOGI("CAN", "TWAI state: %d", stBusStatus.state);
    }
    

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

