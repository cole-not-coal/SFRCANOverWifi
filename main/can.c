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
#include "espnow.h"

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
extern CAN_frame_t *stCANRingBuffer;
extern _Atomic word wRingBufHead;
extern _Atomic word wRingBufTail;


/* --------------------------- Function prototypes -------------------------- */
esp_err_t CAN_receive(twai_handle_t stCANBus);
esp_err_t CAN_transmit(twai_handle_t stCANBus, CAN_frame_t stFrame);
esp_err_t CAN_init(void);
void process_CAN_message(twai_message_t *stMessage);
esp_err_t CAN_empty_buffer(twai_handle_t stCANBus);

/* --------------------------- Definitions ---------------------------------- */
#define CAN0_CONTROLLER_ID 0
#define CAN0_TX_QUEUE_LENGTH 10
#define CAN0_RX_QUEUE_LENGTH 10
#define CAN0_TX_MESSAGE_LENGTH 8

#define CAN1_CONTROLLER_ID 1
#define CAN1_TX_QUEUE_LENGTH 10
#define CAN1_RX_QUEUE_LENGTH 10
#define CAN1_TX_MESSAGE_LENGTH 8

#define MAX_CAN_TXS_PER_CALL 1

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
    /* Build Config for CAN0 if RX and TX pins are defined */
    twai_general_config_t stConfig = TWAI_GENERAL_CONFIG_DEFAULT_V2(CAN0_CONTROLLER_ID,
                            GPIO_CAN0_TX, GPIO_CAN0_RX, TWAI_MODE_NORMAL);
    
    stConfig.tx_queue_len = CAN0_TX_QUEUE_LENGTH;
    stConfig.rx_queue_len = CAN0_RX_QUEUE_LENGTH;
    twai_timing_config_t stTimingConfig = TWAI_TIMING_CONFIG_1MBITS();
    twai_filter_config_t stFilterConfig = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    
    /* Install driver */
    stState0 = twai_driver_install_v2(&stConfig, &stTimingConfig, &stFilterConfig, &stCANBus0);
    ESP_LOGI("CAN", "CAN0 driver_install returned: %s, handle=%p", esp_err_to_name(stState0), (void*)stCANBus0);
    if ( stState0 == ESP_OK )
    {
        /* If driver successfully installed then start Bus0 */
        stState0 = twai_start_v2(stCANBus0); 
        ESP_LOGI("CAN", "CAN0 start returned: %s", esp_err_to_name(stState0));   
    }
    #endif

    #ifdef GPIO_CAN1_TX
    /* Build config for CAN1 if RX and TX pins are defined */
    twai_general_config_t stConfig = TWAI_GENERAL_CONFIG_DEFAULT_V2(CAN1_CONTROLLER_ID,
                            GPIO_CAN1_TX, GPIO_CAN1_RX, TWAI_MODE_NORMAL);
    
    stConfig.tx_queue_len = CAN1_TX_QUEUE_LENGTH;
    stConfig.rx_queue_len = CAN1_RX_QUEUE_LENGTH;
    twai_timing_config_t stTimingConfig = TWAI_TIMING_CONFIG_1MBITS();
    twai_filter_config_t stFilterConfig = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    
    /* Install driver */
    stState1 = twai_driver_install_v2(&stConfig, &stTimingConfig, &stFilterConfig, &stCANBus1);
    if ( stState1 == ESP_OK )
    {
        /* If driver successfully installed then start Bus1 */
        stState1 = twai_start_v2(stCANBus1);    
    }
    #endif

    /* Return the highest error code (in most cases both will be ESP_OK = 0) */
    if ( stState0 > stState1 )
    {
        return stState0;
    }
    else
    {
        return stState1;
    }
}

esp_err_t CAN_transmit(twai_handle_t stCANBus, CAN_frame_t stFrame)
{
    /*
    *===========================================================================
    *   CAN_transmit
    *   Takes:   stCANBus: Pointer to the CAN bus handle
    *            dwNID: ID of the message to send
    *            qwNData: Pointer to the data to send
    * 
    *   Returns: ESP_OK if successful, error code if not.
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
    word wDataIndex;

    /* Validate inputs */
    if (!stCANBus) {
        ESP_LOGE("CAN", "CAN_transmit: NULL stCANBus handle");
        return ESP_ERR_INVALID_ARG;
    }

    twai_status_info_t dbgStatus;
    if ( twai_get_status_info_v2(stCANBus, &dbgStatus) == ESP_OK ) {
        ESP_LOGI("CAN", "DBG status state=%d tec=%lu rec=%lu msgs_tx=%lu msgs_rx=%lu",
                 dbgStatus.state, dbgStatus.tx_error_counter, dbgStatus.rx_error_counter, dbgStatus.msgs_to_tx, dbgStatus.msgs_to_rx);
    } else {
        ESP_LOGW("CAN", "DBG: twai_get_status_info_v2 failed");
    }

    /* init message */
    memset(&stMessage, 0, sizeof(stMessage));

    /* Construct message */
    stMessage.identifier = stFrame.dwId;
    stMessage.data_length_code = stFrame.bDLC;
    stMessage.flags = TWAI_MSG_FLAG_NONE;
    memcpy(stMessage.data, stFrame.abData, stMessage.data_length_code);
    
    /* Transmit message */
    stState = twai_transmit_v2(stCANBus, &stMessage, FALSE);
    
    return stState;
}

esp_err_t CAN_receive(twai_handle_t stCANBus)
{
    /*
    *===========================================================================
    *   CAN_receive
    *   Takes:   stCANBus: Pointer to the CAN bus handle
    * 
    *   Returns: ESP_OK if successful, error code if not.dwNID
    * 
    *   Checks if data is in the RX buffer and if adds it to the ring buffer. If
    *   there is no data it returns ESP_OK. If the ring buffer is full it drops
    *   the message.
    *=========================================================================== 
    *   Revision History:
    *   20/04/25 CP Initial Version
    *   08/10/25 CP Updated to implement ring buffer
    *
    *===========================================================================
    */
    dword dwNalerts;
    word wMsgIndex;
    twai_status_info_t stBusStatus;
    esp_err_t stState = twai_get_status_info_v2(stCANBus, &stBusStatus);
    twai_message_t stMessage;
    CAN_frame_t stRxedFrame;
    
    for (wMsgIndex = 0; wMsgIndex < stBusStatus.msgs_to_rx; wMsgIndex++)
    {
        stState = twai_receive_v2(stCANBus, &stMessage, FALSE);
        if ( stState != ESP_OK ) /* Rx Fail */
        {
            ESP_LOGE("CAN", "Receive failed: %s", esp_err_to_name(stState));
        }
        else /* Put CAN Frame into Ring Buffer */
        {
            if (!stCANRingBuffer) 
            {
                return ESP_ERR_INVALID_STATE;
            }

            /* Get local copy of queue head and tail */
            word wLocalHead = __atomic_load_n(&wRingBufHead, __ATOMIC_RELAXED);
            word wNext = wLocalHead + 1;
            if (wNext >= CAN_QUEUE_LENGTH) 
            {
                wNext = 0;
            }
            word wLocalTail = __atomic_load_n(&wRingBufTail, __ATOMIC_ACQUIRE);

            if (wNext == wLocalTail) {
                /* Buffer full, drop frame */
                return ESP_ERR_NO_MEM;
            }

            /* Copy frame into buffer */
            stRxedFrame.dwId = (dword)stMessage.identifier;
            stRxedFrame.bDLC = (byte)stMessage.data_length_code;
            memcpy(stRxedFrame.abData, stMessage.data, stMessage.data_length_code);
            stCANRingBuffer[wLocalHead] = stRxedFrame;

            /* Publish new head */
            __atomic_store_n(&wRingBufHead, wNext, __ATOMIC_RELEASE);

        }
    }

    return stState;
}

void process_CAN_message(twai_message_t *stMessage)
{
    /*
    *===========================================================================
    *   process_CAN_message - Debug function
    *   Takes:   stMessage: Pointer to a received can message
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
    uint16_t wMsgIndex;
    int nPos = 0;

    nPos = snprintf(abyMessage, sizeof(abyMessage), "ID: %03lx |", stMessage->identifier);
    for ( wMsgIndex = 0; wMsgIndex < stMessage->data_length_code && nPos < (int)sizeof(abyMessage) - 4; wMsgIndex++ )
    {
        nPos += snprintf(&abyMessage[nPos], sizeof(abyMessage) - nPos, " %02x", (unsigned int)stMessage->data[wMsgIndex]);
    }
    ESP_LOGI(SFR_TAG, "%s", abyMessage);
}

esp_err_t CAN_empty_buffer(twai_handle_t stCANBus)
{
    /*
    *===========================================================================
    *   CAN_empty_buffer
    *   Takes:   stCANBus: Pointer to the CAN bus handle
    * 
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *   Empties the CAN ring buffer by transmitting messages until the buffer is
    *   empty or the max number of messages per call is reached.
    *=========================================================================== 
    *   Revision History:
    *   15/10/25 CP Initial Version
    *
    *===========================================================================
    */
    esp_err_t stStatus = ESP_OK;
    byte byBytesToSend[MAX_ESPNOW_PAYLOAD];
    if (!stCANRingBuffer) 
    {
        return ESP_ERR_INVALID_STATE;
    }

    /* Load ring buffer head and tail */
    dword dwLocalHead = __atomic_load_n(&wRingBufHead, __ATOMIC_ACQUIRE);
    dword dwLocalTail = __atomic_load_n(&wRingBufTail, __ATOMIC_RELAXED);
    word wCounter = 0;
        
    /* Until the ring buffer is empty or the max number of messages is reached send CAN messages */ 
    while (wCounter < MAX_CAN_TXS_PER_CALL && dwLocalTail != dwLocalHead) 
    {
        CAN_frame_t stCANFrame = stCANRingBuffer[dwLocalTail];   
        stStatus = CAN_transmit(stCANBus, stCANFrame);  
        if (stStatus != ESP_OK) 
        {
            /* Publish new tail */
            __atomic_store_n(&wRingBufTail, dwLocalTail, __ATOMIC_RELEASE);
            return stStatus;
        }

        /* Advance tail */ 
        dwLocalTail++;
        wCounter++;
        if (dwLocalTail >= CAN_QUEUE_LENGTH) 
        {
            dwLocalTail = 0;
        }
    }
    
    /* Publish new tail */
    __atomic_store_n(&wRingBufTail, dwLocalTail, __ATOMIC_RELEASE);
    return stStatus;
}