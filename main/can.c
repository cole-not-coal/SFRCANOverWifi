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

#include "esp_log.h"
#include "esp_err.h"

/* --------------------------- Global Variables ----------------------------- */
#ifdef GPIO_CAN0_TX
twai_handle_t stCANBus0;
#endif
#ifdef GPIO_CAN1_TX
twai_handle_t stCANBus1;
#endif
/* --------------------------- Local Variables ------------------------------ */

/* --------------------------- Function prototypes -------------------------- */

/* --------------------------- Definitions ---------------------------------- */
#define CAN0_CONTROLLER_ID 0
#define CAN0_TX_QUEUE_LENGTH 10
#define CAN0_RX_QUEUE_LENGTH 10
#define CAN0_TX_MESSAGE_LENGTH 8

#define CAN1_CONTROLLER_ID 1
#define CAN1_TX_QUEUE_LENGTH 10
#define CAN1_RX_QUEUE_LENGTH 10
#define CAN1_TX_MESSAGE_LENGTH 8

/* --------------------------- Functions ------------------------------------ */
esp_err_t CAN_init(void)
{
    /*
    *===========================================================================
    *   CAN_init
    *   Takes:   None
    * 
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *   Creates and starts all defined CAN busses that have pins specifed in
    *   pin.h.
    *=========================================================================== 
    *   Revision History:
    *   20/04/25 CP Initial Version
    *
    *===========================================================================
    */
    esp_err_t stState0 = ESP_OK;
    esp_err_t stState1 = ESP_OK;

    #ifdef GPIO_CAN0_TX
    //Build Config for CAN0 if RX and TX pins are defined
    twai_general_config_t stConfig = TWAI_GENERAL_CONFIG_DEFAULT_V2(CAN0_CONTROLLER_ID,
                            GPIO_CAN0_TX, GPIO_CAN0_RX, TWAI_MODE_NORMAL);
    stConfig.tx_queue_len = CAN0_TX_QUEUE_LENGTH;
    stConfig.rx_queue_len = CAN0_RX_QUEUE_LENGTH;

    twai_timing_config_t stTimingConfig = TWAI_TIMING_CONFIG_1MBITS();
    twai_filter_config_t stFilterConfig = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    
    //Install driver
    stState0 = twai_driver_install_v2(&stConfig, &stTimingConfig, &stFilterConfig, &stCANBus0);
    if ( stState0 == ESP_OK )
    {
        //If driver successfully installed then start Bus0
        stState0 = twai_start_v2(stCANBus0);    
    }
    #endif

    #ifdef GPIO_CAN1_TX
    //Build config for CAN1 if RX and TX pins are defined
    twai_general_config_t stConfig = TWAI_GENERAL_CONFIG_DEFAULT_V2(CAN1_CONTROLLER_ID,
                            GPIO_CAN1_TX, GPIO_CAN1_RX, TWAI_MODE_NORMAL);
    stConfig.tx_queue_len = CAN1_TX_QUEUE_LENGTH;
    stConfig.rx_queue_len = CAN1_RX_QUEUE_LENGTH;

    twai_timing_config_t stTimingConfig = TWAI_TIMING_CONFIG_1MBITS();
    twai_filter_config_t stFilterConfig = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    //Install driver
    stState1 = twai_driver_install_v2(&stConfig, &stTimingConfig, &stFilterConfig, &stCANBus1);
    if ( stState1 == ESP_OK )
    {
        //If driver successfully installed then start Bus1
        stState1 = twai_start_v2(stCANBus1);    
    }
    #endif

    //Return the highest error code (in most cases both will be ESP_OK = 0)
    if ( stState0 > stState1 )
    {
        return stState0;
    }
    else
    {
        return stState1;
    }
}

esp_err_t CAN_transmit(twai_handle_t *stCANBus, dword dwNID, word wDataLength, qword *qwNData)
{
    /*
    *===========================================================================
    *   CAN_transmit
    *   Takes:   stCANBus: Pointer to the CAN bus handle
    *            dwNID: ID of the message to send
    *            qwNData: Pointer to the data to send
    * 
    *   Returns: ESP_OK if successful, error code if not.dwNID
    * 
    *   Transmits a CAN message on the given CAN bus.
    *=========================================================================== 
    *   Revision History:
    *   20/04/25 CP Initial Version
    *
    *===========================================================================
    */
    twai_message_t stMessage;
    esp_err_t stState;
    twai_status_info_t stBusStatus;

    //Construct message
    stMessage.identifier = dwNID;
    stMessage.data_length_code = wDataLength;
    stMessage.flags = TWAI_MSG_FLAG_NONE;
    memcpy(stMessage.data, qwNData, wDataLength);
    
    //Transmit message
    stState = twai_transmit_v2(&stCANBus, &stMessage, FALSE);
    
    //Return error code
    return stState;
}

esp_err_t CAN_receive(twai_handle_t *stCANBus)
{
    /*
    *===========================================================================
    *   CAN_receive
    *   Takes:   stCANBus: Pointer to the CAN bus handle
    * 
    *   Returns: ESP_OK if successful, error code if not.dwNID
    * 
    *   Checks if data is in the RX buffer and if so sends it to 
    *   process_CAN_messaage().
    *=========================================================================== 
    *   Revision History:
    *   20/04/25 CP Initial Version
    *
    *===========================================================================
    */
    uint32_t dwNalerts;
    esp_err_t stState = twai_read_alerts_v2(stCANBus, &dwNalerts, 1);
    twai_message_t stMessage;
    //if ( stState == ESP_OK && (dwNalerts & TWAI_ALERT_RX_DATA) )
    if ( TRUE )
    {
        stState = twai_receive_v2(stCANBus, &stMessage, FALSE);
        if ( stState != ESP_OK )
        {
            ESP_LOGE("CAN", "Receive failed: %s", esp_err_to_name(stState));
        }
        else
        {
            process_CAN_message(&stMessage);
        }
    }

    return stState;
}

void process_CAN_message(twai_message_t *stMessage)
{
    /*
    *===========================================================================
    *   CAN_receive
    *   Takes:   stMessage: Pointer to a recived can message
    * 
    *   Returns: Nothing.
    * 
    *   Prints the message to the terminal. This is temporary and should be
    *   probably do something else.
    *   TODO: Make this work with the proper SFR types. char... eww
    *=========================================================================== 
    *   Revision History:
    *   20/04/25 CP Initial Version
    *
    *===========================================================================
    */
    char abyMessage[250];
    char abyData[10];
    uint16_t wMsgIndex;
    sprintf(abyMessage, "ID: %03lx |", stMessage->identifier);
    for ( wMsgIndex = 0; wMsgIndex < stMessage->data_length_code; wMsgIndex++ )
    {
        sprintf(abyData, " %02x", (unsigned int)stMessage->data[wMsgIndex]);
        strcat((char)abyMessage, (char)abyData);
    }
    ESP_LOGI("CAN", "%s", abyMessage);
}