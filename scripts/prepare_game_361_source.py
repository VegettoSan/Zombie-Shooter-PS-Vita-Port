#!/usr/bin/env python3
"""Set the Java-facing game identity to the verified Zombie Shooter 3.6.1 build."""
from pathlib import Path

path = Path(__file__).resolve().parent.parent / "source" / "java_base.inc"
text = path.read_text(encoding="utf-8")

old_version = 'return jni->NewStringUTF(&jni, "3.5.3");'
new_version = 'return jni->NewStringUTF(&jni, "3.6.1");'
old_code = 'return 1153;'
new_code = 'return 1161;'
old_sdk_comment = 'const int SDK_INT = 24; // Matches the APK\'s minSdk and native build target.'
new_sdk_comment = 'const int SDK_INT = 24; // Matches the ARMv7 native ELF/API target used by the compatibility layer.'

if new_version not in text:
    if text.count(old_version) != 1:
        raise SystemExit("Unexpected getVersion() source; refusing to patch blindly")
    text = text.replace(old_version, new_version, 1)
if new_code not in text:
    if text.count(old_code) != 1:
        raise SystemExit("Unexpected getVersionCode() source; refusing to patch blindly")
    text = text.replace(old_code, new_code, 1)
if old_sdk_comment in text:
    text = text.replace(old_sdk_comment, new_sdk_comment, 1)

path.write_text(text, encoding="utf-8")
print("Prepared Java identity: Zombie Shooter 3.6.1 / versionCode 1161 / native API 24")
