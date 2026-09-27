/*
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE for details.
 */

/**
 * @file  settings.h
 * @brief Loader settings that can be set via a configurator app.
 */

#ifndef SOLOADER_SETTINGS_H
#define SOLOADER_SETTINGS_H

#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int setting_asset_cache_mib;
extern int setting_audio_frames;
extern int setting_software_width; // 0: Android policy; 960: native; 864: reduced
extern int setting_music_mode;     // 0: stable/silent; 1: legacy PCM WAV; 2: compressed original M4A/AAC Vita mixer
extern int setting_log_mode;       // 0: off; 1: errors/fatal; 2: +PERF; 3: verbose Debug diagnostics
extern int setting_framebuffer_565; // 0: original RGBA; 2: corrected RGB565 diagnostic (legacy value 1 is disabled)
extern int setting_software_frameskip; // 0: render every tick; 1: adaptive expensive-frame reuse; 2: legacy forced 2:1
extern int  setting_sampleSetting;
extern bool setting_sampleSetting2;

void settings_load();
void settings_save();
void settings_reset();

#ifdef __cplusplus
};
#endif

#endif // SOLOADER_SETTINGS_H