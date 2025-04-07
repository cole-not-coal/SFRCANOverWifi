#include "driver/twai.h"
#include "sfrtypes.h"

void CAN_receive(twai_handle_t *stCANBus);
void CAN_transmit(twai_handle_t *stCANBus, dword dwNID, qword *qwNData);
void CAN_init(void);
void process_CAN_message(twai_message_t *stMessage);