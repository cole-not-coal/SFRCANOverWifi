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

#define SFRCAN
#endif