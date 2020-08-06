/*
 * Copyright (c) 2020, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#define MICROPY_HW_BOARD_NAME                                   "Heltec Wireless Stick"
#define MICROPY_PY_SYS_PLATFORM                                 "Heltec-WS"
#define MICROPY_HW_HB_PIN_NUM                                   (0)
#define MICROPY_HW_SAFE_PIN_NUM                                 (21)

#define DEFAULT_AP_SSID                                         "heltec-ws-wlan"

#define MICROPY_LPWAN_DIO_PIN

extern uint32_t micropy_hw_flash_size;

extern uint32_t micropy_hw_antenna_diversity_pin_num;

extern bool micropy_lpwan_use_reset_pin;
extern uint32_t micropy_lpwan_reset_pin_num;
extern uint32_t micropy_lpwan_reset_pin_index;
extern void * micropy_lpwan_reset_pin;

extern uint32_t micropy_lpwan_dio_pin_num;
extern uint32_t micropy_lpwan_dio_pin_index;
extern void * micropy_lpwan_dio_pin;

extern uint32_t micropy_lpwan_dio1_pin_num;
extern uint32_t micropy_lpwan_dio1_pin_index;
extern void * micropy_lpwan_dio1_pin;

extern uint32_t micropy_lpwan_dio2_pin_num;
extern uint32_t micropy_lpwan_dio2_pin_index;
extern void * micropy_lpwan_dio2_pin;

extern uint32_t micropy_lpwan_ncs_pin_num;
extern uint32_t micropy_lpwan_ncs_pin_index;
extern void * micropy_lpwan_ncs_pin;
