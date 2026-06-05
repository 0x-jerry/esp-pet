/*
 * btstack_chipset_zephyr.h - Stub for ESP32 (no Zephyr RTOS)
 *
 * The ESP32 port only needs the declaration of btstack_chipset_zephyr_instance().
 * The actual implementation in chipset/zephyr/ is compiled but never called for
 * Espressif chips (they use VHCI, not an external chipset).
 */
#ifndef BTSTACK_CHIPSET_ZEPHYR_H
#define BTSTACK_CHIPSET_ZEPHYR_H

#if defined __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "btstack_chipset.h"

const btstack_chipset_t * btstack_chipset_zephyr_instance(void);

#if defined __cplusplus
}
#endif

#endif
