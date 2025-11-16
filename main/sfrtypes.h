/*
sfrtypes.h | Sheffield Formula Racing
Generic types for SFR codebase

Written by Cole Perera for Sheffield Formula Racing 2025
*/
#define DEBUG
#ifndef SFRTypes
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#define TRUE 1
#define FALSE 0

typedef int boolean;

typedef unsigned char byte;
typedef signed char sbyte;

typedef unsigned short word;
typedef signed short sword;

typedef unsigned long dword;
typedef signed long sdword;

typedef unsigned long long qword;
typedef signed long long sqword;

typedef struct {
    dword dwID;      // CAN ID (11- bit packed in 16-bit)
    byte  byDLC;      // 0-8 (Data Length Code)
    byte  abData[8];  // up to 8 bytes
} CAN_frame_t;

typedef struct {
    adc_cali_handle_t stCalibration;
    adc_oneshot_unit_handle_t stADCUnit;
    adc_channel_t eNChannel;
} stADCHandles_t;

typedef struct {
    float fLowerLimit;
    float fUpperLimit;
    float afLookupTable[2][101];
} stSensorMap_t;

#define SFRTypes
#endif