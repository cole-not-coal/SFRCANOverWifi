#ifndef SFRCAN
#include "driver/twai.h"
#include "sfrtypes.h"

esp_err_t CAN_receive(twai_handle_t *stCANBus);
esp_err_t CAN_transmit(twai_handle_t *stCANBus, dword dwNID, word wDataLength, qword *qwNData);
esp_err_t CAN_init(void);
void process_CAN_message(twai_message_t *stMessage);

#define SFRCAN
#endif