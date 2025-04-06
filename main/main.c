/*
 * SPDX-FileCopyrightText: 2010-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/*
 * The following example demonstrates a master node in a TWAI network. The master
 * node is responsible for initiating and stopping the transfer of data messages.
 * The example will execute multiple iterations, with each iteration the master
 * node will do the following:
 * 1) Start the TWAI driver
 * 2) Repeatedly send ping messages until a ping response from slave is received
 * 3) Send start command to slave and receive data messages from slave
 * 4) Send stop command to slave and wait for stop response from slave
 * 5) Stop the TWAI driver
 */
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/twai.h"
#include "main.h"

/* --------------------- Definitions and static variables ------------------ */
//Example Configuration
#define PING_PERIOD_MS          250
#define NO_OF_DATA_MSGS         50
#define NO_OF_ITERS             3
#define ITER_DELAY_MS           1000
#define RX_TASK_PRIO            8
#define TX_TASK_PRIO            9
#define CTRL_TSK_PRIO           10
#define TX_GPIO_NUM             CONFIG_EXAMPLE_TX_GPIO_NUM
#define RX_GPIO_NUM             CONFIG_EXAMPLE_RX_GPIO_NUM


#define ID_MASTER_STOP_CMD      0x0A0
#define ID_MASTER_START_CMD     0x0A1
#define ID_MASTER_PING          0x0A2
#define ID_SLAVE_STOP_RESP      0x0B0
#define ID_SLAVE_DATA           0x0B1
#define ID_SLAVE_PING_RESP      0x0B2

typedef enum {
    TX_SEND_PINGS,
    TX_SEND_START_CMD,
    TX_SEND_STOP_CMD,
    TX_TASK_EXIT,
} tx_task_action_t;

typedef enum {
    RX_RECEIVE_PING_RESP,
    RX_RECEIVE_DATA,
    RX_RECEIVE_STOP_RESP,
    RX_TASK_EXIT,
} rx_task_action_t;

static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_25KBITS();
static const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
static const twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TX_GPIO_NUM, RX_GPIO_NUM, TWAI_MODE_NORMAL);

static const twai_message_t ping_message = {.identifier = ID_MASTER_PING, .data_length_code = 0,
                                            .ss = 1, .data = {0, 0, 0, 0, 0, 0, 0, 0}
                                           };
static const twai_message_t start_message = {.identifier = ID_MASTER_START_CMD, .data_length_code = 0,
                                             .data = {0, 0, 0, 0, 0, 0, 0, 0}
                                            };
static const twai_message_t stop_message = {.identifier = ID_MASTER_STOP_CMD, .data_length_code = 0,
                                            .data = {0, 0, 0, 0, 0, 0, 0, 0}
                                           };

static QueueHandle_t tx_task_queue;
static QueueHandle_t rx_task_queue;
static SemaphoreHandle_t stop_ping_sem;
static SemaphoreHandle_t ctrl_task_sem;
static SemaphoreHandle_t done_sem;

/* --------------------------- Tasks and Functions -------------------------- */

static void twai_receive_task(void *arg)
{
    while (1) {
        rx_task_action_t action;
        xQueueReceive(rx_task_queue, &action, portMAX_DELAY);

        if (action == RX_RECEIVE_PING_RESP) {
            //Listen for ping response from slave
            while (1) {
                twai_message_t rx_msg;
                twai_receive(&rx_msg, portMAX_DELAY);
                if (rx_msg.identifier == ID_SLAVE_PING_RESP) {
                    xSemaphoreGive(stop_ping_sem);
                    xSemaphoreGive(ctrl_task_sem);
                    break;
                }
            }
        } else if (action == RX_RECEIVE_DATA) {
            //Receive data messages from slave
            uint32_t data_msgs_rec = 0;
            while (data_msgs_rec < NO_OF_DATA_MSGS) {
                twai_message_t rx_msg;
                twai_receive(&rx_msg, portMAX_DELAY);
                if (rx_msg.identifier == ID_SLAVE_DATA) {
                    uint32_t data = 0;
                    for (int i = 0; i < rx_msg.data_length_code; i++) {
                        data |= (rx_msg.data[i] << (i * 8));
                    }
                    ESP_LOGI(TAG, "Received data value %"PRIu32, data);
                    data_msgs_rec ++;
                }
            }
            xSemaphoreGive(ctrl_task_sem);
        } else if (action == RX_RECEIVE_STOP_RESP) {
            //Listen for stop response from slave
            while (1) {
                twai_message_t rx_msg;
                twai_receive(&rx_msg, portMAX_DELAY);
                if (rx_msg.identifier == ID_SLAVE_STOP_RESP) {
                    xSemaphoreGive(ctrl_task_sem);
                    break;
                }
            }
        } else if (action == RX_TASK_EXIT) {
            break;
        }
    }
    vTaskDelete(NULL);
}

static void twai_transmit_task(void *arg)
{
    while (1) {
        tx_task_action_t action;
        xQueueReceive(tx_task_queue, &action, portMAX_DELAY);

        if (action == TX_SEND_PINGS) {
            //Repeatedly transmit pings
            ESP_LOGI(TAG, "Transmitting ping");
            while (xSemaphoreTake(stop_ping_sem, 0) != pdTRUE) {
                twai_transmit(&ping_message, portMAX_DELAY);
                vTaskDelay(pdMS_TO_TICKS(PING_PERIOD_MS));
            }
        } else if (action == TX_SEND_START_CMD) {
            //Transmit start command to slave
            twai_transmit(&start_message, portMAX_DELAY);
            ESP_LOGI(TAG, "Transmitted start command");
        } else if (action == TX_SEND_STOP_CMD) {
            //Transmit stop command to slave
            twai_transmit(&stop_message, portMAX_DELAY);
            ESP_LOGI(TAG, "Transmitted stop command");
        } else if (action == TX_TASK_EXIT) {
            break;
        }
    }
    vTaskDelete(NULL);
}

static void twai_control_task(void *arg)
{
    xSemaphoreTake(ctrl_task_sem, portMAX_DELAY);
    tx_task_action_t tx_action;
    rx_task_action_t rx_action;

    for (int iter = 0; iter < NO_OF_ITERS; iter++) {
        ESP_ERROR_CHECK(twai_start());
        ESP_LOGI(TAG, "Driver started");

        //Start transmitting pings, and listen for ping response
        tx_action = TX_SEND_PINGS;
        rx_action = RX_RECEIVE_PING_RESP;
        xQueueSend(tx_task_queue, &tx_action, portMAX_DELAY);
        xQueueSend(rx_task_queue, &rx_action, portMAX_DELAY);

        //Send Start command to slave, and receive data messages
        xSemaphoreTake(ctrl_task_sem, portMAX_DELAY);
        tx_action = TX_SEND_START_CMD;
        rx_action = RX_RECEIVE_DATA;
        xQueueSend(tx_task_queue, &tx_action, portMAX_DELAY);
        xQueueSend(rx_task_queue, &rx_action, portMAX_DELAY);

        //Send Stop command to slave when enough data messages have been received. Wait for stop response
        xSemaphoreTake(ctrl_task_sem, portMAX_DELAY);
        tx_action = TX_SEND_STOP_CMD;
        rx_action = RX_RECEIVE_STOP_RESP;
        xQueueSend(tx_task_queue, &tx_action, portMAX_DELAY);
        xQueueSend(rx_task_queue, &rx_action, portMAX_DELAY);

        xSemaphoreTake(ctrl_task_sem, portMAX_DELAY);
        ESP_ERROR_CHECK(twai_stop());
        ESP_LOGI(TAG, "Driver stopped");
        vTaskDelay(pdMS_TO_TICKS(ITER_DELAY_MS));
    }
    //Stop TX and RX tasks
    tx_action = TX_TASK_EXIT;
    rx_action = RX_TASK_EXIT;
    xQueueSend(tx_task_queue, &tx_action, portMAX_DELAY);
    xQueueSend(rx_task_queue, &rx_action, portMAX_DELAY);

    //Delete Control task
    xSemaphoreGive(done_sem);
    vTaskDelete(NULL);
}

void app_main_old(void)
{
    //Create tasks, queues, and semaphores
    rx_task_queue = xQueueCreate(1, sizeof(rx_task_action_t));
    tx_task_queue = xQueueCreate(1, sizeof(tx_task_action_t));
    ctrl_task_sem = xSemaphoreCreateBinary();
    stop_ping_sem = xSemaphoreCreateBinary();
    done_sem = xSemaphoreCreateBinary();
    xTaskCreatePinnedToCore(twai_receive_task, "TWAI_rx", 4096, NULL, RX_TASK_PRIO, NULL, tskNO_AFFINITY);
    xTaskCreatePinnedToCore(twai_transmit_task, "TWAI_tx", 4096, NULL, TX_TASK_PRIO, NULL, tskNO_AFFINITY);
    xTaskCreatePinnedToCore(twai_control_task, "TWAI_ctrl", 4096, NULL, CTRL_TSK_PRIO, NULL, tskNO_AFFINITY);

    //Install TWAI driver
    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_LOGI(TAG, "Driver installed");

    xSemaphoreGive(ctrl_task_sem);              //Start control task
    xSemaphoreTake(done_sem, portMAX_DELAY);    //Wait for completion

    //Uninstall TWAI driver
    ESP_ERROR_CHECK(twai_driver_uninstall());
    ESP_LOGI(TAG, "Driver uninstalled");

    //Cleanup
    vQueueDelete(rx_task_queue);
    vQueueDelete(tx_task_queue);
    vSemaphoreDelete(ctrl_task_sem);
    vSemaphoreDelete(stop_ping_sem);
    vSemaphoreDelete(done_sem);
}

/* --------------------------- My Code ----------------------------- */
#include "tasks.h"
#include "pin.h"
#include "freertos/timers.h"
#include "driver/gptimer.h"
#include "esp_task_wdt.h"

/* --------------------------- Definitions ----------------------------- */
#define TIMER_INTERVAL_WD       100     // in microseconds
#define TIMER_INTERVAL_GP       100
#define TIMER_INTERVAL_1MS      pdMS_TO_TICKS(1)       // in milliseconds
#define TIMER_INTERVAL_100MS    pdMS_TO_TICKS(100)

/* --------------------------- Global Variables ----------------------------- */
static gptimer_handle_t timerWD;
static gptimer_handle_t timerGP;
static TimerHandle_t timer1ms;
static TimerHandle_t timer100ms;

/* --------------------------- Function prototypes ----------------------------- */
static void timers_INIT(void);
static void main_INIT(void);
static void CAN_INIT(void){};       //TODO
static void GPIO_INIT(void);
void timer_callback_1ms(TimerHandle_t xTimer);
void timer_callback_100ms(TimerHandle_t xTimer); 

/* --------------------------- Functions ----------------------------- */

void app_main(void)
{
    // Register app_main task with the WDT
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL)); 

    esp_task_wdt_delete(xTaskGetIdleTaskHandleForCPU(0));  // always exists

    #if CONFIG_FREERTOS_UNICORE == 0  // Dual-core only
        esp_task_wdt_delete(xTaskGetIdleTaskHandleForCPU(1));
    #endif

    /* Initialise device */
    main_INIT();

    /* Run background task all other tasks are called by interupts */
    while(1)
    {
        task_BG();
    }

}// 1ms task
void timer_callback_1ms(TimerHandle_t xTimer) 
{
    
    //TODO add task timer
    task_1ms();

}

// 100ms task
void timer_callback_100ms(TimerHandle_t xTimer)  
{
    
    //TODO add task timer
    task_100ms();

}

static void main_INIT(void)
{
    timers_INIT();
    CAN_INIT();
    GPIO_INIT();

    // Start the timers
    xTimerStart(timer1ms, 0);
    xTimerStart(timer100ms, 0);
    ESP_ERROR_CHECK(gptimer_start(timerWD));
    ESP_ERROR_CHECK(gptimer_start(timerGP));
}

static void timers_INIT(void)
{
    // Freerunning timers for watchdog timeout and wall clock purposes
    gptimer_config_t config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,  // 1 tick = 1 µs
    };

    // Create and enable both timers
    ESP_ERROR_CHECK(gptimer_new_timer(&config, &timerWD));
    ESP_ERROR_CHECK(gptimer_new_timer(&config, &timerGP));

    ESP_ERROR_CHECK(gptimer_enable(timerWD));
    ESP_ERROR_CHECK(gptimer_enable(timerGP));
    
    // FreeRTOS timers that do not support sub ms timing, but are more suitable for the main tasks
    timer1ms = xTimerCreate("1ms", TIMER_INTERVAL_1MS, pdTRUE, NULL, timer_callback_1ms);
    timer100ms = xTimerCreate("100ms", TIMER_INTERVAL_100MS, pdTRUE, NULL, timer_callback_100ms);

    configASSERT(timer1ms && timer100ms);
}

static void GPIO_INIT(void)
{
    gpio_config_t onboardLEDConfig = {
        .pin_bit_mask = 1ULL << GPIO_ONBOARD_LED,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&onboardLEDConfig);
}


