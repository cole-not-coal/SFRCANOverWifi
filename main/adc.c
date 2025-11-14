/*
adc.c
File contains the ADC initialization and calibration functions.
Written by Cole Perera for Sheffield Formula Racing 2025
*/

#include "adc.h"

/* --------------------------- Local Variables ------------------------- */


/* --------------------------- Global Variables ------------------------ */
stADCHandles_t stADCHandle0 =
{
    .eNChannel = ADC_CHANNEL_0, //GPIO0
    .stCalibration = NULL,
}; //Add as needed for more ADC channels

/* --------------------------- Definitions ----------------------------- */


/* --------------------------- Functions ------------------------------- */
esp_err_t adc_register(int NPin, adc_atten_t eNAtten, adc_unit_t eNUnit, stADCHandles_t *stADCHandle)
{
    /*
    *===========================================================================
    *   adc_register
    *   Takes:  NPin: GPIO pin number to read ADC from
    *          eNAtten: ADC attenuation setting
    *          eNUnit: ADC unit to use
    *          stADCHandle0: Pointer to ADC handle structure, contains unit and calibration handles
    * 
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *   Creates a oneshot ADC handle and calibration handle for the specified pin and settings.
    *   The ESP32-C6 has two ADC units, each with multiple channels. Each unit can handle 10
    *   channels refer to the manual for more details. Chapter 39 
    *   https://documentation.espressif.com/esp32-c6_technical_reference_manual_en.pdf
    *=========================================================================== 
    *   Revision History:
    *   31/10/25 CP Initial Version
    *   14/11/25 CP Make work
    *
    *===========================================================================
    */

    esp_err_t NStatus = ESP_OK;

    adc_oneshot_unit_init_cfg_t stADCConfig = {
        .unit_id = eNUnit,
    };

    NStatus = adc_oneshot_new_unit(&stADCConfig, &stADCHandle->stADCUnit);
    if (NStatus != ESP_OK) {
        ESP_LOGE("ADC", "Failed to create ADC unit handle: %s", esp_err_to_name(NStatus));
        return NStatus;
    }

    adc_oneshot_chan_cfg_t stChannelConfig = {
        .atten = eNAtten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    NStatus = adc_oneshot_config_channel(stADCHandle->stADCUnit, stADCHandle->eNChannel, &stChannelConfig);
    if (NStatus != ESP_OK) {
        ESP_LOGE("ADC", "Failed to configure ADC channel: %s", esp_err_to_name(NStatus));
        return NStatus;
    }

    adc_cali_curve_fitting_config_t stCalibrationConfig = {
        .unit_id = eNUnit,
        .chan = NPin,
        .atten = eNAtten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    
    NStatus = adc_cali_create_scheme_curve_fitting(&stCalibrationConfig, &stADCHandle->stCalibration);
    if (NStatus != ESP_OK) {
        ESP_LOGE("ADC", "Failed to create calibration handle: %s", esp_err_to_name(NStatus));
    }

    return NStatus;
}

float adc_read_voltage(stADCHandles_t *stADCHandle)
/*
*===========================================================================
*   adc_read_voltage
*   Takes:  stADCHandle: Pointer to ADC handle structure, contains unit and calibration handles
* 
*   Returns: voltage in volts as float
* 
*   Uses the ADC handle to read the voltage from the specified ADC channel and
*   returns the voltage in volts as a float. Uses the calibration for the 
*   conversion from raw adc counts to volts.
*=========================================================================== 
*   Revision History:
*   31/10/25 CP Initial Version
*
*===========================================================================
*/
{
    int adc_raw = 0;
    int voltage = 0;
    adc_oneshot_read(stADCHandle->stADCUnit, stADCHandle->eNChannel, &adc_raw);
    adc_cali_raw_to_voltage(stADCHandle->stCalibration, adc_raw, &voltage);
    return (float)voltage / 1000.0f; // Convert mV to V
}
