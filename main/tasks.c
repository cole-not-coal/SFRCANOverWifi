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
extern esp_reset_reason_t eResetReason;
extern stADCHandles_t stADCHandle0;

/* --------------------------- Global Variables ----------------------------- */
dword adwMaxTaskTime[eTASK_TOTAL];
dword adwLastTaskTime[eTASK_TOTAL];
eTaskState_t astTaskState[eTASK_TOTAL];
dword dwTimeSincePowerUpms = 0;
word wfPWM;
word wrPWMDutyCycle;

/* --------------------------- Definitions ----------------------------- */
#define TIME_BETWEEN_CAN_MSGS 100  // ms
#define IMDOFF_BITSHIFT              0
#define IMDUV_BITSHIFT               1
#define IMDSTARTING_BITSHIFT         2
#define IMDSSTGOOD_BITSHIFT          3
#define IMDDEVICEERROR_BITSHIFT      4
#define IMDGROUNDFAULT_BITSHIFT      5
#define IMDINVALIDSTATE_BITSHIFT     6
#define PWM_CROSSOVER_THRESHOLD      1.5  // Volts

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
        (void)esp_task_wdt_reset();           
        
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
    qword qwtTaskTimer;
    static qword qwtLastRisingEdge = 0;
    static qword qwtPWMPeriod = 0;
    qwtTaskTimer = esp_timer_get_time();
    astTaskState[eTASK_1MS] = eTASK_ACTIVE;
    static boolean BLastPWMState = FALSE;
    boolean BCurrentPWMState;
    if (adc_read_voltage(&stADCHandle0) > PWM_CROSSOVER_THRESHOLD)
    {
        BCurrentPWMState = TRUE;
        
    } else
    {
        BCurrentPWMState = FALSE;
        
    }
    if (BLastPWMState != BCurrentPWMState) {
        BLastPWMState ^= TRUE;
        if (BLastPWMState)
        {
            /* Rising Edge Detected */
            qwtPWMPeriod = esp_timer_get_time() - qwtLastRisingEdge;
            qwtLastRisingEdge = esp_timer_get_time();
        } else
        {
            /* Falling Edge Detected */
            wrPWMDutyCycle = (word)100*(esp_timer_get_time() - qwtLastRisingEdge) / (float)qwtPWMPeriod;
        }
    }

    if ((esp_timer_get_time() - qwtLastRisingEdge) > 1000000ULL)
    {
        /* No signal detected for 1 seconds, reset frequency and duty cycle */
        wfPWM = 0;
        wrPWMDutyCycle = 0;
    } else
    {
        wfPWM = (word)(1000000ULL / qwtPWMPeriod);
    }

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
    boolean BIMDOff = FALSE;
    boolean BIMDUnderVoltage = FALSE;
    boolean BIMDStarting = FALSE;
    boolean BIMDSSTGood = FALSE;
    boolean BIMDDeviceError = FALSE;
    boolean BIMDGroundConnectionFault = FALSE;
    boolean BIMDInvalidState = FALSE;
    float fRIsolation = 0.0f;

    qwtTaskTimer = esp_timer_get_time();
    astTaskState[eTASK_100MS] = eTASK_ACTIVE;

    /* Every Second */
    if ( wNCounter % 10 == 0 ) 
    {
        /* Toggle LED */
        pin_toggle(GPIO_ONBOARD_LED); 

        /* Send Status Message */
        CAN_transmit(stCANBus0, (CAN_frame_t)
        {
            .dwID = 0xFF, // UPDATE THIS FOR EACH DEVICE
            .byDLC = 8,
            .abData = {
                (byte)(adwLastTaskTime[eTASK_1MS] / 50 & 0xFF),          
                (byte)(adwMaxTaskTime[eTASK_1MS] / 50 & 0xFF),
                (byte)(adwLastTaskTime[eTASK_100MS] / 500 & 0xFF),       
                (byte)(adwMaxTaskTime[eTASK_100MS] / 500 & 0xFF),
                (byte)(adwLastTaskTime[eTASK_BG] / 500 & 0xFF),        
                (byte)(adwMaxTaskTime[eTASK_BG] / 500 & 0xFF),
                (byte)((dwTimeSincePowerUpms/4000) >> 6 & 0xFF),
                (byte)(((dwTimeSincePowerUpms/4000) << 2 | (eResetReason & 0x03)) & 0xFF)
            }
        });

    };

    /* Decode PWM Signal from IMD */
    switch (wfPWM)
    {
    case 0 ... 7:
    {
        BIMDOff = TRUE;
        break;
    }
    case 8 ... 25:
    {
        switch (wrPWMDutyCycle)
        {
        case 0 ... 5:
        {
            fRIsolation = 5e6f; // 5 Mohm
            BIMDInvalidState = TRUE;
            break;
        }
        case 6 ... 95:
        {
            fRIsolation = (90*1.2e6f)/(wrPWMDutyCycle - 5.0f) - 1.2e6f; 
            break;
        }
        case 96 ... 100:
        {
            fRIsolation = 0.0f;
            BIMDInvalidState = TRUE;
            break;
        }
        default:
        {
            fRIsolation = 0.0f;
            BIMDInvalidState = TRUE;
            break;
        }
        }
        if (wfPWM > 15)
        {
            BIMDUnderVoltage = TRUE;
        } else
        {
            BIMDUnderVoltage = FALSE;
        }
        break;
    }
    case 26 ... 35:
    {
        BIMDStarting = TRUE;
        switch (wrPWMDutyCycle)
        {
        case 5 ... 10:
        {
            BIMDSSTGood = TRUE;
            break;
        }
        case 90 ... 95:
        {
            BIMDSSTGood = FALSE;
            break;
        }
        default:
        {
            BIMDInvalidState = TRUE;
            break;
        }
        }
        break;
    }
    case 36 ... 45:
    {
        if (46 <wrPWMDutyCycle && wrPWMDutyCycle < 53)
        {
            BIMDDeviceError = TRUE;
        } else
        {
            BIMDInvalidState = TRUE;
        }
        break;
    }
    case 46 ... 55:
    {
        if (46 <wrPWMDutyCycle && wrPWMDutyCycle < 53)
        {
            BIMDGroundConnectionFault = TRUE;
        } else
        {
            BIMDInvalidState = TRUE;
        }
        break;
    }
    default:
    {
        BIMDInvalidState = TRUE;
        break;
    }
    }

    /* Create Status Byte */
    byte byStatusByte = 0;
    byStatusByte |= (BIMDOff                    << IMDOFF_BITSHIFT);
    byStatusByte |= (BIMDUnderVoltage           << IMDUV_BITSHIFT);
    byStatusByte |= (BIMDStarting               << IMDSTARTING_BITSHIFT);
    byStatusByte |= (BIMDSSTGood                << IMDSSTGOOD_BITSHIFT);
    byStatusByte |= (BIMDDeviceError            << IMDDEVICEERROR_BITSHIFT);
    byStatusByte |= (BIMDGroundConnectionFault  << IMDGROUNDFAULT_BITSHIFT);
    byStatusByte |= (BIMDInvalidState           << IMDINVALIDSTATE_BITSHIFT);

    /* Send 0x400 CAN message */
    #ifdef DEBUG
        //ESP_LOGI(SFR_TAG, "IMD PWM Frequency: %d Hz, Duty Cycle: %d %%", wfPWM, wrPWMDutyCycle);
        //ESP_LOGI(SFR_TAG, "IMD Status Byte: 0x%02X, Riso: %.2f KOhms", byStatusByte, fRIsolation/1000.0f);
        //ESP_LOGI(SFR_TAG, "ADC Read: %.2f", adc_read_voltage(&stADCHandle0));
    #endif
    CAN_transmit(stCANBus0, (CAN_frame_t)
    {
        .dwID = 0x400,
        .byDLC = 3,
        .abData = {
            (byte)byStatusByte,
            (byte)((word)fRIsolation*200 >> 8),
            (byte)((word)fRIsolation*200 & 0xFF),
            0,
            0,
            0,
            0,
            0
        }
    });

    /* Every 10 Seconds */
    if (wNCounter >= 100)
    {
        /* Print the task times */
        #ifdef DEBUG
        ESP_LOGI(SFR_TAG, "Max Task Time: %5d BG %5d 1ms %5d 100ms", 
            (int)adwMaxTaskTime[eTASK_BG],
            (int)adwMaxTaskTime[eTASK_1MS],
            (int)adwMaxTaskTime[eTASK_100MS]);
        ESP_LOGI(SFR_TAG, "Last Task Time: %5d BG %5d 1ms %5d 100ms", 
            (int)adwLastTaskTime[eTASK_BG], 
            (int)adwLastTaskTime[eTASK_1MS],
            (int)adwLastTaskTime[eTASK_100MS]);
        wNCounter = 0;
        #endif
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

