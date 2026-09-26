/* MIT. Runtime PS Vita -> Xbox logical control mapping for hardware tests. */
#include "utils/gamepad.h"
#include "utils/settings.h"
#include "utils/logger.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <psp2/ctrl.h>

#ifndef DATA_PATH
#define DATA_PATH ""
#endif

#define CONTROLS_FILE_PATH DATA_PATH "controls.txt"

typedef struct {
    const char *physical_name;
    uint32_t physical_mask;
    bool from_rear;
    uint32_t xbox_mask;
} VitaXboxBinding;

/* Internal masks deliberately reuse SceCtrl bits only as a transport between
 * the port and FalsoNDK. The public configuration contract is Xbox names. */
static VitaXboxBinding bindings[] = {
    {"cross",      SCE_CTRL_CROSS,    false, SCE_CTRL_CROSS},
    {"circle",     SCE_CTRL_CIRCLE,   false, SCE_CTRL_CIRCLE},
    {"square",     SCE_CTRL_SQUARE,   false, SCE_CTRL_SQUARE},
    {"triangle",   SCE_CTRL_TRIANGLE, false, SCE_CTRL_TRIANGLE},
    {"l",          SCE_CTRL_L1,       false, SCE_CTRL_L1},
    {"r",          SCE_CTRL_R1,       false, SCE_CTRL_R1},
    {"rear_left",  SCE_CTRL_L2,       true,  SCE_CTRL_L2},
    {"rear_right", SCE_CTRL_R2,       true,  SCE_CTRL_R2},
    {"start",      SCE_CTRL_START,    false, SCE_CTRL_START},
    {"select",     SCE_CTRL_SELECT,   false, SCE_CTRL_SELECT},
    {"dpad_up",    SCE_CTRL_UP,       false, SCE_CTRL_UP},
    {"dpad_down",  SCE_CTRL_DOWN,     false, SCE_CTRL_DOWN},
    {"dpad_left",  SCE_CTRL_LEFT,     false, SCE_CTRL_LEFT},
    {"dpad_right", SCE_CTRL_RIGHT,    false, SCE_CTRL_RIGHT},
    {"l3",         SCE_CTRL_L3,       false, SCE_CTRL_L3},
    {"r3",         SCE_CTRL_R3,       false, SCE_CTRL_R3},
};

static bool controls_loaded;

static void reset_xbox_defaults(void) {
    bindings[0].xbox_mask  = SCE_CTRL_CROSS;    /* A */
    bindings[1].xbox_mask  = SCE_CTRL_CIRCLE;   /* B */
    bindings[2].xbox_mask  = SCE_CTRL_SQUARE;   /* X */
    bindings[3].xbox_mask  = SCE_CTRL_TRIANGLE; /* Y */
    bindings[4].xbox_mask  = SCE_CTRL_L1;       /* LB */
    bindings[5].xbox_mask  = SCE_CTRL_R1;       /* RB */
    bindings[6].xbox_mask  = SCE_CTRL_L2;       /* LT */
    bindings[7].xbox_mask  = SCE_CTRL_R2;       /* RT */
    bindings[8].xbox_mask  = SCE_CTRL_START;
    bindings[9].xbox_mask  = SCE_CTRL_SELECT;   /* BACK */
    bindings[10].xbox_mask = SCE_CTRL_UP;
    bindings[11].xbox_mask = SCE_CTRL_DOWN;
    bindings[12].xbox_mask = SCE_CTRL_LEFT;
    bindings[13].xbox_mask = SCE_CTRL_RIGHT;
    bindings[14].xbox_mask = SCE_CTRL_L3;       /* LS */
    bindings[15].xbox_mask = SCE_CTRL_R3;       /* RS */
}

static void ascii_lower(char *s) {
    for (; *s; ++s)
        if (*s >= 'A' && *s <= 'Z') *s = (char)(*s - 'A' + 'a');
}

static void ascii_upper(char *s) {
    for (; *s; ++s)
        if (*s >= 'a' && *s <= 'z') *s = (char)(*s - 'a' + 'A');
}

static int binding_index(const char *name) {
    for (unsigned i = 0; i < sizeof(bindings) / sizeof(bindings[0]); ++i)
        if (strcmp(bindings[i].physical_name, name) == 0) return (int)i;
    return -1;
}

static uint32_t xbox_mask_from_name(const char *name, bool *valid) {
    *valid = true;
    if (strcmp(name, "A") == 0) return SCE_CTRL_CROSS;
    if (strcmp(name, "B") == 0) return SCE_CTRL_CIRCLE;
    if (strcmp(name, "X") == 0) return SCE_CTRL_SQUARE;
    if (strcmp(name, "Y") == 0) return SCE_CTRL_TRIANGLE;
    if (strcmp(name, "LB") == 0) return SCE_CTRL_L1;
    if (strcmp(name, "RB") == 0) return SCE_CTRL_R1;
    if (strcmp(name, "LT") == 0) return SCE_CTRL_L2;
    if (strcmp(name, "RT") == 0) return SCE_CTRL_R2;
    if (strcmp(name, "LS") == 0) return SCE_CTRL_L3;
    if (strcmp(name, "RS") == 0) return SCE_CTRL_R3;
    if (strcmp(name, "START") == 0) return SCE_CTRL_START;
    if (strcmp(name, "BACK") == 0) return SCE_CTRL_SELECT;
    if (strcmp(name, "DPAD_UP") == 0) return SCE_CTRL_UP;
    if (strcmp(name, "DPAD_DOWN") == 0) return SCE_CTRL_DOWN;
    if (strcmp(name, "DPAD_LEFT") == 0) return SCE_CTRL_LEFT;
    if (strcmp(name, "DPAD_RIGHT") == 0) return SCE_CTRL_RIGHT;
    if (strcmp(name, "NONE") == 0) return 0;
    *valid = false;
    return 0;
}

static const char *xbox_name(uint32_t mask) {
    if (mask == SCE_CTRL_CROSS) return "A";
    if (mask == SCE_CTRL_CIRCLE) return "B";
    if (mask == SCE_CTRL_SQUARE) return "X";
    if (mask == SCE_CTRL_TRIANGLE) return "Y";
    if (mask == SCE_CTRL_L1) return "LB";
    if (mask == SCE_CTRL_R1) return "RB";
    if (mask == SCE_CTRL_L2) return "LT";
    if (mask == SCE_CTRL_R2) return "RT";
    if (mask == SCE_CTRL_L3) return "LS";
    if (mask == SCE_CTRL_R3) return "RS";
    if (mask == SCE_CTRL_START) return "START";
    if (mask == SCE_CTRL_SELECT) return "BACK";
    if (mask == SCE_CTRL_UP) return "DPAD_UP";
    if (mask == SCE_CTRL_DOWN) return "DPAD_DOWN";
    if (mask == SCE_CTRL_LEFT) return "DPAD_LEFT";
    if (mask == SCE_CTRL_RIGHT) return "DPAD_RIGHT";
    return "NONE";
}

static bool write_default_controls(void) {
    FILE *f = fopen(CONTROLS_FILE_PATH, "w");
    if (!f) return false;
    fputs("# Zombie Shooter Vita control mapping\n"
          "# Physical PS Vita input -> Xbox logical control\n"
          "# Values: A B X Y LB RB LT RT LS RS START BACK DPAD_UP DPAD_DOWN DPAD_LEFT DPAD_RIGHT NONE\n"
          "# Restart the game after editing this file.\n\n"
          "cross A\n"
          "circle B\n"
          "square X\n"
          "triangle Y\n\n"
          "l LB\n"
          "r RB\n"
          "rear_left LT\n"
          "rear_right RT\n\n"
          "start START\n"
          "select BACK\n\n"
          "dpad_up DPAD_UP\n"
          "dpad_down DPAD_DOWN\n"
          "dpad_left DPAD_LEFT\n"
          "dpad_right DPAD_RIGHT\n\n"
          "# l3/r3 are accepted if the hardware/controller layer reports them.\n"
          "l3 LS\n"
          "r3 RS\n", f);
    fclose(f);
    return true;
}

static void log_effective_mapping(const char *source) {
    l_perf("[INPUT] xbox_map source=%s emulation=xbox cross=%s circle=%s square=%s triangle=%s l=%s r=%s rear_left=%s rear_right=%s start=%s select=%s dpad_up=%s dpad_down=%s dpad_left=%s dpad_right=%s l3=%s r3=%s legacy_vita_shooter=%d_ignored",
           source,
           xbox_name(bindings[0].xbox_mask), xbox_name(bindings[1].xbox_mask),
           xbox_name(bindings[2].xbox_mask), xbox_name(bindings[3].xbox_mask),
           xbox_name(bindings[4].xbox_mask), xbox_name(bindings[5].xbox_mask),
           xbox_name(bindings[6].xbox_mask), xbox_name(bindings[7].xbox_mask),
           xbox_name(bindings[8].xbox_mask), xbox_name(bindings[9].xbox_mask),
           xbox_name(bindings[10].xbox_mask), xbox_name(bindings[11].xbox_mask),
           xbox_name(bindings[12].xbox_mask), xbox_name(bindings[13].xbox_mask),
           xbox_name(bindings[14].xbox_mask), xbox_name(bindings[15].xbox_mask),
           setting_vita_shooter);
}

void gamepad_config_load(void) {
    reset_xbox_defaults();

    FILE *f = fopen(CONTROLS_FILE_PATH, "r");
    if (!f) {
        bool created = write_default_controls();
        controls_loaded = true;
        log_effective_mapping(created ? "generated_controls.txt" : "defaults_create_failed");
        return;
    }

    char line[160];
    while (fgets(line, sizeof(line), f)) {
        char physical[32], action[32];
        if (sscanf(line, " %31s %31s", physical, action) != 2) continue;
        if (physical[0] == '#') continue;
        ascii_lower(physical);
        ascii_upper(action);

        int index = binding_index(physical);
        if (index < 0) {
            l_perf("[INPUT] controls warning=unknown_physical value=%s", physical);
            continue;
        }

        bool valid;
        uint32_t logical = xbox_mask_from_name(action, &valid);
        if (!valid) {
            l_perf("[INPUT] controls warning=unknown_xbox_control physical=%s value=%s default=%s",
                   physical, action, xbox_name(bindings[index].xbox_mask));
            continue;
        }
        bindings[index].xbox_mask = logical;
    }
    fclose(f);

    controls_loaded = true;
    log_effective_mapping("controls.txt");
}

uint32_t fndk_translate_pad_buttons(uint32_t buttons, uint32_t rear, bool handheld) {
    if (!handheld) return buttons; /* DS3/DS4 keep native FalsoNDK mappings. */

    /* Host regressions and emergency fallback retain the old profile until the
     * startup path explicitly loads controls.txt. Real Vita startup does load it. */
    if (!controls_loaded) {
        if (!setting_vita_shooter) return buttons | rear;
        uint32_t logical = buttons & ~(SCE_CTRL_L1 | SCE_CTRL_R1);
        if (buttons & SCE_CTRL_L1) logical |= SCE_CTRL_L2;
        if (buttons & SCE_CTRL_R1) logical |= SCE_CTRL_R2;
        if (rear & SCE_CTRL_L2) logical |= SCE_CTRL_L1;
        if (rear & SCE_CTRL_R2) logical |= SCE_CTRL_R1;
        return logical;
    }

    uint32_t logical = 0;
    for (unsigned i = 0; i < sizeof(bindings) / sizeof(bindings[0]); ++i) {
        uint32_t state = bindings[i].from_rear ? rear : buttons;
        if (state & bindings[i].physical_mask)
            logical |= bindings[i].xbox_mask;
    }

#ifdef ZOMBIE_DEBUG_BUILD
    static uint32_t previous_buttons = UINT32_MAX;
    static uint32_t previous_rear = UINT32_MAX;
    static uint32_t previous_logical = UINT32_MAX;
    if (buttons != previous_buttons || rear != previous_rear || logical != previous_logical) {
        l_debug("[INPUT] vita_physical buttons=0x%08X rear=0x%08X xbox_mask=0x%08X",
                (unsigned)buttons, (unsigned)rear, (unsigned)logical);
        previous_buttons = buttons;
        previous_rear = rear;
        previous_logical = logical;
    }
#endif

    return logical;
}
