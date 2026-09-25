# Canonical Zombie Shooter Android SO reference

`demo/libzombie_shooter.so` is the persistent reference copy of the Android ARMv7 game library used by this PS Vita port.

## Preservation rule

**Do not delete, rename, move, replace, strip, patch in-place, or regenerate this file as part of cleanup/refactoring.**

Treat it as read-only evidence for reverse engineering and crash analysis. Runtime patches must be implemented by the port/loader, not by modifying this canonical binary.

Current repository object:

```text
path: demo/libzombie_shooter.so
size: 9,773,412 bytes
Git blob SHA: 2ffcb0fc66aa29c5e695efd0618f21f2d74c8420
```

The real-Vita logs for the current game version report loading a `libzombie_shooter.so` of the same 9,773,412-byte size at:

```text
0x98000000
```

Use this repository copy whenever resolving native crash addresses or offsets. For example, the 2026-09-25 new-zone crash currently under investigation resolves to:

```text
PC = 0x98515AE4 -> SO + 0x00515AE4
LR = 0x98515A5F -> SO + 0x00515A5F
```

Before applying an offset-specific patch, verify that the analyzed binary is this canonical reference (or explicitly document a new version). Never reuse these offsets blindly with another game version.
