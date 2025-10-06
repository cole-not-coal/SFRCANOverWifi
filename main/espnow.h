/* Only define once */
#ifndef SFREspNow
#include "nvs_flash.h"
#include "esp_random.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_crc.h"

#define CONFIG_ESPNOW_CHANNEL 1
#define ESPNOW_WIFI_IF   ESP_IF_WIFI_AP

esp_err_t espNow_init(void);
#define SFREspNow

/*
* MAC Adresses of my devices
* 1: 8C:BF:EA:CF:94:24
* 2: 8C:BF:EA:CF:90:34
* 3: 9C:9E:6E:77:AF:50
*/
#define byMACADRDRESS[6] = {0x24, 0x6F, 0x28, 0x00, 0x00, 0x00} // Change to the MAC address of target device

#endif