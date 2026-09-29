#!/usr/bin/env python3
"""Inventory GLSL candidates embedded in Zombie Shooter 3.6.1 ARMv7.

This is an engineering aid, not a runtime dependency. It scans NUL-terminated
ASCII strings in the exact 3.6.1 libzombie_shooter.so and reports shader
templates/candidates. If one or more Vita logs are supplied, it also marks exact
XXH3 hashes already observed by the runtime shader instrumentation.

XXH3 matching is optional. If libxxhash is not available on the host, the
inventory still works and prints SHA-256 identities; only runtime-log matching
is skipped.
"""
from __future__ import annotations
import argparse
import ctypes
import ctypes.util
import hashlib
import json
import re
from pathlib import Path

EXPECTED_SO_SHA256 = "cb461ac47de79536824e8f7b64fa82293b796bcc159675b4f5ad6304e60bdca1"
RUNTIME_RE = re.compile(
    r"shader_unique hash=([0-9A-Fa-f]{16}) type=0x([0-9A-Fa-f]+) source_length=(\d+)"
)
PLACEHOLDER_RE = re.compile(r"#[A-Za-z0-9_]+#")


def load_xxh3():
    name = ctypes.util.find_library("xxhash")
    if not name:
        return None
    lib = ctypes.CDLL(name)
    fn = lib.XXH3_64bits
    fn.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    fn.restype = ctypes.c_uint64
    return fn


def xxh3(fn, data: bytes) -> str | None:
    if fn is None:
        return None
    buf = ctypes.create_string_buffer(data)
    return f"{fn(buf, len(data)):016X}"


def parse_runtime(log_paths: list[Path]) -> dict[str, dict]:
    observed: dict[str, dict] = {}
    for path in log_paths:
        text = path.read_text(errors="ignore")
        for match in RUNTIME_RE.finditer(text):
            h = match.group(1).upper()
            observed[h] = {
                "type": int(match.group(2), 16),
                "length": int(match.group(3)),
                "log": str(path),
            }
    return observed


def scan_strings(blob: bytes, xxh3_fn) -> list[dict]:
    out = []
    offset = 0
    for part in blob.split(b"\0"):
        if (
            len(part) >= 80
            and b"void main" in part
            and (b"gl_Position" in part or b"gl_FragColor" in part)
        ):
            try:
                src = part.decode("ascii")
            except UnicodeDecodeError:
                offset += len(part) + 1
                continue
            stage = "vertex" if "gl_Position" in src else "fragment"
            canonical = " ".join(src.split()).encode("ascii")
            out.append(
                {
                    "offset": offset,
                    "stage": stage,
                    "length": len(part),
                    "sha256": hashlib.sha256(part).hexdigest(),
                    "xxh3": xxh3(xxh3_fn, part),
                    "canonical_sha256": hashlib.sha256(canonical).hexdigest(),
                    "canonical_xxh3": xxh3(xxh3_fn, canonical),
                    "placeholders": sorted(set(PLACEHOLDER_RE.findall(src))),
                    "source": src,
                }
            )
        offset += len(part) + 1
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("so", type=Path, help="exact 3.6.1 armeabi-v7a libzombie_shooter.so")
    ap.add_argument("--log", action="append", type=Path, default=[], help="Vita log; repeatable")
    ap.add_argument("--json", type=Path, help="optional machine-readable output")
    ap.add_argument("--dump-dir", type=Path, help="optional directory for extracted GLSL candidates")
    args = ap.parse_args()

    data = args.so.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != EXPECTED_SO_SHA256:
        raise SystemExit(
            f"Wrong libzombie_shooter.so: {digest}\nExpected 3.6.1 build 1161: {EXPECTED_SO_SHA256}"
        )

    xxh3_fn = load_xxh3()
    observed = parse_runtime(args.log)
    candidates = scan_strings(data, xxh3_fn)

    for item in candidates:
        h = item["xxh3"]
        item["runtime_exact_observed"] = bool(h and h in observed)
        if h and h in observed:
            item["runtime_log"] = observed[h]["log"]

    vertices = [x for x in candidates if x["stage"] == "vertex"]
    fragments = [x for x in candidates if x["stage"] == "fragment"]
    exact_observed = [x for x in candidates if x["runtime_exact_observed"]]

    print(f"SO SHA256: {digest}")
    print(f"Static GLSL candidates: {len(candidates)} ({len(vertices)} vertex, {len(fragments)} fragment)")
    print(f"Runtime exact matches: {len(exact_observed)}")
    if xxh3_fn is None:
        print("XXH3 unavailable: install libxxhash to correlate directly with Vita shader_unique hashes.")

    for i, item in enumerate(candidates, 1):
        h = item["xxh3"] or item["sha256"][:16].upper()
        status = "OBSERVED" if item["runtime_exact_observed"] else "UNCONFIRMED"
        dynamic = " template" if item["placeholders"] else ""
        print(
            f"{i:02d} {item['stage']:8}{dynamic:9} "
            f"off=0x{item['offset']:06X} len={item['length']:3d} "
            f"id={h} {status}"
        )

    if args.dump_dir:
        args.dump_dir.mkdir(parents=True, exist_ok=True)
        for i, item in enumerate(candidates, 1):
            suffix = "vert" if item["stage"] == "vertex" else "frag"
            h = item["xxh3"] or item["sha256"][:16].upper()
            path = args.dump_dir / f"{i:02d}_{h}_{suffix}.glsl"
            path.write_text(item["source"])

    if args.json:
        payload = {
            "target": "Zombie Shooter 3.6.1 build 1161 armeabi-v7a",
            "so_sha256": digest,
            "xxh3_available": xxh3_fn is not None,
            "logs": [str(x) for x in args.log],
            "summary": {
                "static_total": len(candidates),
                "static_vertex": len(vertices),
                "static_fragment": len(fragments),
                "runtime_exact_matches": len(exact_observed),
            },
            "candidates": candidates,
        }
        args.json.write_text(json.dumps(payload, indent=2))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
