#ifndef ADC_H
#define ADC_H

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "esp_err.h"

#include "pin.h"
#include "sfrtypes.h"

/* --------------------------- Function prototypes --------------------- */
esp_err_t adc_register(adc_atten_t eNAtten, adc_unit_t eNUnit, stADCHandles_t *stADCHandle);
float adc_read_voltage(stADCHandles_t *stADCHandle);
float read_sensor(stADCHandles_t *stADCHandle, stSensorMap_t *stSensorMap);

#endif // ADC_H