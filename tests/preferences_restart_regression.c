/* Storage contract fixtures, never a real or synthetic campaign save. Each
 * mode runs in a fresh OS process; exact opaque values must survive restart. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#define DATA_PATH ""
#define l_perf(...) ((void)0)
#define l_warn(...) ((void)0)
#define l_error(...) ((void)0)
static int fail_replace,fail_restore;
static int test_rename(const char*a,const char*b) {
 if((fail_replace && !strcmp(a,"shared_preferences.tmp")) ||
    (fail_restore && !strcmp(a,"shared_preferences.bak"))) {errno=EIO;return -1;}
 return rename(a,b);
}
#define PREF_RENAME test_rename
#define PREFS_FILE "shared_preferences.bin"
#define PREFS_TMP "shared_preferences.tmp"
#include "../source/preferences_store.inc"
int main(int argc,char**argv) {
 assert(argc>=3 && chdir(argv[1])==0);
 pref_load();
 if(!strcmp(argv[2],"write")) {
  assert(argc==4 && pref_set("fixture.opaque",argv[3]));assert(pref_save());
 } else if(!strcmp(argv[2],"read")) {
  assert(argc==4);int x=pref_find("fixture.opaque");assert(x>=0);
  assert(!strcmp(pref_entries[x].value,argv[3]));assert(!pref_dirty);
 } else if(!strcmp(argv[2],"fail")) {
  assert(pref_set("fixture.opaque","must-not-replace-valid-store"));
  fail_replace=1;fail_restore=argc==4;assert(!pref_save());assert(pref_dirty);
 } else assert(0);
 pref_free_all();puts("Preferences subprocess contract PASS");
}
