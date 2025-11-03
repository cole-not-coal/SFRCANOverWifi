#ifndef SFRCAN
#include "sfrtypes.h"

#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "esp_log.h"
#include "esp_err.h"

#include "pin.h"
#include "string.h"
#include "espnow.h"

esp_err_t CAN_init(boolean bEnableRx);
esp_err_t CAN_transmit(twai_node_handle_t stCANBus, CAN_frame_t stFrame);
bool CAN_receive_callback(twai_node_handle_t stCANBus, const twai_rx_done_event_data_t *edata, void *stRxCallback);
esp_err_t CAN_receive_debug();
void CAN_bus_diagnosics();
const char* CAN_error_state_to_string(twai_error_state_t stState);
esp_err_t CAN_empty_buffer(twai_node_handle_t stCANBus);

#define LOG_CAN_FRAME(frame) do { \
    char _buf[128]; \
    int _off = snprintf(_buf, sizeof(_buf), "ID=%u DLC=%u", (unsigned)(frame).dwID, (unsigned)(frame).byDLC); \
    for (int _i = 0; _i < (frame).byDLC && _i < 8; _i++) \
        _off += snprintf(_buf + _off, sizeof(_buf) - _off, " %02X", (unsigned)(frame).abData[_i]); \
    ESP_LOGI("CAN", "%s", _buf); \
} while(0)

#define SFRCAN
#endif