/* Only define once */
#ifndef SFRESPNow

#include <stdio.h>
#include "nvs_flash.h"
#include "esp_random.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_crc.h"
#include "esp_err.h"
#include "string.h"
#include "sfrtypes.h"

#define CONFIG_ESPNOW_CHANNEL 1
#define ESPNOW_WIFI_IF   ESP_IF_WIFI_AP
#define CAN_QUEUE_LENGTH 115 // Number of CAN frames in the ring buffer
#define MAX_ESPNOW_PAYLOAD 250


esp_err_t espNow_init(void);
esp_err_t esp_now_send_can(void);
#define SFREspNow
#define TX_SIDE  // Comment out this line on the RX side

#endif // SFRESPNow