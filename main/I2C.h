#ifndef SFRTIMER

#include "sfrtypes.h"

#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c_master.h"

#include "pin.h"

/* --------------------------- Function prototypes -------------------------- */
esp_err_t I2C_init(void);
esp_err_t I2C_write( uint8_t *abyData, size_t NDataLength);
esp_err_t I2C_read( uint8_t *abyData, size_t NDataLength);
esp_err_t eternal_clock_read_time(void);
esp_err_t eternal_clock_write_time(int year, int month, int day, int hour, int min, int sec);

#define SFRTIMER
#endif