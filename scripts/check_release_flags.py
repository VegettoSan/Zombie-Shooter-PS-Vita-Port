#!/usr/bin/env python3
"""Reject heavy diagnostics or wrong optimization/ABI in all Release C/C++ flags."""
from pathlib import Path
import sys
root=Path(sys.argv[1])
for path in sorted(root.rglob('flags.make')):
    text=path.read_text()
    for required in ('-O3', '-DNDEBUG', '-mfloat-abi=softfp', '-DFALSOJNI_DEBUGLEVEL=3', '-DZOMBIE_RELEASE_BUILD=1'):
        assert required in text, (str(path),required)
    for forbidden in ('-DDEBUG_SOLOADER','-DSO_UTIL_VERBOSE','-DZOMBIE_THREAD_TRACE','-DZOMBIE_STALL_DUMP','-O0','-fsanitize'):
        assert forbidden not in text, (str(path),forbidden)
    print('Release flags verified:',path)
