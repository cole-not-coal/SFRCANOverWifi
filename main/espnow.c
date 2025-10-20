/*
espnow.c
File contains the ESP now functions for a esp32c6.

Written by Cole Perera for Sheffield Formula Racing 2025
*/

#include "espnow.h"
#include "sfrtypes.h"

/* --------------------------- Local Types ----------------------------- */
typedef enum {
    EXAMPLE_ESPNOW_SEND_CB,
    EXAMPLE_ESPNOW_RECV_CB,
} espnow_event_id_t;

typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    esp_now_send_status_t status;
} espnow_event_send_cb_t;

typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    uint8_t *data;
    int data_len;
} espnow_event_recv_cb_t;

typedef union {
    espnow_event_send_cb_t send_cb;
    espnow_event_recv_cb_t recv_cb;
} espnow_event_info_t;

typedef struct {
    espnow_event_id_t id;
    espnow_event_info_t info;
} espnow_event_t;

/* --------------------------- Local Variables ------------------------ */

/* --------------------------- Global Variables ----------------------- */
/*
* MAC Adresses of my devices
* 1: 8C:BF:EA:CF:94:24
* 2: 8C:BF:EA:CF:90:34
* 3: 9C:9E:6E:77:AF:50
*/
uint8_t byMACAddress[6] = {0x8C, 0xBF, 0xEA, 0xCF, 0x90, 0x34}; // Change to the MAC address of target device
CAN_frame_t *stCANRingBuffer = NULL;
_Atomic word wRingBufHead = 0; // next write index
_Atomic word wRingBufTail = 0; // next read index

/* --------------------------- Definitions ----------------------------- */
#define PACKED_FRAME_SIZE 11 // 2 bytes ID + 1 byte DLC + 8 bytes data

/* --------------------------- Function prototypes --------------------- */
esp_err_t ESPNOW_init(void);
esp_err_t NVS_init(void);
esp_err_t ESPNOW_fill_buffer(const byte *abyData, byte bNDataLength);
esp_err_t ESPNOW_empty_buffer(void);
static void ESPNOW_tx_callback(const byte *abyMACAddress, esp_now_send_status_t stStatus);
static void ESPNOW_rx_callback(const esp_now_recv_info_t *recv_info, const byte *byData, byte byNLength);


/* --------------------------- Functions ----------------------------- */
esp_err_t ESPNOW_init(void)
{
    /*
    *===========================================================================
    *   ESPNOW_init
    *   Takes:   None
    * 
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *   Installs the wifi driver and starts the esp now service.
    *=========================================================================== 
    *   Revision History:
    *   04/05/25 CP Initial Version
    *
    *===========================================================================
    */
    wifi_init_config_t stWifiConfig = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t stStatus;

    /* Initialise NVS */
    stStatus = NVS_init();
    if (stStatus != ESP_OK)
    {
        ESP_LOGE("NVS", "Failed to start: %s", esp_err_to_name(stStatus));
        return stStatus;
    }
    
    /* Initialise Wifi */
    esp_netif_init();
    esp_event_loop_create_default();
    esp_wifi_init(&stWifiConfig);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_ps(WIFI_PS_NONE);
    stStatus = esp_wifi_start();
    if (stStatus != ESP_OK) {
        ESP_LOGE("WiFi", "Failed to start: %s", esp_err_to_name(stStatus));
        return stStatus;
    }
    esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    stStatus = esp_now_init();
    if (stStatus != ESP_OK) {
        ESP_LOGE("ESP-NOW", "Failed to start: %s", esp_err_to_name(stStatus));
        return stStatus;
    }
    
    /* Print this ESP's MAC Address */
    uint8_t abyThisESPMacAddr[6];
    esp_read_mac(abyThisESPMacAddr, ESP_MAC_WIFI_STA);
    ESP_LOGI("ESP-NOW", "ESP MAC Address: %02X:%02X:%02X:%02X:%02X:%02X", 
             abyThisESPMacAddr[0], abyThisESPMacAddr[1], abyThisESPMacAddr[2],
             abyThisESPMacAddr[3], abyThisESPMacAddr[4], abyThisESPMacAddr[5]);
    
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

    #ifdef TX_SIDE
    /* Add Peers */
    esp_now_peer_info_t stPeerInfo = {0};
    memcpy(stPeerInfo.peer_addr, byMACAddress, ESP_NOW_ETH_ALEN);
    stPeerInfo.channel = CONFIG_ESPNOW_CHANNEL;
    stPeerInfo.encrypt = false;
    stStatus = esp_now_add_peer(&stPeerInfo);
    if (stStatus != ESP_OK) {
        ESP_LOGE("ESP-NOW", "Failed to add peer: %s", esp_err_to_name(stStatus));
        return stStatus;
    }
    #endif

    /* Register Callbacks */
    esp_now_register_send_cb(ESPNOW_tx_callback);
    esp_now_register_recv_cb(ESPNOW_rx_callback);

    return stStatus;
}

esp_err_t NVS_init(void)
{
    /*
    *===========================================================================
    *   NVS_init
    *   Takes:   None
    * 
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *   Installs the flash driver and starts the storage manager. Needed for the
    *   wifi driver to work for some reason.
    *=========================================================================== 
    *   Revision History:
    *   04/05/25 CP Initial Version
    *
    *===========================================================================
    */
    esp_err_t stStatus;
    stStatus = nvs_flash_init();
    /* If flash is full wipe it and retry */
    if (stStatus == ESP_ERR_NVS_NO_FREE_PAGES || stStatus == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        stStatus = nvs_flash_erase();
        if (stStatus != ESP_OK)
        {
            ESP_LOGE("NVS", "Failed to erase flash: %s", esp_err_to_name(stStatus));
            return stStatus;
        }
        stStatus = nvs_flash_init();
    }
    return stStatus;
}

static void ESPNOW_tx_callback(const byte *abyMACAddress, esp_now_send_status_t stStatus)
{
    /*
    *===========================================================================
    *   ESPNOW_tx_callback
    *   Takes:   stStatus - status of send
    *            abyMACAddress - MAC address of the receiver
    * 
    *   Returns: None
    * 
    *   The callback function for when data is sent via esp now. Currently does
    *   nothing. Do not do anything lengthy in this function.
    *=========================================================================== 
    *   Revision History:
    *   06/10/25 CP Initial Version
    *
    *===========================================================================
    */

    ESP_LOGI("ESP-NOW","sent to : %02X:%02X:%02X:%02X:%02X:%02X status: %d", 
        abyMACAddress[0], abyMACAddress[1], abyMACAddress[2],
        abyMACAddress[3], abyMACAddress[4], abyMACAddress[5], stStatus);

}

static void ESPNOW_rx_callback(const esp_now_recv_info_t *recv_info, const byte *byData, byte byNLength)
{
     /*
    *===========================================================================
    *   ESPNOW_rx_callback
    *   Takes:   recv_info - information about the received data
    *            byData - pointer to the received data
    *            byNLength - length of the received data
    * 
    *   Returns: None
    * 
    *   The callback function for when data is received via esp now. When a packet
    *   is Rxed add it to a queue for processing. Do not do anything lengthy in 
    *   this function, post to a queue
    *   and handle it from a lower priority task.
    *=========================================================================== 
    *   Revision History:
    *   06/10/25 CP Initial Version
    *
    *===========================================================================
    */

    #ifdef RX_SIDE
    ESPNOW_fill_buffer(byData, byNLength);
    #endif
    
    ESP_LOGI("ESP-NOW", "Data received: %.*s", byNLength, byData); 

}

esp_err_t ESPNOW_empty_buffer(void)
{
    /*
    *===========================================================================
    *   ESPNOW_empty_buffer
    *   Takes:   None
    * 
    *   Returns: stStatus - ESP_OK if successful, error code if not.
    * 
    *   Empties the CAN ring buffer by packing as many CAN frames as possible
    *   into a single ESP-NOW packet (250 bytes) and sending it. If there are no
    *   frames to send, it returns ESP_OK. Each CAN frame takes 11 bytes in the
    *   ESP-NOW packet (2 bytes ID, 1 byte DLC, 8 bytes data). The ring buffer is
    *   115 frames in total so it can take up to 3 ESP-NOW packets to empty
    *   the buffer if it is full. This function only sends one ESP-NOW packet per
    *   call, it is intended to be run once per 100ms or so.
    * 
    *=========================================================================== 
    *   Revision History:
    *   08/10/25 CP Initial Version
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
    dword dwOffset = 0;

    /* Until the ring buffer is empty or the ESP-NOW message is full, pack the message */ 
    while (dwOffset + PACKED_FRAME_SIZE <= MAX_ESPNOW_PAYLOAD && dwLocalTail != dwLocalHead) 
    {
        CAN_frame_t stCANFrame = stCANRingBuffer[dwLocalTail];

        byBytesToSend[dwOffset + 0] = (byte)(stCANFrame.dwId & 0xFF);
        byBytesToSend[dwOffset + 1] = (byte)((stCANFrame.dwId >> 8) & 0xFF);
        byBytesToSend[dwOffset + 2] = (byte)stCANFrame.bDLC;
        memcpy(&byBytesToSend[dwOffset + 3], stCANFrame.abData, 8);
        dwOffset += PACKED_FRAME_SIZE;

        /* Advance tail */ 
        dwLocalTail++;
        if (dwLocalTail >= CAN_QUEUE_LENGTH) 
        {
            dwLocalTail = 0;
        }
    }
    /* Publish new tail */
    __atomic_store_n(&wRingBufTail, dwLocalTail, __ATOMIC_RELEASE);

    /* If data is present send it otherwise return ESP_OK */
    if (dwOffset > 0) 
    {
        return esp_now_send(byMACAddress, byBytesToSend, dwOffset);
        ESP_LOGI("ESP-NOW", "Data sent: %.*s", (int)dwOffset, byBytesToSend); 
    }
    
    return ESP_OK;
}

esp_err_t ESPNOW_fill_buffer(const byte *abyData, byte bNDataLength)
{
    /*
    *===========================================================================
    *   ESPNOW_fill_buffer
    *   Takes:   abData - pointer to data Rxed over ESP-NOW
    *            bNDataLength - length of data Rxed
    * 
    *   Returns: None
    * 
    *   Processes data received over ESP-NOW and adds it to the CAN ring buffer.
    *   The ring buffer is intended to be emptied by a seperate CAN task. If the
    *   buffer is full (or not initialised) the message will be dropped.
    * 
    *=========================================================================== 
    *   Revision History:
    *   15/10/25 CP Initial Version
    *
    *===========================================================================
    */
    if (bNDataLength <= 0) 
    {
        return ESP_OK;
    }

    /* Check out Ring buffer Vars */
    if (!stCANRingBuffer) 
    {
        return ESP_ERR_INVALID_STATE;
    }
    word wLocalHead = __atomic_load_n(&wRingBufHead, __ATOMIC_RELAXED);
    word wLocalTail = __atomic_load_n(&wRingBufTail, __ATOMIC_ACQUIRE);
    word wNext = wLocalHead + 1;
    
    byte offset = 0;
    while (offset + PACKED_FRAME_SIZE <= bNDataLength) {
        CAN_frame_t stFrame;
        dword dwId = ((dword)abyData[offset + 1] << 8)  |
                        ((dword)abyData[offset + 0]);
        stFrame.dwId = dwId;
        stFrame.bDLC = abyData[offset + 2];
        memcpy(stFrame.abData, &abyData[offset + 3], 8);

        /* Add Frame to Ring Buffer */
        if (wNext >= CAN_QUEUE_LENGTH) 
        {
            wNext = 0;
        }
        if (wNext == wLocalTail) 
        {
            /* Buffer full, drop remaining frames */
            __atomic_store_n(&wRingBufHead, wNext, __ATOMIC_RELEASE);
            return ESP_ERR_NO_MEM;
        }
        stCANRingBuffer[wLocalHead] = stFrame;
        wNext++;
        offset += PACKED_FRAME_SIZE;
    }

    /* Publish new head */
    __atomic_store_n(&wRingBufHead, wNext--, __ATOMIC_RELEASE);
    return ESP_OK;
}
