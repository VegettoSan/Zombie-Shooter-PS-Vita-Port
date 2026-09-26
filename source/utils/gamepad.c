#include "utils/settings.h"
#include <stdint.h>
#include <psp2/ctrl.h>

uint32_t fndk_translate_pad_buttons(uint32_t buttons, uint32_t rear, bool handheld) {
    if (!handheld) return buttons;
    if (!setting_vita_shooter) return buttons | rear;
    uint32_t logical = buttons & ~(SCE_CTRL_L1 | SCE_CTRL_R1);
    if (buttons & SCE_CTRL_L1) logical |= SCE_CTRL_L2;
    if (buttons & SCE_CTRL_R1) logical |= SCE_CTRL_R2;
    if (rear & SCE_CTRL_L2) logical |= SCE_CTRL_L1;
    if (rear & SCE_CTRL_R2) logical |= SCE_CTRL_R1;
    return logical;
}
