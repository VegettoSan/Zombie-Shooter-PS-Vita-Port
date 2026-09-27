#include <assert.h>
#include <stdio.h>
#include "../source/utils/settings.h"
int main(void) {
    settings_load();
    assert(setting_software_width==864 && setting_music_mode==2);
    assert(setting_framebuffer_565==0 && setting_software_frameskip==1);

    assert(setting_asset_cache_mib==8 && setting_audio_frames==1024);

    FILE *f=fopen(DATA_PATH "config.txt","w");assert(f);
    fputs("vita_shooter 1\nsetting_sampleSetting 7\n",f);fclose(f);
    settings_load();
    assert(setting_software_width==864 && setting_music_mode==2 && setting_vita_shooter==1);
    assert(setting_framebuffer_565==0 && setting_software_frameskip==1);

    setting_software_width=960;setting_music_mode=2;settings_save();settings_reset();settings_load();
    assert(setting_software_width==960 && setting_music_mode==2 && setting_vita_shooter==1 && setting_sampleSetting==7);
    assert(setting_framebuffer_565==0 && setting_software_frameskip==1);

    f=fopen(DATA_PATH "config.txt","w");assert(f);
    fputs("software_width 864\nmusic_mode 1\nvita_shooter 1\nframebuffer_565 1\nsoftware_frameskip 0\n",f);fclose(f);
    settings_load();
    /* Legacy Pass10 value 1 must migrate to stable RGBA; explicit frameskip 0 stays off. */
    assert(setting_software_width==864 && setting_music_mode==1 && setting_vita_shooter==1);
    assert(setting_framebuffer_565==0 && setting_software_frameskip==0);

    f=fopen(DATA_PATH "config.txt","w");assert(f);
    fputs("software_width 864\nmusic_mode 0\nframebuffer_565 2\nsoftware_frameskip 1\n",f);fclose(f);
    settings_load();
    assert(setting_framebuffer_565==2 && setting_software_frameskip==1);

    f=fopen(DATA_PATH "config.txt","w");assert(f);
    fputs("software_width 123\nmusic_mode 9\nvita_shooter 1\nframebuffer_565 99\n",f);fclose(f);
    settings_load();
    assert(setting_software_width==864 && setting_music_mode==0 && setting_vita_shooter==1);
    assert(setting_framebuffer_565==0 && setting_software_frameskip==1);

    f=fopen(DATA_PATH "config.txt","w");assert(f);
    fputs("software_width 0\nmusic_mode 2\n",f);fclose(f);settings_load();
    assert(setting_software_width==0 && setting_music_mode==2);
    assert(setting_framebuffer_565==0 && setting_software_frameskip==1);

    const int cache_values[]={0,8,16,99};const int audio_values[]={128,1024,2048,99};
    for(int i=0;i<4;i++) {
        f=fopen(DATA_PATH "config.txt","w");assert(f);
        fprintf(f,"asset_cache_mib %d\naudio_frames %d\nmusic_mode 0\n",cache_values[i],audio_values[i]);fclose(f);
        settings_load();assert(setting_asset_cache_mib==(i==3?8:cache_values[i]));
        assert(setting_audio_frames==(i==3?1024:audio_values[i]) && setting_music_mode==0);
    }
    puts("Settings regression PASS: 864 default, Pass10 framebuffer_565=1 migration to RGBA, explicit diagnostic=2, render reuse default/on/off, music and input preservation");
}
