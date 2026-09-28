#!/usr/bin/env python3
"""Run the 3.6.1 source preparer with the build-48 save migration replaced
by validation of the current direct Registry backend.

The historical preparer still contains an exact-text migration for the old
aggregate SharedPreferences implementation. Build 48 replaced that backend
with direct save/*.dat files, Build 52 made mutations incremental, and the
current Vita path queues physical writes to a dedicated I/O worker so gameplay
never blocks on memory-card metadata. Keep all other exact 3.6.1 migrations
unchanged, but fail closed if these save invariants drift.
"""
from pathlib import Path

ORIGINAL = Path(__file__).with_name("prepare_game_361_source.py")
source = ORIGINAL.read_text(encoding="utf-8")
start_marker = "# Do not rely on the engine eventually calling Editor.commit()/apply()."
end_marker = '\nprint("Prepared Zombie Shooter 3.6.1 build 1161 profile:")'

if source.count(start_marker) != 1 or source.count(end_marker) != 1:
    raise SystemExit("prepare_game_361_source.py: save migration anchors drifted; refusing wrapper patch")
start = source.index(start_marker)
end = source.index(end_marker, start)

validation = r'''# Physical-Vita testing established four save invariants:
#   1. one logical Registry key remains one save/*.dat file;
#   2. apply/commit never rewrite the whole Registry;
#   3. there is no device-wide/per-value sceIoSync on gameplay;
#   4. physical writes/deletes/clears are queued to a dedicated Vita I/O worker.
prefs_jni = (ROOT / "source/preferences_jni.inc").read_text(encoding="utf-8")
prefs_store = (ROOT / "source/preferences_store.inc").read_text(encoding="utf-8")
required_incremental = {
    "source/preferences_jni.inc": [
        "pref_persist_current_key(k)",
        "pref_delete_key_file(k)",
        "pref_clear_files()",
        "[SAVE] apply incremental",
    ],
    "source/preferences_store.inc": [
        "[SAVE] write-one",
        "[SAVE] delete-one",
        "[SAVE] incremental apply",
        "changed=x<0||strcmp(pref_entries[x].value,value)!=0",
        "pref_io_worker_main",
        "pref_io_enqueue_key(PREF_IO_WRITE",
        "pref_io_enqueue_key(PREF_IO_DELETE",
        "pref_io_enqueue_clear()",
        "sceKernelDelayThread(15000)",
        "sceIoGetstat(path,&st)",
        "async Registry I/O worker ready",
    ],
}
for relative, tokens in required_incremental.items():
    text = prefs_jni if relative.endswith("preferences_jni.inc") else prefs_store
    missing = [token for token in tokens if token not in text]
    if missing:
        raise SystemExit(f"{relative}: asynchronous incremental save backend missing {missing}; refusing build")

for forbidden in (
    'sceIoSync("ux0:",0)',
    'for(unsigned i=0;ok&&i<pref_count;i++)ok=pref_write_entry(&pref_entries[i]);',
    'directory commit path=',
):
    if forbidden in prefs_store:
        raise SystemExit(f"source/preferences_store.inc: obsolete blocking/O(n) save path returned: {forbidden}")
'''

patched = source[:start] + validation + source[end:]
patched = patched.replace(
    'print("  - SharedPreferences mutations persisted immediately")',
    'print("  - Registry save/*.dat mutations queued asynchronously on Vita (RAM reads, coalesced I/O)")',
)
namespace = {"__name__": "__main__", "__file__": str(ORIGINAL)}
exec(compile(patched, str(ORIGINAL), "exec"), namespace)
