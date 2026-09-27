#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>
#include <filesystem>
#define ZOMBIE_ASSET_HOST 1
#define ALOGD(...) ((void)0)
#define ALOGW(...) ((void)0)
#define ALOGE(...) ((void)0)
typedef int SceKernelLwMutexWork;
struct SceIoStat { int dummy; };
static int sceKernelCreateLwMutex(void *,const char *,int,int,void *) { return 0; }
static int sceIoGetstat(const char *,void *) { return 0; }
#include "../lib/falso_ndk/android/AAssetManager.cpp"
static void create(const char *name,size_t n) {
    char path[512];snprintf(path,sizeof(path),DATA_PATH "assets/%s",name);
    FILE *f=fopen(path,"wb");assert(f);for(size_t i=0;i<n;i++) fputc((int)(i%251),f);assert(!fclose(f));
}
int main() {
    assert(!AAssetManager_open(nullptr,nullptr,1));
    std::filesystem::create_directory(DATA_PATH "assets/sub");create("sub/nested.vid",100);
    asset_cache_configure(8192);
    AAsset *slash=AAssetManager_open(nullptr,"sub/nested.vid",1),*backslash=AAssetManager_open(nullptr,"sub\\nested.vid",1);
    assert(slash && backslash && ((aAsset *)slash)->buffer==((aAsset *)backslash)->buffer);AAsset_close(slash);AAsset_close(backslash);
    asset_cache_configure(0);
    create("test.vid",4096);create("second.vid",4096);create("big.bin",300000);
    create("empty.vid",0);asset_cache_configure(8192);
    AAsset *a=AAssetManager_open(nullptr,"test.vid",1),*b=AAssetManager_open(nullptr,"test.vid",1);
    assert(a && b);assert(((aAsset *)a)->buffer==((aAsset *)b)->buffer);assert(((aAsset *)a)->shared);
    unsigned char buf[5000];assert(AAsset_read(a,buf,10)==10 && buf[9]==9);
    assert(AAsset_getRemainingLength(b)==4096);assert(AAsset_seek(a,-1,SEEK_END)==4095);
    assert(AAsset_read(a,buf,50)==1 && buf[0]==4095%251);assert(AAsset_read(a,buf,1)==0);
    assert(AAsset_seek(a,1,SEEK_CUR)==-1);assert(AAsset_seek(b,5,SEEK_SET)==5);
    AAsset_close(a);assert(AAsset_read(b,buf,1)==1 && buf[0]==5);
    asset_cache_configure(1); // pinned storage remains live, cannot evict
    assert(AAsset_getLength(b)==4096);AAsset_close(b);
    AssetCacheStats s;asset_cache_snapshot(&s);assert(s.bytes==0 && s.pinned==0);
    asset_cache_configure(4096);
    a=AAssetManager_open(nullptr,"test.vid",1);b=AAssetManager_open(nullptr,"second.vid",1);
    assert(a && b && ((aAsset *)a)->shared && !((aAsset *)b)->shared); // safe private fallback
    AAsset_close(a);AAsset_close(b);
    b=AAssetManager_open(nullptr,"second.vid",1);assert(b && ((aAsset *)b)->shared);AAsset_close(b);
    assert(!AAssetManager_open(nullptr,"missing.vid",1));
    a=AAssetManager_open(nullptr,"big.bin",1);assert(a && !((aAsset *)a)->buffer);
    assert(AAsset_seek(a,299999,SEEK_SET)==299999);assert(AAsset_read(a,buf,2)==1);AAsset_close(a);
    a=AAssetManager_open(nullptr,"empty.vid",1);assert(a && AAsset_getLength(a)==0);assert(!AAsset_read(a,buf,1));AAsset_close(a);
    asset_cache_configure(8192);
    std::vector<std::thread> threads;
    for(int i=0;i<8;i++) threads.emplace_back([] {for(int j=0;j<500;j++) {
        AAsset *h=AAssetManager_open(nullptr,"test.vid",1);assert(h);unsigned char p[3];
        assert(AAsset_seek(h,71,SEEK_SET)==71 && AAsset_read(h,p,3)==3 && p[0]==71);AAsset_close(h);
    }});
    for(auto &thread:threads) thread.join();
    asset_cache_snapshot(&s);assert(s.hits>3900 && s.bytes<=8192 && !s.pinned);
    asset_cache_configure(0);asset_cache_snapshot(&s);assert(s.bytes==0);
    puts("Shared cache + real AAsset handles PASS: cursors, seek, EOF, refs, eviction, pinned fallback, FILE fallback, concurrent readers");
}
