#include <assert.h>
#include <stdio.h>
#include "../source/utils/settings.h"
int main(void) {
    settings_load();
    assert(setting_software_width==864);
    FILE *f=fopen(DATA_PATH "config.txt","w");assert(f);
    fputs("vita_shooter 1\nsetting_sampleSetting 7\n",f);fclose(f);
    settings_load();assert(setting_software_width==864 && setting_vita_shooter==1);
    setting_software_width=960;settings_save();settings_reset();settings_load();
    assert(setting_software_width==960 && setting_vita_shooter==1 && setting_sampleSetting==7);
    f=fopen(DATA_PATH "config.txt","w");assert(f);
    fputs("software_width 123\nvita_shooter 1\n",f);fclose(f);
    settings_load();assert(setting_software_width==864 && setting_vita_shooter==1);
    f=fopen(DATA_PATH "config.txt","w");assert(f);
    fputs("software_width 0\n",f);fclose(f);settings_load();assert(setting_software_width==0);
    puts("Settings regression PASS: old config, resolution defaults, save/load and input preservation");
}
