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

/* --------------------------- Local Variables ----------------------------- */
extern twai_node_handle_t stCANBus0;
extern uint8_t byMACAddress[6];

/* --------------------------- Global Variables ----------------------------- */
dword adwMaxTaskTime[eTASK_TOTAL];
dword adwLastTaskTime[eTASK_TOTAL];
eTaskState_t astTaskState[eTASK_TOTAL];
dword dwTimeSincePowerUpms = 0;

/* --------------------------- Function prototypes ----------------------------- */

/* --------------------------- Definitions ----------------------------- */
#define TIME_BETWEEN_CAN_MSGS 100  // ms

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

    /* Flush CAN buffer to SD card */
    sdcard_flush_can_buffer();

    /* Service the watchdog if all task have been completed at least once */
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
        /* Reset the watchdog timer */
        esp_err_t err = esp_task_wdt_reset();
        if (err == ESP_ERR_NOT_FOUND) {
            esp_task_wdt_add(NULL);       
            esp_task_wdt_reset();           
        }
        for (wNTaskCounter = 0; wNTaskCounter < eTASK_TOTAL; wNTaskCounter++)
        {
            astTaskState[wNTaskCounter] = eTASK_INACTIVE;
        }
    }

    /* Update max task time */
    qwtTaskTimer = esp_timer_get_time() - qwtTaskTimer;
    adwLastTaskTime[eTASK_BG] = (dword)qwtTaskTimer;
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

    /* CAN Rx */
    CAN_receive(stCANBus0);

    /* Update time since power up */
    dwTimeSincePowerUpms++;

    /* Update max task time */
    qwtTaskTimer = esp_timer_get_time() - qwtTaskTimer;
    adwLastTaskTime[eTASK_1MS] = (dword)qwtTaskTimer;
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

    qwtTaskTimer = esp_timer_get_time();
    astTaskState[eTASK_100MS] = eTASK_ACTIVE;

    /* Every Second */
    if ( wNCounter % 10 == 0 ) 
    {
        /* Toggle LED */
        pin_toggle(GPIO_ONBOARD_LED); 

    }

    if (wNCounter >= 100)
    {
        /* Print the task times every 10 seconds */
        ESP_LOGI(SFR_TAG, "Max Task Time: %5d BG %5d 1ms %5d 100ms", 
            (int)adwMaxTaskTime[eTASK_BG],
            (int)adwMaxTaskTime[eTASK_1MS],
            (int)adwMaxTaskTime[eTASK_100MS]);
        ESP_LOGI(SFR_TAG, "Last Task Time: %5d BG %5d 1ms %5d 100ms", 
            (int)adwLastTaskTime[eTASK_BG], 
            (int)adwLastTaskTime[eTASK_1MS],
            (int)adwLastTaskTime[eTASK_100MS]);
        wNCounter = 0;
    }
    wNCounter++;

    /* Check if the CAN bus is in error state and recover */
    CAN_bus_diagnosics();

    /* Update max task time */
    qwtTaskTimer = esp_timer_get_time() - qwtTaskTimer;
    adwLastTaskTime[eTASK_100MS] = (dword)qwtTaskTimer;
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

