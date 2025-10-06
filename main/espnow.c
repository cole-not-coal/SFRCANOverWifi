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
uint8_t byMACAdress[6] = {0x8C, 0xBF, 0xEA, 0xCF, 0x90, 0x34}; // Change to the MAC address of target device

/* --------------------------- Definitions ----------------------------- */

/* --------------------------- Function prototypes --------------------- */
esp_err_t espNow_init(void);
esp_err_t nvs_init(void);
static void espNowTxCallback(const uint8_t *mac_addr, esp_now_send_status_t status);
static void espNowRxCallback(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len);

/* --------------------------- Functions ----------------------------- */
esp_err_t espNow_init(void)
{
    /*
    *===========================================================================
    *   espNow_init
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
    stStatus = nvs_init();
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
            
    #ifdef TX_SIDE
    /* Add Peers */
    esp_now_peer_info_t stPeerInfo = {0};
    memcpy(stPeerInfo.peer_addr, byMACAdress, ESP_NOW_ETH_ALEN);
    stPeerInfo.channel = CONFIG_ESPNOW_CHANNEL;
    stPeerInfo.encrypt = false;
    stStatus = esp_now_add_peer(&stPeerInfo);
    if (stStatus != ESP_OK) {
        ESP_LOGE("ESP-NOW", "Failed to add peer: %s", esp_err_to_name(stStatus));
        return stStatus;
    }
    #endif

    /* Register Callbacks */
    esp_now_register_send_cb(espNowTxCallback);
    esp_now_register_recv_cb(espNowRxCallback);

    return stStatus;
}

esp_err_t nvs_init(void)
{
    /*
    *===========================================================================
    *   nvs_init
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

static void espNowTxCallback(const uint8_t *mac_addr, esp_now_send_status_t stStatus)
{
    /*
    *===========================================================================
    *   espNowTxCallback
    *   Takes:   status - status of send
    *            mac_addr - MAC address of the receiver
    * 
    *   Returns: None
    * 
    *   The callback function for when data is sent via esp now. Currently does
    *   nothing. Do not do anything lengthy in this function, post to a queue
    *   if and handle it in a lower priority task.
    *=========================================================================== 
    *   Revision History:
    *   06/10/25 CP Initial Version
    *
    *===========================================================================
    */

    ESP_LOGI("ESP-NOW","sent to : %02X:%02X:%02X:%02X:%02X:%02X status: %d", 
             mac_addr[0], mac_addr[1], mac_addr[2],
             mac_addr[3], mac_addr[4], mac_addr[5], stStatus);

}

static void espNowRxCallback(const esp_now_recv_info_t *recv_info, const uint8_t *byData, int NLen)
{
     /*
    *===========================================================================
    *   espNowRxCallback
    *   Takes:   recv_info - information about the received data
    *            byData - pointer to the received data
    *            NLen - length of the received data
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

    ESP_LOGI("ESP-NOW","received data : %.*s", NLen, byData);

}


