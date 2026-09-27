#!/usr/bin/env python3
"""Verify the functional SDK, apply versioned patches, and reject source drift."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
SDK = Path('/usr/local/vitasdk')

# The baseline lock records the source state after the long-lived VitaGL patch.
# Pass 10 is intentionally kept as a small second patch, so pin its two changed
# final files here rather than silently weakening dependency verification.
PASS10_FINAL_FILES = {
    'lib/vitagl/source/textures.c':
        '4655017c147ed010640481685a86668fbaa72361c97ed0a54410def6b71f9be2',
    'lib/vitagl/source/utils/zombie_texture_update.h':
        'f304fe25a0d5855b120aa21550bc45cc832a6a3201097c43bb9e1b1866aad7be',
}


def digest(path, normalize=False):
    data = path.read_bytes()
    if normalize:
        data = data.replace(b'\r\n', b'\n')
    return hashlib.sha256(data).hexdigest()


def git(module, *args, check=True):
    return subprocess.run(['git', '-C', str(ROOT / 'lib' / module), *args],
                          check=check, capture_output=True, text=True)


def apply_patch(module, patch):
    if not patch.exists():
        return
    args = ['apply', '--ignore-space-change']
    if git(module, *args, '--reverse', '--check', str(patch), check=False).returncode == 0:
        return
    git(module, *args, '--check', str(patch))
    git(module, *args, str(patch))


def pass10_vitagl_is_prepared():
    return all((ROOT / name).is_file() and digest(ROOT / name, True) == sha
               for name, sha in PASS10_FINAL_FILES.items())


def main():
    if os.environ.get('VITASDK') != str(SDK):
        raise RuntimeError('Use VITASDK=/usr/local/vitasdk; HardFP is not supported')
    manifest = json.loads((ROOT / 'scripts/vitasdk-files.sha256.json').read_text())
    bad = [name for name, sha in manifest.items()
           if not (SDK / name).is_file() or digest(SDK / name) != sha]
    if bad:
        raise RuntimeError('SDK differs from the functional baseline: ' + ', '.join(bad[:12]))
    abi = subprocess.check_output([str(SDK / 'bin/arm-vita-eabi-gcc'),
                                   '-Q', '--help=target'], text=True)
    if not any('-mfloat-abi=' in line and line.split()[-1] == 'softfp'
               for line in abi.splitlines()):
        raise RuntimeError('Compiler default must be SoftFP')

    lock = json.loads((ROOT / 'patches/submodules.lock.json').read_text())
    for module, revision in lock['revisions'].items():
        if git(module, 'rev-parse', 'HEAD').stdout.strip() != revision:
            raise RuntimeError(f'{module}: wrong revision; use git submodule update --init --recursive')

    vitagl_ready = pass10_vitagl_is_prepared()
    for module in lock['revisions']:
        # CMake calls this preparer again after install_baseline_sdk.sh. Once the
        # exact combined VitaGL result is present, do not try to reverse-check
        # the older baseline patch against files that intentionally have Pass10
        # edits layered on top.
        if module == 'vitagl' and vitagl_ready:
            continue
        apply_patch(module, ROOT / 'patches' / (module + '.patch'))

    if not vitagl_ready:
        # Pass 10 is intentionally split from the long baseline VitaGL patch.
        # Apply it only after the baseline and require the exact final hashes.
        apply_patch('vitagl', ROOT / 'patches' / 'vitagl_pass10.patch')

    expected_files = dict(lock['files'])
    expected_files.update(PASS10_FINAL_FILES)
    bad = [name for name, sha in expected_files.items()
           if not (ROOT / name).is_file() or digest(ROOT / name, True) != sha]
    if bad:
        raise RuntimeError('Dependency sources differ from the baseline: ' + ', '.join(bad[:12]))
    print('Functional SDK, SoftFP ABI, pinned submodules and exact Pass10 sources verified')


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, OSError, subprocess.CalledProcessError) as exc:
        print(f'ERROR: {exc}', file=sys.stderr)
        if isinstance(exc, subprocess.CalledProcessError):
            print(exc.stderr, file=sys.stderr)
        sys.exit(1)
