#!/usr/bin/env python3
"""Route Zombie Shooter's AAsset imports to the direct Vita read backend.

The game sees AAsset as an opaque pointer, so this remains ABI-compatible while
avoiding FalsoNDK's FILE + fseek/ftell/fseek sequence on every asset open.
Fail closed if the dynlib anchors drift.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PATH = ROOT / "source/dynlib.c"
text = PATH.read_text(encoding="utf-8")

include_token = '#include "utils/asset_fast.inc"'
include_anchor = '#include "utils/perf.h"\n'
if include_token not in text:
    if text.count(include_anchor) != 1:
        raise SystemExit("source/dynlib.c: perf include anchor drifted; refusing fast asset patch")
    text = text.replace(
        include_anchor,
        include_anchor + '\n#ifdef NDK_PORT\n#include "utils/asset_fast.inc"\n#endif\n',
        1,
    )

replacements = {
    '{ "AAsset_close", (uintptr_t)&AAsset_close },':
        '{ "AAsset_close", (uintptr_t)&zombie_asset_close },',
    '{ "AAsset_getLength", (uintptr_t)&AAsset_getLength },':
        '{ "AAsset_getLength", (uintptr_t)&zombie_asset_get_length },',
    '{ "AAsset_getRemainingLength", (uintptr_t)&AAsset_getRemainingLength },':
        '{ "AAsset_getRemainingLength", (uintptr_t)&zombie_asset_get_remaining_length },',
    '{ "AAsset_read", (uintptr_t)&AAsset_read_perf },':
        '{ "AAsset_read", (uintptr_t)&zombie_asset_read },',
    '{ "AAsset_seek", (uintptr_t)&AAsset_seek_perf },':
        '{ "AAsset_seek", (uintptr_t)&zombie_asset_seek },',
    '{ "AAsset_openFileDescriptor", (uintptr_t)&AAsset_openFileDescriptor },':
        '{ "AAsset_openFileDescriptor", (uintptr_t)&zombie_asset_open_file_descriptor },',
    '{ "AAssetManager_open", (uintptr_t)&AAssetManager_open_perf },':
        '{ "AAssetManager_open", (uintptr_t)&zombie_asset_open },',
}

for old, new in replacements.items():
    if new in text:
        continue
    if text.count(old) != 1:
        raise SystemExit(f"source/dynlib.c: AAsset import anchor drifted: {old}")
    text = text.replace(old, new, 1)

required = [include_token, *replacements.values()]
missing = [token for token in required if token not in text]
if missing:
    raise SystemExit(f"source/dynlib.c: fast AAsset backend incomplete: {missing}")

PATH.write_text(text, encoding="utf-8")
print("Prepared direct sceIo AAsset backend for startup I/O")
