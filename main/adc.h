
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "esp_err.h"

#include "pin.h"
#include "sfrtypes.h"

typedef struct {
    adc_cali_handle_t stCalibration;
    adc_oneshot_unit_handle_t stADCUnit;
    adc_channel_t eNChannel;
} stADCHandles_t;

/* --------------------------- Function prototypes --------------------- */
esp_err_t ADC_register(int NPin, ADC_atten_t eNAtten, adc_unit_t eNUnit, stADCHandles_t *stADCHandle);
float adc_read_voltage(stADCHandles_t *stADCHandle);