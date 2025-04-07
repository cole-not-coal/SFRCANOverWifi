/*
can.c
File contains the CAN functions for a esp32c6.

Written by Cole Perera for Sheffield Formula Racing 2025
*/

#include "esp_err.h"
#include <stdio.h>
#include <stdlib.h>
#include "pin.h"
#include "can.h"
#include "string.h"

/* --------------------------- Global Variables ----------------------------- */

/* --------------------------- Local Variables ----------------------------- */

/* --------------------------- Function prototypes ----------------------------- */

/* --------------------------- Definitions ----------------------------- */
#define CAN_TX_QUEUE_LENGTH 10
#define CAN_RX_QUEUE_LENGTH 10
#define CAN_TX_MESSAGE_LENGTH 8

/* --------------------------- Function prototypes ----------------------------- */
void CAN_receive(twai_handle_t *stCANBus);
void CAN_transmit(twai_handle_t *stCANBus, dword dwNID, qword *qwNData);
void CAN_init(void);
void process_CAN_message(twai_message_t *stMessage);

/* --------------------------- Functions ----------------------------- */
void CAN_init(void)
{
    twai_handle_t stCANBus0;
    twai_general_config_t stConfig = {
        .controller_id = 0,
        .mode = TWAI_MODE_NORMAL,
        .tx_io = GPIO_CAN_TX,
        .rx_io = GPIO_CAN_RX,
        .clkout_io = TWAI_IO_UNUSED,
        .bus_off_io = TWAI_IO_UNUSED,
        .tx_queue_len = CAN_TX_QUEUE_LENGTH,
        .rx_queue_len = CAN_RX_QUEUE_LENGTH,
        .alerts_enabled = TWAI_ALERT_NONE,
    };
    twai_timing_config_t stTimingConfig = TWAI_TIMING_CONFIG_1MBITS();
    twai_filter_config_t stFilterConfig = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    ESP_ERROR_CHECK(twai_driver_install_v2(&stConfig, &stTimingConfig, &stFilterConfig, &stCANBus0));
    ESP_ERROR_CHECK(twai_start_v2(stCANBus0));

}

void CAN_transmit(twai_handle_t *stCANBus, dword dwNID, qword *qwNData)
{
    twai_message_t stMessage;
    stMessage.identifier = dwNID;
    stMessage.data_length_code = CAN_TX_MESSAGE_LENGTH;
    memcpy(stMessage.data, qwNData, CAN_TX_MESSAGE_LENGTH);
    stMessage.flags = TWAI_MSG_FLAG_NONE;
    ESP_ERROR_CHECK(twai_transmit_v2(stCANBus, &stMessage, false));
}

void CAN_receive(twai_handle_t *stCANBus)
{
    uint32_t dwNalerts;
    esp_err_t stState = twai_read_alerts_v2(stCANBus, &dwNalerts, false);
    twai_message_t stMessage;
    if ( stState == ESP_OK && (dwNalerts & TWAI_ALERT_RX_DATA) )
    {
        ESP_ERROR_CHECK(twai_receive_v2(stCANBus, &stMessage, false));
        process_CAN_message(&stMessage);
    }
}

void process_CAN_message(twai_message_t *stMessage)
{
    // TODO make this a thing
    // This function should be implemented according to your application needs

}