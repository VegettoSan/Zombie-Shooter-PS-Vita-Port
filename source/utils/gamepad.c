/* MIT. Runtime PS Vita -> Xbox logical control mapping for hardware tests. */
#include "utils/gamepad.h"
#include "utils/logger.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <psp2/ctrl.h>
#include <falso_ndk/shim/fndk_controls.h>
#include <falso_ndk/android/keycodes.h>

#ifndef DATA_PATH
#define DATA_PATH ""
#endif

#define CONTROLS_FILE_PATH DATA_PATH "controls.txt"

typedef enum {
    VITA_INPUT_STRICT = 0,
    VITA_INPUT_HYBRID = 1,
    VITA_INPUT_GAMEPLAY = 2,
} VitaInputMode;

typedef struct {
    const char *physical_name;
    uint32_t physical_mask;
    bool from_rear;
    uint32_t xbox_mask;
} VitaXboxBinding;

typedef enum {
    GAMEPLAY_TOUCH_NONE = 0,
    GAMEPLAY_TOUCH_BUY_AMMO,
    GAMEPLAY_TOUCH_PREV_WEAPON,
    GAMEPLAY_TOUCH_NEXT_WEAPON,
    GAMEPLAY_TOUCH_MEDIKIT,
    GAMEPLAY_TOUCH_GRENADE,
} GameplayTouchAction;

typedef struct {
    GameplayTouchAction action;
    const char *name;
    float x;
    float y;
} GameplayTouchTarget;

/*
 * Coordinates come from the Android joystick HUD .men assets. FalsoNDK reports
 * front touch in the same 960x544 coordinate space, so no scaling is needed:
 *   action5                    -> buy ammo      (765, 280)
 *   dpadleft/action7           -> prev weapon   (709, 328)
 *   dpadright/action8          -> next weapon   (815, 328)
 *   action9/dpadup/ltrigger    -> medikit       (924, 316)
 *   action10/dpaddown/rtrigger -> grenade       (743, 180)
 */
static const GameplayTouchTarget gameplay_touch_targets[] = {
    { GAMEPLAY_TOUCH_BUY_AMMO,    "buy_ammo",    765.0f, 280.0f },
    { GAMEPLAY_TOUCH_PREV_WEAPON, "prev_weapon", 709.0f, 328.0f },
    { GAMEPLAY_TOUCH_NEXT_WEAPON, "next_weapon", 815.0f, 328.0f },
    { GAMEPLAY_TOUCH_MEDIKIT,     "medikit",     924.0f, 316.0f },
    { GAMEPLAY_TOUCH_GRENADE,     "grenade",     743.0f, 180.0f },
};

extern AInputQueue *inputQueue;

static GameplayTouchAction gameplay_touch_current = GAMEPLAY_TOUCH_NONE;

/*
 * Strong override of FalsoNDK's weak table.
 *
 * Entries 0..9 are the strict Xbox key path: face buttons, shoulders,
 * Start/Back and stick clicks are Android KeyEvents. D-pad and LT/RT are kept
 * out of the strict prefix because Xbox-style Android controllers expose those
 * through HAT_X/HAT_Y and trigger axes. The final six entries restore the old
 * hybrid aliases when requested, and are always enabled for DS3/DS4 so external
 * controllers keep their previous behaviour.
 */
enum {
    FNDK_STRICT_KEY_COUNT = 10,
    FNDK_HYBRID_KEY_COUNT = 16,
};

ButtonMapping fndk_button_mapping[] = {
    { SCE_CTRL_CROSS,     AKEYCODE_BUTTON_A },
    { SCE_CTRL_CIRCLE,    AKEYCODE_BUTTON_B },
    { SCE_CTRL_SQUARE,    AKEYCODE_BUTTON_X },
    { SCE_CTRL_TRIANGLE,  AKEYCODE_BUTTON_Y },
    { SCE_CTRL_L1,        AKEYCODE_BUTTON_L1 },
    { SCE_CTRL_R1,        AKEYCODE_BUTTON_R1 },
    { SCE_CTRL_START,     AKEYCODE_BUTTON_START },
    { SCE_CTRL_SELECT,    AKEYCODE_BUTTON_SELECT },
    { SCE_CTRL_L3,        AKEYCODE_BUTTON_THUMBL },
    { SCE_CTRL_R3,        AKEYCODE_BUTTON_THUMBR },

    /* Legacy/hybrid aliases; not emitted by handheld strict/gameplay mode. */
    { SCE_CTRL_UP,        AKEYCODE_DPAD_UP },
    { SCE_CTRL_DOWN,      AKEYCODE_DPAD_DOWN },
    { SCE_CTRL_LEFT,      AKEYCODE_DPAD_LEFT },
    { SCE_CTRL_RIGHT,     AKEYCODE_DPAD_RIGHT },
    { SCE_CTRL_L2,        AKEYCODE_BUTTON_L2 },
    { SCE_CTRL_R2,        AKEYCODE_BUTTON_R2 },
};
int fndk_button_mapping_count = FNDK_STRICT_KEY_COUNT;

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
static VitaInputMode input_mode = VITA_INPUT_GAMEPLAY;

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
    input_mode = VITA_INPUT_GAMEPLAY;
    gameplay_touch_current = GAMEPLAY_TOUCH_NONE;
    fndk_button_mapping_count = FNDK_STRICT_KEY_COUNT;
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

static const char *input_mode_name(void) {
    if (input_mode == VITA_INPUT_HYBRID) return "hybrid";
    if (input_mode == VITA_INPUT_GAMEPLAY) return "gameplay";
    return "strict";
}

static bool set_input_mode(const char *name) {
    if (strcmp(name, "GAMEPLAY") == 0 || strcmp(name, "VITA_GAMEPLAY") == 0 ||
        strcmp(name, "TOUCH_GAMEPLAY") == 0) {
        input_mode = VITA_INPUT_GAMEPLAY;
        fndk_button_mapping_count = FNDK_STRICT_KEY_COUNT;
        return true;
    }
    if (strcmp(name, "STRICT") == 0 || strcmp(name, "XBOX_STRICT") == 0) {
        input_mode = VITA_INPUT_STRICT;
        fndk_button_mapping_count = FNDK_STRICT_KEY_COUNT;
        return true;
    }
    if (strcmp(name, "HYBRID") == 0 || strcmp(name, "ANDROID_HYBRID") == 0 ||
        strcmp(name, "LEGACY") == 0) {
        input_mode = VITA_INPUT_HYBRID;
        fndk_button_mapping_count = FNDK_HYBRID_KEY_COUNT;
        return true;
    }
    return false;
}

static bool write_default_controls(void) {
    FILE *f = fopen(CONTROLS_FILE_PATH, "w");
    if (!f) return false;
    fputs("# Zombie Shooter Vita control mapping\n"
          "# Edit this file, save it, then fully restart the game.\n"
          "#\n"
          "# input_mode gameplay (recommended): gameplay buttons press the\n"
          "#   Android HUD touch targets used by Zombie Shooter itself.\n"
          "#   Y=buy ammo, LB/RB=next/previous weapon, LT/RT=medikit/grenade,\n"
          "#   and DPAD duplicates medikit/grenade/weapon switching.\n"
          "# input_mode strict: standard Xbox/Android events only.\n"
          "# input_mode hybrid: strict + legacy DPAD/L2/R2 KeyEvents.\n"
          "input_mode gameplay\n\n"
          "# Physical PS Vita input -> Xbox logical control\n"
          "# Values: A B X Y LB RB LT RT LS RS START BACK\n"
          "#         DPAD_UP DPAD_DOWN DPAD_LEFT DPAD_RIGHT NONE\n"
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
    l_perf("[INPUT] xbox_map source=%s emulation=xbox input_mode=%s strict_key_count=%d active_key_count=%d external_mode=hybrid cross=%s circle=%s square=%s triangle=%s l=%s r=%s rear_left=%s rear_right=%s start=%s select=%s dpad_up=%s dpad_down=%s dpad_left=%s dpad_right=%s l3=%s r3=%s",
           source, input_mode_name(), FNDK_STRICT_KEY_COUNT,
           input_mode == VITA_INPUT_HYBRID ? FNDK_HYBRID_KEY_COUNT : FNDK_STRICT_KEY_COUNT,
           xbox_name(bindings[0].xbox_mask), xbox_name(bindings[1].xbox_mask),
           xbox_name(bindings[2].xbox_mask), xbox_name(bindings[3].xbox_mask),
           xbox_name(bindings[4].xbox_mask), xbox_name(bindings[5].xbox_mask),
           xbox_name(bindings[6].xbox_mask), xbox_name(bindings[7].xbox_mask),
           xbox_name(bindings[8].xbox_mask), xbox_name(bindings[9].xbox_mask),
           xbox_name(bindings[10].xbox_mask), xbox_name(bindings[11].xbox_mask),
           xbox_name(bindings[12].xbox_mask), xbox_name(bindings[13].xbox_mask),
           xbox_name(bindings[14].xbox_mask), xbox_name(bindings[15].xbox_mask));
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

        if (strcmp(physical, "input_mode") == 0 || strcmp(physical, "event_mode") == 0) {
            if (!set_input_mode(action))
                l_perf("[INPUT] controls warning=unknown_input_mode value=%s default=gameplay", action);
            continue;
        }

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

static const GameplayTouchTarget *gameplay_touch_target(GameplayTouchAction action) {
    for (unsigned i = 0; i < sizeof(gameplay_touch_targets) / sizeof(gameplay_touch_targets[0]); ++i)
        if (gameplay_touch_targets[i].action == action) return &gameplay_touch_targets[i];
    return NULL;
}

static void emit_gameplay_touch(GameplayTouchAction action, bool down) {
    const GameplayTouchTarget *target = gameplay_touch_target(action);
    if (!target || !inputQueue) return;

    inputEvent e = {0};
    e.device_id = FNDK_TOUCH_DEVICE_ID;
    e.source = AINPUT_SOURCE_TOUCHSCREEN;
    e.type = AINPUT_EVENT_TYPE_MOTION;
    e.motion_ptrcount = 1;
    e.motion_ptridx[0] = 0;
    e.motion_x[0] = target->x;
    e.motion_y[0] = target->y;
    e.motion_action = down ? AMOTION_EVENT_ACTION_DOWN : AMOTION_EVENT_ACTION_UP;

#ifdef ZOMBIE_DEBUG_BUILD
    l_debug("[INPUT] gameplay_touch action=%s %s x=%d y=%d",
            target->name, down ? "DOWN" : "UP", (int)target->x, (int)target->y);
#endif

    AInputEvent *aie = AInputEvent_create(&e);
    if (aie) AInputQueue_enqueueEvent(inputQueue, aie);
}

static GameplayTouchAction gameplay_action_from_logical(uint32_t logical) {
    /*
     * One virtual HUD finger is enough for controller gameplay and avoids
     * duplicating the game's own touch state. Deterministic priority only
     * matters if contradictory controls are held simultaneously.
     */
    if (logical & (SCE_CTRL_L2 | SCE_CTRL_UP)) return GAMEPLAY_TOUCH_MEDIKIT;
    if (logical & (SCE_CTRL_R2 | SCE_CTRL_DOWN)) return GAMEPLAY_TOUCH_GRENADE;
    if (logical & (SCE_CTRL_R1 | SCE_CTRL_LEFT)) return GAMEPLAY_TOUCH_PREV_WEAPON;
    if (logical & (SCE_CTRL_L1 | SCE_CTRL_RIGHT)) return GAMEPLAY_TOUCH_NEXT_WEAPON;
    if (logical & SCE_CTRL_TRIANGLE) return GAMEPLAY_TOUCH_BUY_AMMO;
    return GAMEPLAY_TOUCH_NONE;
}

static void gameplay_touch_update(uint32_t logical) {
    GameplayTouchAction next = gameplay_action_from_logical(logical);
    if (next == gameplay_touch_current) return;

    if (gameplay_touch_current != GAMEPLAY_TOUCH_NONE)
        emit_gameplay_touch(gameplay_touch_current, false);
    gameplay_touch_current = next;
    if (gameplay_touch_current != GAMEPLAY_TOUCH_NONE)
        emit_gameplay_touch(gameplay_touch_current, true);
}

uint32_t fndk_translate_pad_buttons(uint32_t buttons, uint32_t rear, bool handheld) {
    /* External controllers keep the complete legacy FalsoNDK representation. */
    if (!handheld) {
        if (gameplay_touch_current != GAMEPLAY_TOUCH_NONE) {
            emit_gameplay_touch(gameplay_touch_current, false);
            gameplay_touch_current = GAMEPLAY_TOUCH_NONE;
        }
        fndk_button_mapping_count = FNDK_HYBRID_KEY_COUNT;
        return buttons;
    }

    /* Startup should load controls.txt before the input queue starts. If that
     * ordering ever changes, use the standard handheld mapping. */
    if (!controls_loaded) {
        fndk_button_mapping_count = FNDK_HYBRID_KEY_COUNT;
        return buttons | rear;
    }

    fndk_button_mapping_count = input_mode == VITA_INPUT_HYBRID
        ? FNDK_HYBRID_KEY_COUNT : FNDK_STRICT_KEY_COUNT;

    uint32_t logical = 0;
    for (unsigned i = 0; i < sizeof(bindings) / sizeof(bindings[0]); ++i) {
        uint32_t state = bindings[i].from_rear ? rear : buttons;
        if (state & bindings[i].physical_mask)
            logical |= bindings[i].xbox_mask;
    }

    uint32_t emitted = logical;
    if (input_mode == VITA_INPUT_GAMEPLAY) {
        gameplay_touch_update(logical);

        /*
         * These controls are consumed through the real Android joystick HUD.
         * Suppress their broken key/HAT/trigger representation so one physical
         * press cannot also toggle flashlight, open diagnostics or double-fire.
         * A/B/X, Start/Back and stick clicks remain normal gamepad KeyEvents.
         */
        emitted &= ~(SCE_CTRL_TRIANGLE |
                     SCE_CTRL_L1 | SCE_CTRL_R1 |
                     SCE_CTRL_L2 | SCE_CTRL_R2 |
                     SCE_CTRL_UP | SCE_CTRL_DOWN |
                     SCE_CTRL_LEFT | SCE_CTRL_RIGHT);
    } else if (gameplay_touch_current != GAMEPLAY_TOUCH_NONE) {
        emit_gameplay_touch(gameplay_touch_current, false);
        gameplay_touch_current = GAMEPLAY_TOUCH_NONE;
    }

#ifdef ZOMBIE_DEBUG_BUILD
    static uint32_t previous_buttons = UINT32_MAX;
    static uint32_t previous_rear = UINT32_MAX;
    static uint32_t previous_logical = UINT32_MAX;
    static uint32_t previous_emitted = UINT32_MAX;
    static int previous_key_count = -1;
    if (buttons != previous_buttons || rear != previous_rear || logical != previous_logical ||
        emitted != previous_emitted || fndk_button_mapping_count != previous_key_count) {
        l_debug("[INPUT] vita_physical mode=%s buttons=0x%08X rear=0x%08X xbox_mask=0x%08X emitted_mask=0x%08X key_count=%d",
                input_mode_name(), (unsigned)buttons, (unsigned)rear, (unsigned)logical,
                (unsigned)emitted, fndk_button_mapping_count);
        previous_buttons = buttons;
        previous_rear = rear;
        previous_logical = logical;
        previous_emitted = emitted;
        previous_key_count = fndk_button_mapping_count;
    }
#endif

    return emitted;
}
