/*
can.c
File contains the CAN functions for a esp32c6.

Written by Cole Perera for Sheffield Formula Racing 2025
*/

#include <stdio.h>
#include <stdlib.h>
#include "can.h"

/* --------------------------- Global Variables ----------------------------- */
#ifdef GPIO_CAN0_TX
twai_node_handle_t stCANBus0;
#endif
#ifdef GPIO_CAN1_TX
twai_node_handle_t stCANBus1;
#endif
CAN_frame_t *stCANRingBuffer = NULL;
_Atomic word wRingBufHead = 0; // next write index
_Atomic word wRingBufTail = 0; // next read index

/* --------------------------- Local Variables ------------------------------ */


/* --------------------------- Function prototypes -------------------------- */
esp_err_t CAN_init(boolean bEnableRx);
esp_err_t CAN_transmit(twai_node_handle_t stCANBus, CAN_frame_t stFrame);
bool CAN_receive_callback(twai_node_handle_t stCANBus, const twai_rx_done_event_data_t *edata, void *stRxCallback);
esp_err_t CAN_receive_debug();
void CAN_bus_diagnosics();
const char* CAN_error_state_to_string(twai_error_state_t stState);
esp_err_t CAN_empty_buffer(twai_node_handle_t stCANBus);

/* --------------------------- Definitions ---------------------------------- */
#define CAN0_BITRATE 1000000  // 1000kbps
#define CAN0_TX_QUEUE_LENGTH 10

#define CAN1_BITRATE 1000000  // 1 Mbps
#define CAN1_TX_QUEUE_LENGTH 10

#define MAX_CAN_TXS_PER_CALL 1

/* --------------------------- Functions ------------------------------------ */

esp_err_t CAN_init(boolean bEnableRx)
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
    *   29/10/25 CP Updated to use onchip driver, old driver depriecated
    *
    *===========================================================================
    */

    esp_err_t stState = ESP_OK;

    /* Bus 0 */
    #ifdef GPIO_CAN0_TX
    twai_onchip_node_config_t stCANNode0Config = 
    {
        .io_cfg.tx = GPIO_CAN0_TX,
        .io_cfg.rx = GPIO_CAN0_RX,
        .bit_timing.bitrate = CAN0_BITRATE,
        .tx_queue_depth = CAN0_TX_QUEUE_LENGTH,
        .intr_priority = 3,
    };
    stState = twai_new_node_onchip(&stCANNode0Config, &stCANBus0);
    if ( stState != ESP_OK )
    {
        ESP_LOGE("CAN", "CAN0 twai_new_node_onchip failed: %s", esp_err_to_name(stState));  
    }
    if (bEnableRx) 
    {
        twai_event_callbacks_t stRxCallback =
        {
            .on_rx_done = CAN_receive_callback,
        };
        stState = twai_node_register_event_callbacks(stCANBus0, &stRxCallback, NULL);
        if ( stState != ESP_OK )
        {
            ESP_LOGE("CAN", "CAN0 failed to register callback: %s", esp_err_to_name(stState));  
        }
    }
    stState = twai_node_enable(stCANBus0); 
    if ( stState != ESP_OK )
    {
        ESP_LOGE("CAN", "CAN0 Bus failed to start: %s", esp_err_to_name(stState));  
    }

    #endif

    /* Bus 1 */
    #ifdef GPIO_CAN1_TX
    twai_onchip_node_config_t stCANNode1Config = 
    {
        .io_cfg.tx = GPIO_CAN1_TX,
        .io_cfg.rx = GPIO_CAN1_RX,
        .bit_timing.bitrate = CAN1_BITRATE,
        .tx_queue_depth = CAN1_TX_QUEUE_LENGTH,
    };
    stState = twai_new_node_onchip(&stCANNode1Config, &stCANBus1);
    if ( stState != ESP_OK )
    {
        ESP_LOGE("CAN", "CAN1 twai_new_node_onchip failed: %s", esp_err_to_name(stState));  
    }
    stState = twai_node_register_event_callbacks(stCANBus1, &stRxCallback, NULL);
    if ( stState != ESP_OK )
    {
        ESP_LOGE("CAN", "CAN1 failed to register callback: %s", esp_err_to_name(stState));  
    }
    stState = twai_node_enable(stCANBus1); 
    if ( stState != ESP_OK )
    {
        ESP_LOGE("CAN", "CAN1 Bus failed to start: %s", esp_err_to_name(stState));  
    }

    #endif

    /* Allocate Ring Buffer */
    CAN_frame_t *stCANRingBufferInitial = (CAN_frame_t *)malloc(sizeof(CAN_frame_t) 
                                        * CAN_QUEUE_LENGTH);                      
    if (!stCANRingBufferInitial) {
        ESP_LOGE("ESP-NOW", "Failed to allocate ring buffer (len=%u)", CAN_QUEUE_LENGTH);
        return ESP_ERR_NO_MEM;
    }
    stCANRingBuffer = stCANRingBufferInitial;   
    __atomic_store_n(&wRingBufHead, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&wRingBufTail, 0, __ATOMIC_RELAXED);  

    return stState;
}

esp_err_t CAN_transmit(twai_node_handle_t stCANBus, CAN_frame_t stFrame)
{
    /*
    *===========================================================================
    *   CAN_transmit
    *   Takes:   stCANBus: Pointer to the CAN bus handle
    *            stFrame: CAN frame to transmit
    * 
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *   Transmits a CAN message on the given CAN bus. in debug mode it prints the
    *   status of the bus for every call.
    *=========================================================================== 
    *   Revision History:
    *   20/04/25 CP Initial Version
    *   29/10/25 CP Updated to use onchip driver, old driver depriecated
    *   02/11/25 CP Makes transmit work with messages < 8 bytes
    *
    *===========================================================================
    */
    esp_err_t stState;

    /* Construct message */
    uint8_t abyData[(int)stFrame.byDLC];
    memcpy(abyData, stFrame.abData, stFrame.byDLC);
    twai_frame_t stMessage = 
    {
        .header.id  = (uint32_t)stFrame.dwID,
        .header.dlc = (uint16_t)stFrame.byDLC,
        .header.ide = 0,
        .buffer     = abyData,
        .buffer_len = sizeof(abyData)
    };
    
    /* Transmit message */
    stState = twai_node_transmit(stCANBus, &stMessage, FALSE);
    
    return stState;
}

esp_err_t CAN_receive_debug()
{   
    /*
    *===========================================================================
    *   CAN_receive_debug
    *   Takes:   stCANBus: Pointer to the CAN bus handle
    *            edata: No idea read https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/twai.html
    *            stRxCallback: see above
    * 
    *   Returns: ESP_OK if successful, error code if not. debug func, fuck about
    * 
    *=========================================================================== 
    *   Revision History:
    *   29/10/25 CP Initial Version
    *   02/11/25 CP Improved terminal readability
    *
    *===========================================================================
    */

    if (!stCANRingBuffer) 
    {
        return ESP_ERR_INVALID_STATE;
    }

    /* Load ring buffer head and tail */
    dword dwLocalHead = __atomic_load_n(&wRingBufHead, __ATOMIC_ACQUIRE);
    dword dwLocalTail = __atomic_load_n(&wRingBufTail, __ATOMIC_RELAXED);
    word wCounter = 0;
        
    /* Until the ring buffer is empty or the max number of messages is reached send CAN messages */ 
    while (dwLocalTail != dwLocalHead) 
    {
        CAN_frame_t stCANFrame = stCANRingBuffer[dwLocalTail];   

        /* Print CAN Msg */
        LOG_CAN_FRAME(stCANFrame);

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
    return ESP_OK;
}

const char* CAN_error_state_to_string(twai_error_state_t stState) {
    switch (stState) {
        case TWAI_ERROR_ACTIVE:  return "TWAI_ERROR_ACTIVE";
        case TWAI_ERROR_WARNING: return "TWAI_ERROR_WARNING";
        case TWAI_ERROR_PASSIVE: return "TWAI_ERROR_PASSIVE";
        case TWAI_ERROR_BUS_OFF: return "TWAI_ERROR_BUS_OFF";
        default:                 return "UNKNOWN_TWAI_ERROR_STATE";
    }
}

void CAN_bus_diagnosics()
{
    /*
    *===========================================================================
    *   CAN_bus_diagnosics
    *   Takes:   stCANBus: Pointer to the CAN bus handle
    * 
    *   Returns: Nothing.
    * 
    *   Checks the status of the CAN bus and attempts to recover if there is an
    *   error.
    *=========================================================================== 
    *   Revision History:
    *   20/04/25 CP Initial Version
    *
    *===========================================================================
    */

    twai_node_status_t stBusStatus;
    twai_node_record_t stBusStatistics;
    static twai_error_state_t stLastErrorState = TWAI_ERROR_ACTIVE;

    #ifdef GPIO_CAN0_TX
    /* Check if the CAN bus is in error state and recover */
    twai_node_get_info(stCANBus0, &stBusStatus, &stBusStatistics);
    /* Detect state change */
    if (stBusStatus.state != stLastErrorState) {
        ESP_LOGW("CAN", "CAN0 bus error state changed from %s to %s",
            CAN_error_state_to_string(stLastErrorState),
            CAN_error_state_to_string(stBusStatus.state));
        stLastErrorState = stBusStatus.state;
    }
    /* If bad error then restart bus */
    if (stBusStatus.state == TWAI_ERROR_BUS_OFF) 
    {
        ESP_LOGW("CAN", "Recovering bus 0 : %s", esp_err_to_name(twai_node_recover(stCANBus0))); 
    }
    #endif

    #ifdef GPIO_CAN1_TX
    /* Check if the CAN bus is in error state and recover */
    twai_node_get_info(stCANBus1, &stBusStatus, &stBusStatistics);
    /* Detect state change */
    if (stBusStatus.state != stLastErrorState) {
        ESP_LOGW("CAN", "CAN1 bus error state changed from %s to %s",
            CAN_error_state_to_string(stLastErrorState),
            CAN_error_state_to_string(stBusStatus.state));
        stLastErrorState = stBusStatus.state;
    }
    /* If bad error then restart bus */
    if (stBusStatus.state == TWAI_ERROR_BUS_OFF) 
    {
        ESP_LOGW("CAN", "Recovering bus 1: %s", esp_err_to_name(twai_node_recover(stCANBus1))); 
    }
    #endif
}

bool CAN_receive_callback(twai_node_handle_t stCANBus, const twai_rx_done_event_data_t *edata, void *stRxCallback)
{
    /*
    *===========================================================================
    *   CAN_receive_callback
    *   Takes:   stCANBus: Pointer to the CAN bus handle
    *            edata: No idea read https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/twai.html
    *            stRxCallback: see above
    * 
    *   Returns: 1 if successful, 0 not.
    * 
    *   The callback for CAN Rx, adds the message to the ring buffer. 
    *   If the ring buffer is full it drops the message.
    * 
    *=========================================================================== 
    *   Revision History:
    *   20/04/25 CP Initial Version
    *   08/10/25 CP Updated to implement ring buffer
    *   30/10/25 CP Updated to use onchip driver, old driver depriecated
    *
    *===========================================================================
    */

    esp_err_t stState;
    CAN_frame_t stRxedFrame;
    /* Initialise Rx Buffer */
    uint8_t abyRxBuffer[8];
    twai_frame_t stRxFrame = {
        .buffer = abyRxBuffer,
        .buffer_len = sizeof(abyRxBuffer),
    };
    
    stState = twai_node_receive_from_isr(stCANBus, &stRxFrame);
    if ( stState == ESP_OK )
    {
        /* Put CAN Frame into Ring Buffer */
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
        stRxedFrame.dwID = (dword)stRxFrame.header.id;
        stRxedFrame.byDLC = (byte)stRxFrame.header.dlc;
        memcpy(stRxedFrame.abData, stRxFrame.buffer, stRxFrame.header.dlc);
        stCANRingBuffer[wLocalHead] = stRxedFrame;

        /* Publish new head */
        __atomic_store_n(&wRingBufHead, wNext, __ATOMIC_RELEASE);
        return TRUE;
    }

    return FALSE;
}

esp_err_t CAN_empty_buffer(twai_node_handle_t stCANBus)
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
    esp_err_t NStatus = ESP_OK;
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
        NStatus = CAN_transmit(stCANBus, stCANFrame);  
        if (NStatus != ESP_OK) 
        {
            /* Publish new tail */
            __atomic_store_n(&wRingBufTail, dwLocalTail, __ATOMIC_RELEASE);
            return NStatus;
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
    return NStatus;
}