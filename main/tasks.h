#ifndef SFRTasks
#include "driver/gpio.h"
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "sfrtypes.h"

#include "pin.h"
#include "can.h"
#include "espnow.h"
#include "sdcard.h"
#include "adc.h"
#include "I2C.h"
#include "NVHDisplay.h"

/* Function Definitions*/
void task_BG(void);
void task_1ms(void);
void task_100ms(void);
void pin_toggle(gpio_num_t pin);

#define SFRTasks
#endif