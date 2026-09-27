/* MIT. Runtime PS Vita -> Xbox logical control mapping for hardware tests. */
#ifndef ZOMBIE_GAMEPAD_H
#define ZOMBIE_GAMEPAD_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Load/create ux0:data/zombieshooter/controls.txt once during startup.
 * The file controls both Vita->Xbox remapping and strict/hybrid event mode. */
void gamepad_config_load(void);

/* Strong override consumed by patched FalsoNDK. Vita handheld input is mapped
 * through controls.txt. External DS3/DS4 bypass Vita remapping and retain the
 * complete legacy FalsoNDK key+axis representation. */
uint32_t fndk_translate_pad_buttons(uint32_t buttons, uint32_t rear, bool handheld);

#ifdef __cplusplus
}
#endif

#endif
