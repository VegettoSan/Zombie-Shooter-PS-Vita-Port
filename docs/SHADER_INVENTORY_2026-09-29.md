# Zombie Shooter 3.6.1 shader inventory

Date: 2026-09-29  
Target: `libzombie_shooter.so` from Zombie Shooter 3.6.1 build 1161, `armeabi-v7a`  
Required SHA-256: `cb461ac47de79536824e8f7b64fa82293b796bcc159675b4f5ad6304e60bdca1`

## Why this inventory exists

The runtime shader inventory only sees shaders that the game actually submits to
`glShaderSource()`. A campaign run that has not reached every level, enemy,
effect, menu, weapon, or special rendering path cannot prove that every shader
has been seen.

Therefore shader work in this port uses two complementary sources of truth:

1. **Static inventory** from the exact 3.6.1 native library.
2. **Runtime inventory** from real PS Vita logs and Debug GLSL dumps.

A shader should only become a precompiled/runtime replacement after its actual
runtime source and semantics are confirmed. Static discovery alone is not enough.

## Static result from the exact 3.6.1 SO

Scanning NUL-terminated GLSL source strings in the verified ARMv7 library found:

```text
25 total shader/template candidates
8 vertex shader templates
17 fragment shaders
```

Seven of the eight vertex candidates contain engine placeholders such as:

```text
#transX#
#transY#
#mirrored#
```

so their final runtime source/hash can differ from the static template. The
eighth vertex shader is a complete source string.

The static fragment shaders are complete source strings and therefore can be
compared directly against the runtime `shader_unique` hashes.

## Comparison with the current real-Vita runtime capture

The current physical-Vita log recorded:

```text
6 unique vertex runtime variants
16 unique fragment shaders
16 unique program combinations
```

For fragment shaders, **16 of the 17 static fragment shaders match runtime
XXH3 hashes exactly**.

One static fragment shader has not yet appeared in the tested runtime path:

```text
offset: 0x32C8F8
length: 395
XXH3: E541C42D768619AC
stage: fragment
status: STATICALLY PRESENT / RUNTIME UNCONFIRMED
```

Its source is:

```glsl
precision mediump float;
precision lowp int;
varying vec2 v_texCoord;
uniform lowp sampler2D s_texture;
void main()
{
  vec4 texColor = texture2D(s_texture, v_texCoord);
  if (texColor.a == 0.0)
  {
    discard;
  }
  gl_FragColor = texColor;
}
```

This is a texture fragment shader with exact-zero alpha discard. It may belong
to a sprite/effect/menu/level path not yet exercised. Do not delete it, merge it
with an alpha-threshold shader, or assume it is dead code until runtime evidence
proves otherwise.

For vertex shaders the current runtime log has 6 variants while the SO contains
8 static template families. Because most templates are transformed before
`glShaderSource()`, exact hash matching is not sufficient. The Debug build 73
writes the final runtime GLSL to:

```text
ux0:data/zombieshooter/glsl_dump/
```

Those dumps are the authoritative input for mapping runtime vertex variants back
to their static templates and for any MetalSyntax-style offline transpilation.

## Tooling

Use:

```bash
python3 scripts/inventory_shaders_361.py \
  /path/to/libzombie_shooter.so \
  --log /path/to/log_0001.log \
  --json shader_inventory.json \
  --dump-dir extracted_static_glsl
```

The script refuses a library whose SHA-256 is not the exact 3.6.1 build 1161
ARMv7 target.

If host `libxxhash` is available, the script correlates static GLSL with the
runtime XXH3 values emitted by VitaGL instrumentation. Without `libxxhash`, it
still produces SHA-256 identities and extracts the candidate source files.

## MetalSyntax workflow for this port

The safe path remains:

```text
static candidate
+ real Vita glShaderSource dump
        ↓
confirm exact runtime source/stage
        ↓
deduplicate only proven semantic duplicates
        ↓
glslangValidator
        ↓
SPIR-V
        ↓
SPIRV-Cross / HLSL SM3
        ↓
narrow Cg cleanup
        ↓
psp2cgc validation
        ↓
A/B test on physical Vita
        ↓
optional precompiled GXP fast path
        ↓
original VitaGL path remains fallback
```

Do not precompile all 25 static candidates blindly. Some vertex templates are
parameterized, some code paths may be unused, and future levels may create
runtime variants not present in the current capture.

## Current verification state

```text
Static SO scan:                    STATICALLY VERIFIED
17 static fragment candidates:    STATICALLY VERIFIED
16 fragment exact runtime matches: REAL VITA VERIFIED
E541C42D768619AC:                  RUNTIME UNCONFIRMED
8 vertex template candidates:      STATICALLY VERIFIED
6 runtime vertex variants:         REAL VITA VERIFIED
vertex template ↔ runtime mapping: PENDING DEBUG GLSL DUMPS
```

This inventory should be updated as later campaign levels, bosses, effects and
menus expose additional runtime shaders.
