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

int setting_software_width;
int setting_vita_shooter;
int  setting_sampleSetting;
bool setting_sampleSetting2;

void settings_reset() {
    /* Safe default for run #16 recovery: preserve the engine's original
     * Android-selected work resolution and do not install the scale hook. */
    setting_software_width = 0;
    setting_vita_shooter = 0; // Enable only after measuring the tutorial bindings.
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
        fprintf(config, "#   864 = cap work surface to ~864x489\n");
        fprintf(config, "# Restart the game after changing this value.\n");
        fprintf(config, "software_width %d\n", setting_software_width);
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
        /* DATA_PATH already exists when the canonical SO is loaded.  Create a
         * documented, safe config on first boot so one VPK can exercise all
         * three resolution modes without rebuilding. */
        settings_save();
        return;
    }

    while (fgets(line, sizeof(line), config)) {
        if (sscanf(line, "%29s %d", buffer, &value) != 2) continue;
        if (strcmp("software_width", buffer) == 0) {
            setting_software_width = value==0 || value==864 || value==960 ? value : 0;
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
}
