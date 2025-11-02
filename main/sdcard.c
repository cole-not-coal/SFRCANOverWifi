/*
sdcard.c
File contains the SDCard functions for a esp32c6.

Written by Cole Perera for Sheffield Formula Racing 2025
*/

#include "sdcard.h"

/* --------------------------- Global Variables ----------------------------- */
static const char *SD_MOUNT_POINT = "/sdcard";
static char abyFilePath[64] = "/sdcard/log000.txt";

/* --------------------------- Local Variables ------------------------------ */
extern dword dwTimeSincePowerUpms;
extern CAN_frame_t *stCANRingBuffer;
extern _Atomic word wRingBufHead;
extern _Atomic word wRingBufTail;

/* --------------------------- Function prototypes -------------------------- */
esp_err_t SD_card_init(void);

/* --------------------------- Definitions ---------------------------------- */
#define MAX_FILES 5
#define ALLOCATION_UNIT_SIZE 16 * 1024
#define SDMMC_FREQ 10000 // 10 kHz
#define MAX_TRANSFER_SIZE 4000 // max transfer size of one spi operation (bytes)

/* --------------------------- Functions ------------------------------------ */

esp_err_t SD_card_init(void)
{
    /*
    *===========================================================================
    *   SD_card_init
    *   Takes:   None
    * 
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *   Initializes the SD card interface.
    *===========================================================================
    *   Revision History:
    *   20/10/25 CP Initial Version
    *
    *===========================================================================
    */
    esp_err_t stStatus = ESP_OK;
    esp_vfs_fat_sdmmc_mount_config_t stSDMountConfig = 
    {
        .format_if_mount_failed = true,
        .max_files = MAX_FILES,
        .allocation_unit_size = ALLOCATION_UNIT_SIZE
    };
    sdmmc_card_t *stSDCard;
    sdmmc_host_t stSDCardHost = SDSPI_HOST_DEFAULT();
    stSDCardHost.max_freq_khz = SDMMC_FREQ;
    spi_bus_config_t stBusConfig = 
    {
        .mosi_io_num = SPI_MOSI,
        .miso_io_num = SPI_MISO,
        .sclk_io_num = SPI_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = MAX_TRANSFER_SIZE,
    };
    sdspi_device_config_t stSDCardSlot = SDSPI_DEVICE_CONFIG_DEFAULT();
    stSDCardSlot.gpio_cs = SPI_SD_CS;
    stSDCardSlot.host_id = stSDCardHost.slot;

    ESP_LOGI("SDCARD", "Initializing SD card...");
    stStatus = spi_bus_initialize(stSDCardHost.slot, &stBusConfig, SDSPI_DEFAULT_DMA);
    if (stStatus != ESP_OK)
    {
        ESP_LOGE("SDCARD", "Failed to initialize SPI bus: %s", esp_err_to_name(stStatus));
        return stStatus;
    }

    ESP_LOGI("SDCARD", "Mounting filesystem...");
    stStatus = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &stSDCardHost, &stSDCardSlot, &stSDMountConfig, &stSDCard);
    if (stStatus != ESP_OK)
    {
        if (stStatus == ESP_FAIL)
        {
            ESP_LOGE("SDCARD", "Failed to mount filesystem. Formatting...");
            stStatus = esp_vfs_fat_sdcard_format("/sdcard", stSDCard);
            if (stStatus != ESP_OK)
            {
                ESP_LOGE("SDCARD", "Failed to format: %s", esp_err_to_name(stStatus));
                return stStatus;
            }
        }
        else
        {
            ESP_LOGE("SDCARD", "Failed to mount filesystem: %s", esp_err_to_name(stStatus));
            return stStatus;
        }
    }
    ESP_LOGI("SDCARD", "Mounted successfully.");
    /* Print Card Details */
    sdmmc_card_print_info(stdout, stSDCard);
    /* Query card to update file abyFilePath */
    DIR *stDirectory;
    stDirectory = opendir(SD_MOUNT_POINT);
    if (stDirectory == NULL)
    {
        ESP_LOGE("SDCARD", "Failed to open directory");
        return ESP_ERR_INVALID_STATE;
    }
    struct dirent *stDirInfo;
    stDirInfo = readdir(stDirectory);
    word wNLastFile = 0;
    while (stDirInfo != NULL)
    {
        wNLastFile++;
        stDirInfo = readdir(stDirectory);
    }
    closedir(stDirectory);
    snprintf(abyFilePath, sizeof(abyFilePath), "%s/log%03d.txt", SD_MOUNT_POINT, (int)wNLastFile);

   return stStatus;
}

esp_err_t SD_card_write(byte *abyData)
{
    /*
    *===========================================================================
    *   SD_card_write
    *   Takes:   abyPath - abyFilePath to file to write to
    *            abyData - data to write
    * 
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *   Writes data to a file on the SD card.
    *===========================================================================
    *   Revision History:
    *   20/10/25 CP Initial Version
    *
    *===========================================================================
    */

    FILE *stFile = fopen(abyFilePath, "a");
    if (stFile == NULL)
    {
        return ESP_FAIL;
    }
    fprintf(stFile, "%s\n", abyData);
    fclose(stFile);
    return ESP_OK;
}

esp_err_t SD_card_write_CAN(CAN_frame_t stCANFrame, dword dwTimestamp)
{
    /*
    *===========================================================================
    *   SD_card_write_CAN
    *   Takes:   abyPath - abyFilePath to file to write to
    *            stCANFrame - pointer to CAN frame to write
    *            dwTimestamp - time since power on (seconds)
    * 
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *   Writes a CAN frame to a file on the SD card.
    *===========================================================================
    *   Revision History:
    *   21/10/25 CP Initial Version
    *
    *===========================================================================
    */

    FILE *stFile = fopen(abyFilePath, "a");
    if (stFile == NULL)
    {
        return ESP_FAIL;
    }
    fprintf(stFile, "%d: %d %X ", (int)dwTimeSincePowerUpms/1000, (int)stCANFrame.dwID, (int)stCANFrame.byDLC);
    for (byte i = 0; i < stCANFrame.byDLC; i++)
    {
        fprintf(stFile, " %02X", (int)stCANFrame.abData[i]);
    }
    fprintf(stFile, "\n");
    fclose(stFile);
    return ESP_OK;
};

esp_err_t sdcard_empty_buffer(void)
{
    /*
    *===========================================================================
    *   sdcard_empty_buffer
    *   Takes:   None
    * 
    *   Returns: stStatus - ESP_OK if successful, error code if not.
    * 
    *   Empties the CAN ring buffer dumping the contents into the sdcard. If there 
    *   is no data to append, it returns ESP_OK. The ring buffer is
    *   115 frames in total.
    * 
    *=========================================================================== 
    *   Revision History:
    *   24/10/25 CP Initial Version
    *
    *===========================================================================
    */

    /* Load ring buffer head and tail */
    if (!stCANRingBuffer) 
    {
        return ESP_ERR_INVALID_STATE;
    }
    dword dwLocalHead = __atomic_load_n(&wRingBufHead, __ATOMIC_ACQUIRE);
    dword dwLocalTail = __atomic_load_n(&wRingBufTail, __ATOMIC_RELAXED);

    /* Load file */
    FILE *stFile = fopen(abyFilePath, "a");
    if (stFile == NULL)
    {
        return ESP_FAIL;
    }

    /* Until the ring buffer is empty, write lines to file */ 
    while (dwLocalTail != dwLocalHead) 
    {
        #ifdef DEBUG
        ESP_LOGI("SDCARD", "Writing CAN Frame to SD Card");
        #endif
        CAN_frame_t stCANFrame = stCANRingBuffer[dwLocalTail];
        fprintf(stFile, "%d: %d %X ", (int)dwTimeSincePowerUpms/1000, (int)stCANFrame.dwID, (int)stCANFrame.byDLC);
        for (byte i = 0; i < stCANFrame.byDLC; i++)
        {
            fprintf(stFile, " %02X", (int)stCANFrame.abData[i]);
        }
        fprintf(stFile, "\n");

        /* Advance tail */ 
        dwLocalTail++;
        if (dwLocalTail >= CAN_QUEUE_LENGTH) 
        {
            dwLocalTail = 0;
        }
    }
    /* Publish new tail */
    __atomic_store_n(&wRingBufTail, dwLocalTail, __ATOMIC_RELEASE);
    fclose(stFile);
    
    return ESP_OK;
}