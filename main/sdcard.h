#ifndef SDCARD

#include "esp_err.h"
#include "pin.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_master.h"
#include "espnow.h"
#include "sfrtypes.h"
#include "dirent.h"

esp_err_t SD_card_init(void);
esp_err_t sdcard_empty_buffer(void);
esp_err_t SD_card_write(byte *abyData);
esp_err_t SD_card_write_CAN(CAN_frame_t stCANFrame, dword dwTimestamp);

#define SDCARD
#endif