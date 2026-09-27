# Performance Pass 9 — gamma palette transform

Fecha: 2026-09-26 (America/Bogota)

## Punto de partida físico

El Release de Pass 8 (`1aebd54123905d59edcd5d799615d0724c7e71e0`) fue ejecutado en una PS Vita real con `software_width=864` y `music_mode=0`. La superficie interna quedó confirmada por runtime en 864x489, con salida física 960x544.

El nuevo desglose de `VID_SOFTWARE` cambió el diagnóstico. En gameplay pesado `VID_SOFTWARE::preparePalette` aparece repetidamente en aproximadamente 4.8–9.4 ms por frame, mientras `VID_SOFTWARE::draw_impl` queda alrededor de 1.5–3.5 ms por frame. En las mismas ventanas `glTexSubImage2D` todavía ronda aproximadamente 5.6–7.1 ms por frame. Los tiempos GRAPH/MAP/software son inclusivos y no deben sumarse entre sí.

Ejemplos del log físico:

- ~12.5 FPS: software ~35.95 ms/frame, Draw ~9.50, preparePalette ~6.22, draw_impl ~3.43, TexSub ~6.04.
- ~15.2 FPS: software ~23.41 ms/frame, preparePalette ~4.76, draw_impl ~1.51, TexSub ~6.88.
- ~12.4 FPS: software ~30.26 ms/frame, preparePalette ~8.70, draw_impl ~1.94, TexSub ~7.15.

Las ocho rutinas `VID_SURFACE::pallete_*` aceleradas en Pass 6 prácticamente no participaron en este recorrido. Por tanto, continuar optimizando esas filas o entrar directamente en los loops de `draw_impl` no estaba respaldado por el perfil de Pass 8.

## Causa localizada dentro de preparePalette

Se desensambló únicamente la SO Android canónica, sin modificarla. El call graph relevante es:

`VID_SOFTWARE::preparePalette` -> lógica Gamma/Army/UniqueGamma -> `VID::SetGammaToPalette` -> 256 x `Color::Color(Gamma const&, Color const&, int)`.

Símbolos/offsets relevantes del binario canónico:

- `VID_SOFTWARE::preparePalette`: `so+0x513488`.
- `VID::SetGammaToPalette(unsigned char*, Gamma const&)`: `so+0x506378`.
- `Color::Color(Gamma const&, Color const&, int)`: `so+0x4160ec`.

`VID::SetGammaToPalette` recorre exactamente 256 entradas de la paleta y construye un `Color` corregido por Gamma para cada una. El formato de `Color` es AARRGGBB.

La representación de `Gamma` usa magnitudes negativas en su primer word de 32 bits y magnitudes positivas en el segundo. La operación ARM original queda reducida exactamente a:

```
outRGB = min(255, (((256 - negativeRGB) * sourceRGB) >> 8) + positiveRGB)
alphaDelta = negativeAlpha != 0 ? -negativeAlpha : positiveAlpha
outAlpha = clamp(sourceAlpha + alphaDelta, 0, 255)
```

Esto explica por qué `preparePalette` era caro aun cuando el raster final medido en `draw_impl` no lo era: cada paleta no-default realizaba 256 llamadas C++ pequeñas antes de dibujar.

## Cambio Pass 9

`source/utils/raster_palette.c` reemplaza únicamente `VID::SetGammaToPalette` en Vita mediante un hook completo y guardado. No se modifica `libzombie_shooter.so`.

Guardas del hook:

- símbolo exacto `_ZN3VID17SetGammaToPaletteEPhRK5Gamma`;
- offset exacto `0x506378`;
- entrada Thumb;
- prólogo exacto de 8 bytes `f0 b5 03 af 4d f8 04 8d`.

Si cualquiera de estas condiciones falla, la aceleración queda desactivada y no se parchea la función.

La nueva implementación:

- conserva el caso Gamma default sin tocar la paleta;
- conserva la transformación AARRGGBB exacta;
- procesa 8 colores simultáneamente con NEON (`vld4_u8`/`vst4_u8`);
- usa aritmética entera de 16 bits para multiplicación, shift, suma y saturación RGB;
- realiza suma signed y clamp exactos para alpha;
- transforma 256 colores en 32 bloques vectoriales en lugar de ejecutar 256 constructores C++;
- no cambia resolución, sprites, blending, Z, efectos ni contenido visual.

También se añadió telemetría `gamma_palette` para confirmar en hardware cuántas transformaciones se ejecutan y cuánto cuestan tras el cambio.

## Validación

Se añadió `tests/run_gamma_palette_regression.py`, con un modelo independiente derivado del ARM original y otro modelo de la nueva expresión. Comprueba casos límite de byte y 250,000 combinaciones pseudoaleatorias de color/Gamma, además de símbolo, offset, prólogo e intrinsics NEON esperados.

El pipeline Pass 9 también conserva:

- SHA-256 de la SO canónica;
- regresiones JNI/gamepad/render-scale/GL/logger/texturas/assets;
- regresiones de trampolines;
- equivalencia ARM de las rutas palette y alpha de pases anteriores;
- builds Debug y Release con el SDK SoftFP reproducible;
- verificación de flags Release y VPK.

## Qué debe demostrar la siguiente Vita real

La compilación no demuestra una mejora de FPS. En el próximo log deben compararse principalmente:

1. `gamma_palette installed=1` y sus contadores.
2. `software_breakdown name=preparePalette` en ms/frame frente al Pass 8.
3. `software_breakdown name=draw_impl` para comprobar que sigue siendo secundario.
4. `gl kind=TexSubImage2D`, que probablemente pase a ser uno de los siguientes objetivos.
5. FPS en la misma clase de escena pesada (zombies/disparos/explosiones), no solo menú.

La expectativa razonable es recuperar varios milisegundos de CPU en frames donde las paletas con Gamma se regeneraban con frecuencia. No se afirma todavía 20–30 FPS sostenidos en gameplay pesado.

## Próximo candidato

Si `preparePalette` cae como se espera y el FPS sigue limitado, el siguiente candidato respaldado por el mismo log es la actualización/upload de textura (`glTexSubImage2D`, aproximadamente 6–7 ms/frame en varias ventanas pesadas), seguido por el tiempo restante de GRAPH fuera de las fases ya desglosadas.
