/* Only define once */
#ifndef SFREspNow

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

#define CONFIG_ESPNOW_CHANNEL 1
#define ESPNOW_WIFI_IF   ESP_IF_WIFI_AP

esp_err_t espNow_init(void);
#define SFREspNow
//#define TX_SIDE  // Comment out this line on the RX side

#endif