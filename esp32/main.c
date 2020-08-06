/*
 * Copyright (c) 2020, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "controller.h"

#include "esp_bt.h"
#include "common/bt_trace.h"
#include "stack/bt_types.h"
#include "stack/btm_api.h"
#include "bta/bta_api.h"
#include "bta/bta_gatt_api.h"
#include "api/esp_gap_ble_api.h"
#include "api/esp_gattc_api.h"
#include "api/esp_gatt_defs.h"
#include "api/esp_bt_main.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "freertos/xtensa_api.h"

#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_attr.h"
#include "esp_spi_flash.h"
#include "rom/spi_flash.h"
#include "rom/ets_sys.h"
#include "nvs_flash.h"
#include "soc/dport_reg.h"
#include "esp_log.h"

#include "py/mpstate.h"
#include "py/mpconfig.h"
#include "py/runtime.h"
#include "mptask.h"
#include "machpin.h"
#include "pins.h"
#include "mperror.h"
#include "machtimer.h"
#include "esp32chipinfo.h"


TaskHandle_t mpTaskHandle;
TaskHandle_t svTaskHandle;
TaskHandle_t SmartConfTaskHandle;
#if defined(LOPY) || defined (LOPY4) || defined (FIPY)
TaskHandle_t xLoRaTaskHndl;
DRAM_ATTR TaskHandle_t xLoRaTimerTaskHndl;
#endif
#if defined(SIPY) || defined (LOPY4) || defined (FIPY)
TaskHandle_t xSigfoxTaskHndl;
#endif
#if defined(GPY) || defined (FIPY)
TaskHandle_t xLTETaskHndl;
TaskHandle_t xLTEUartEvtTaskHndl;
TaskHandle_t xLTEUpgradeTaskHndl;
#endif
TaskHandle_t xSocketOpsTaskHndl;

extern void machine_init0(void);

/******************************************************************************
 DECLARE PUBLIC DATA
 ******************************************************************************/
StackType_t *mpTaskStack;

// board configuration options from mpconfigboard.h
uint32_t micropy_hw_flash_size;
// uint32_t micropy_hw_spiram_size;

uint32_t micropy_hw_antenna_diversity_pin_num;

uint32_t micropy_lpwan_reset_pin_num;
uint32_t micropy_lpwan_reset_pin_index;
void * micropy_lpwan_reset_pin;
bool micropy_lpwan_use_reset_pin;

uint32_t micropy_lpwan_dio_pin_num;
uint32_t micropy_lpwan_dio_pin_index;
void * micropy_lpwan_dio_pin;

uint32_t micropy_lpwan_ncs_pin_num;
uint32_t micropy_lpwan_ncs_pin_index;
void * micropy_lpwan_ncs_pin;

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static StaticTask_t mpTaskTCB;

/******************************************************************************
 * FunctionName : app_main
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void app_main(void) {

    esp32_init_chip_info();

    // remove all the logs from the IDF
    esp_log_level_set("*", ESP_LOG_NONE);

    // setup the timer used as a reference in mphal
    machtimer_preinit();

    // this one gets the remaining sleep time
    machine_init0();

    // initalize the non-volatile flash space
    esp_err_t ret = nvs_flash_init();
    if ((ret == ESP_ERR_NVS_NO_FREE_PAGES) || (ret == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        nvs_flash_erase();
        nvs_flash_init();
    }
#ifndef RGB_LED_DISABLE
    // initialise heartbeat on Core 0
    mperror_pre_init();
#endif

    // printf("Flash size reported by esp_idf: 0x%x\n", spi_flash_get_chip_size());

    micropy_hw_flash_size = esp32_get_flash_size();
    // propagating the Flash Size in the global variable (used in multiple IDF modules)
    // after that spi_flash_get_chip_size() will report what was set here.
    g_rom_flashchip.chip_size = micropy_hw_flash_size;

    // printf("Flash size set: 0x%x\n", micropy_hw_flash_size);
    // printf("SPI RAM size : 0x%x\n", esp32_get_spiram_size());
    // printf("Reported Chip revision : %d\n", esp32_get_chip_rev());

#ifndef ESP32_GENERIC
    if (esp32_get_chip_rev() > 0) {
        micropy_hw_antenna_diversity_pin_num = MICROPY_SECOND_GEN_ANT_SELECT_PIN_NUM;

        micropy_lpwan_ncs_pin_index = 1;
        micropy_lpwan_ncs_pin_num = 18;
        micropy_lpwan_ncs_pin = &pin_GPIO18;

        micropy_lpwan_use_reset_pin = false;

        micropy_lpwan_dio_pin_index = 2;
        micropy_lpwan_dio_pin_num = 23;
        micropy_lpwan_dio_pin = &pin_GPIO23;
    } else {
        micropy_hw_antenna_diversity_pin_num = MICROPY_FIRST_GEN_ANT_SELECT_PIN_NUM;

        micropy_lpwan_ncs_pin_index = 0;
        micropy_lpwan_ncs_pin_num = 17;
        micropy_lpwan_ncs_pin = &pin_GPIO17;

        micropy_lpwan_reset_pin_index = 1;
        micropy_lpwan_reset_pin_num = 18;
        micropy_lpwan_reset_pin = &pin_GPIO18;
        micropy_lpwan_use_reset_pin = true;

        micropy_lpwan_dio_pin_index = 2;
        micropy_lpwan_dio_pin_num = 23;
        micropy_lpwan_dio_pin = &pin_GPIO23;
    }
#endif

    if (esp32_get_spiram_size() >= 4194304) {
        mpTaskStack = malloc(MICROPY_TASK_STACK_SIZE_PSRAM);

        // create the MicroPython task
        mpTaskHandle =
        (TaskHandle_t)xTaskCreateStaticPinnedToCore(TASK_Micropython, "MicroPy", (MICROPY_TASK_STACK_SIZE_PSRAM / sizeof(StackType_t)), NULL,
                                                    MICROPY_TASK_PRIORITY, mpTaskStack, &mpTaskTCB, 1);
    } else {
        mpTaskStack = heap_caps_malloc(MICROPY_TASK_STACK_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

        // create the MicroPython task
        mpTaskHandle =
        (TaskHandle_t)xTaskCreateStaticPinnedToCore(TASK_Micropython, "MicroPy", (MICROPY_TASK_STACK_SIZE / sizeof(StackType_t)), NULL,
                                                    MICROPY_TASK_PRIORITY, mpTaskStack, &mpTaskTCB, 1);
    }
}
