#ifndef SFRCAN
#include "driver/twai.h"
#include "sfrtypes.h"

esp_err_t CAN_receive(twai_handle_t stCANBus);
esp_err_t CAN_transmit(twai_handle_t stCANBus, CAN_frame_t stFrame);
esp_err_t CAN_init(void);
void process_CAN_message(twai_message_t *stMessage);
esp_err_t CAN_empty_buffer(twai_handle_t stCANBus);

#define SFRCAN
#endif