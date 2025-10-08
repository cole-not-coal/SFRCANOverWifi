#ifndef SFRTasks
#include "driver/gpio.h"
#include "sfrtypes.h"

/* Function Definitions*/
void task_BG(void);
void task_1ms(void);
void task_100ms(void);
void pin_toggle(gpio_num_t pin);

#define SFRTasks
#endif