/*
espnow.c
File contains the ESP now functions for a esp32c6.

Written by Cole Perera for Sheffield Formula Racing 2025
*/

#include "espnow.h"
#include "sfrtypes.h"
#include "esp_err.h"
#include "esp_log.h"

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

/* --------------------------- Local Variables ----------------------------- */

/* --------------------------- Global Variables ----------------------------- */

/* --------------------------- Function prototypes ----------------------------- */

/* --------------------------- Definitions ----------------------------- */

/* --------------------------- Function prototypes ----------------------------- */
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
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t stStatus;

    /* Initialise NVS */
    stStatus = nvs_init();
    if (stStatus != ESP_OK)
    {
        ESP_LOGE("NVS", "Failed to initialise NVS: %s", esp_err_to_name(stStatus));
        return stStatus;
    }
    
    /* Initialise Wifi */
    esp_netif_init();
    esp_event_loop_create_default();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_ps(WIFI_PS_NONE);
    stStatus = esp_wifi_start();
    if (stStatus != ESP_OK) {
        ESP_LOGE("WiFi", "Failed to start WiFi: %s", esp_err_to_name(stStatus));
        return stStatus;
    }
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    
    /* Print this ESP's MAC Address */
    uint8_t mac_addr[6];
    esp_read_mac(mac_addr, ESP_MAC_WIFI_STA);
    ESP_LOGI("ESP-NOW", "ESP MAC Address: %02X:%02X:%02X:%02X:%02X:%02X", 
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
             
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

static void espNowTxCallback(const uint8_t *mac_addr, esp_now_send_status_t status)
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

}

static void espNowRxCallback(const esp_now_recv_info_t *recv_info, const uint8_t *bydata, int Nlen)
{
     /*
    *===========================================================================
    *   espNowRxCallback
    *   Takes:   recv_info - information about the received data
    *            data - pointer to the received data
    *            len - length of the received data
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

    ESP_LOGI("ESP-NOW","received data : %.*s", Nlen, bydata);

}


