#include <assert.h>
#include <stdio.h>
#include "../source/utils/settings.h"
int main(void) {
    settings_load();
    assert(setting_software_width==864 && setting_music_mode==0);
    FILE *f=fopen(DATA_PATH "config.txt","w");assert(f);
    fputs("vita_shooter 1\nsetting_sampleSetting 7\n",f);fclose(f);
    settings_load();assert(setting_software_width==864 && setting_music_mode==0 && setting_vita_shooter==1);
    setting_software_width=960;setting_music_mode=2;settings_save();settings_reset();settings_load();
    assert(setting_software_width==960 && setting_music_mode==2 && setting_vita_shooter==1 && setting_sampleSetting==7);
    f=fopen(DATA_PATH "config.txt","w");assert(f);
    fputs("software_width 864\nmusic_mode 1\nvita_shooter 1\n",f);fclose(f);
    settings_load();assert(setting_software_width==864 && setting_music_mode==1 && setting_vita_shooter==1);
    f=fopen(DATA_PATH "config.txt","w");assert(f);
    fputs("software_width 123\nmusic_mode 9\nvita_shooter 1\n",f);fclose(f);
    settings_load();assert(setting_software_width==864 && setting_music_mode==0 && setting_vita_shooter==1);
    f=fopen(DATA_PATH "config.txt","w");assert(f);
    fputs("software_width 0\nmusic_mode 2\n",f);fclose(f);settings_load();
    assert(setting_software_width==0 && setting_music_mode==2);
    puts("Settings regression PASS: 864 validated default, 0/960/864 resolution modes, music 0/1/2, invalid fallback and input preservation");
}
