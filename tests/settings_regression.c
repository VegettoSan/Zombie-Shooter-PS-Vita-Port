#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../source/utils/settings.h"

#ifdef ZOMBIE_RELEASE_BUILD
#define EXPECTED_DEFAULT_LOG_MODE 1
#else
#define EXPECTED_DEFAULT_LOG_MODE 3
#endif

int main(void) {
    settings_load();
    assert(setting_software_width==864 && setting_music_mode==2);
    assert(setting_log_mode==EXPECTED_DEFAULT_LOG_MODE);
    assert(setting_framebuffer_565==0 && setting_software_frameskip==1);
    assert(setting_asset_cache_mib==8 && setting_audio_frames==1024);

    /* Old configs may still contain vita_shooter. It must be harmless and the
     * new logging mode must load without depending on controls.txt. */
    FILE *f=fopen(DATA_PATH "config.txt","w");assert(f);
    fputs("vita_shooter 1\nlog_mode 2\nsetting_sampleSetting 7\n",f);fclose(f);
    settings_load();
    assert(setting_software_width==864 && setting_music_mode==2 && setting_log_mode==2);
    assert(setting_framebuffer_565==0 && setting_software_frameskip==1);

    setting_software_width=960;setting_music_mode=2;settings_save();settings_reset();settings_load();
    assert(setting_software_width==960 && setting_music_mode==2 && setting_log_mode==2 && setting_sampleSetting==7);
    assert(setting_framebuffer_565==0 && setting_software_frameskip==1);

    /* Saved config must no longer advertise the removed vita_shooter switch. */
    f=fopen(DATA_PATH "config.txt","r");assert(f);
    char saved[4096]={0};size_t n=fread(saved,1,sizeof(saved)-1,f);fclose(f);saved[n]=0;
    assert(strstr(saved,"log_mode 2") && !strstr(saved,"vita_shooter"));

    f=fopen(DATA_PATH "config.txt","w");assert(f);
    fputs("software_width 864\nmusic_mode 1\nlog_mode 0\nvita_shooter 1\nframebuffer_565 1\nsoftware_frameskip 0\n",f);fclose(f);
    settings_load();
    /* Legacy Pass10 framebuffer value 1 migrates to RGBA; stale vita_shooter is ignored. */
    assert(setting_software_width==864 && setting_music_mode==1 && setting_log_mode==0);
    assert(setting_framebuffer_565==0 && setting_software_frameskip==0);

    f=fopen(DATA_PATH "config.txt","w");assert(f);
    fputs("software_width 864\nmusic_mode 0\nlog_mode 3\nframebuffer_565 2\nsoftware_frameskip 1\n",f);fclose(f);
    settings_load();
    assert(setting_log_mode==3 && setting_framebuffer_565==2 && setting_software_frameskip==1);

    f=fopen(DATA_PATH "config.txt","w");assert(f);
    fputs("software_width 123\nmusic_mode 9\nlog_mode 99\nvita_shooter 1\nframebuffer_565 99\n",f);fclose(f);
    settings_load();
    assert(setting_software_width==864 && setting_music_mode==0 && setting_log_mode==EXPECTED_DEFAULT_LOG_MODE);
    assert(setting_framebuffer_565==0 && setting_software_frameskip==1);

    f=fopen(DATA_PATH "config.txt","w");assert(f);
    fputs("software_width 0\nmusic_mode 2\nlog_mode 1\n",f);fclose(f);settings_load();
    assert(setting_software_width==0 && setting_music_mode==2 && setting_log_mode==1);
    assert(setting_framebuffer_565==0 && setting_software_frameskip==1);

    const int cache_values[]={0,8,16,99};const int audio_values[]={128,1024,2048,99};
    for(int i=0;i<4;i++) {
        f=fopen(DATA_PATH "config.txt","w");assert(f);
        fprintf(f,"asset_cache_mib %d\naudio_frames %d\nmusic_mode 0\nlog_mode 1\n",cache_values[i],audio_values[i]);fclose(f);
        settings_load();assert(setting_asset_cache_mib==(i==3?8:cache_values[i]));
        assert(setting_audio_frames==(i==3?1024:audio_values[i]) && setting_music_mode==0 && setting_log_mode==1);
    }
    puts("Settings regression PASS: log modes 0-3, stale vita_shooter ignored, 864 default, framebuffer migration, render reuse, music/cache/audio preservation");
}
