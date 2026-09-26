/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include <stdio.h>
#include <string.h>
#include "settings.h"

#define CONFIG_FILE_PATH DATA_PATH"config.txt"

/* Optional startup hook supplied by source/utils/gamepad.c in the real Vita
 * build. Keeping it weak preserves the standalone host settings regression. */
void gamepad_config_load(void) __attribute__((weak));

int setting_software_width;
int setting_music_mode;
int setting_vita_shooter;
int  setting_sampleSetting;
bool setting_sampleSetting2;

static void settings_load_dependent_configs(void) {
    if (gamepad_config_load) gamepad_config_load();
}

void settings_reset() {
    /* 864x489 is now physically validated on real Vita and is the preferred
     * baseline. Keep replacement music opt-in until each backend is physically
     * validated. */
    setting_software_width = 864;
    setting_music_mode = 0;
    setting_vita_shooter = 0; // Legacy fallback only; controls.txt wins on Vita.
    setting_sampleSetting  = 1;
    setting_sampleSetting2 = true;
}

void settings_save() {
    FILE *config = fopen(CONFIG_FILE_PATH, "w+");

    if (config) {
        fprintf(config, "# Zombie Shooter Vita runtime configuration\n");
        fprintf(config, "# software_width modes:\n");
        fprintf(config, "#   0   = original Android engine policy (~1024x580 on Vita, no scale hook)\n");
        fprintf(config, "#   960 = cap work surface to ~960x544\n");
        fprintf(config, "#   864 = cap work surface to ~864x489 (real-Vita validated, recommended)\n");
        fprintf(config, "# music_mode modes:\n");
        fprintf(config, "#   0 = no replacement music backend; stable/silent fallback\n");
        fprintf(config, "#   1 = LEGACY PCM16 WAV hook; confirmed OpenSL crash, diagnostic only\n");
        fprintf(config, "#   2 = compressed OGG/Vorbis Vita mixer; no giant PCM WAV files\n");
        fprintf(config, "# Restart the game after changing these values.\n");
        fprintf(config, "software_width %d\n", setting_software_width);
        fprintf(config, "music_mode %d\n", setting_music_mode);
        fprintf(config, "vita_shooter %d\n", setting_vita_shooter);
        fprintf(config, "%s %d\n", "setting_sampleSetting", (int)(setting_sampleSetting));
        fprintf(config, "%s %d\n", "setting_sampleSetting2", (int)(setting_sampleSetting2));
        fclose(config);
    }
}

void settings_load() {
    settings_reset();

    char buffer[30];
    int value;
    char line[128];

    FILE *config = fopen(CONFIG_FILE_PATH, "r");

    if (!config) {
        /* DATA_PATH already exists when the canonical SO is loaded. Create a
         * documented config once; never overwrite it on later boots. */
        settings_save();
        settings_load_dependent_configs();
        return;
    }

    while (fgets(line, sizeof(line), config)) {
        if (sscanf(line, "%29s %d", buffer, &value) != 2) continue;
        if (strcmp("software_width", buffer) == 0) {
            setting_software_width = value==0 || value==864 || value==960 ? value : 864;
            continue;
        }
        if (strcmp("music_mode", buffer) == 0) {
            setting_music_mode = value>=0 && value<=2 ? value : 0;
            continue;
        }
        if (strcmp("vita_shooter", buffer) == 0) {
            setting_vita_shooter = value == 1;
            continue;
        }
        if      (strcmp("setting_sampleSetting", buffer) == 0)  setting_sampleSetting  = (int)value;
        else if (strcmp("setting_sampleSetting2", buffer) == 0) setting_sampleSetting2 = (bool)value;
    }
    fclose(config);
    settings_load_dependent_configs();
}
