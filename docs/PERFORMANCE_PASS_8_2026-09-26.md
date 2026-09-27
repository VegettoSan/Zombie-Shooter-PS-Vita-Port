# Performance pass 8: descomponer VID_SOFTWARE antes de volver a optimizar

## Objetivo y baseline físico

Este pase no intenta prometer otra mejora de FPS sin evidencia. Su objetivo es
identificar con una sola ejecución en Vita qué parte del rasterizador software
consume la mayor parte de `GRAPH::softwareTact`.

El último perfil físico utilizable sigue siendo el documentado en el pase7:
- ventanas19–48: 1598 frames / 151,673 s = 10,536 FPS;
  `software` 47,7358 ms/frame, `graph` 23,7043 ms/frame,
  `TexSubImage2D` 18,0505 ms/frame y `map` 93,5044 ms/frame;
- ventanas79–96: 776 frames / 91,647 s = 8,467 FPS;
  `software` 59,9194 ms/frame, `graph` 23,3568 ms/frame,
  `TexSubImage2D` 17,6580 ms/frame y `map` 115,785 ms/frame.

`map`, `software` y `graph` son inclusivos/anidados y no se suman. Los pases6/7
optimizaron familias palette/alpha concretas, pero el pase6 no mostró mejora
física perceptible ni cobertura útil de sus filas largas. Por eso este pase evita
seguir eligiendo kernels por nombre y mide la jerarquía real que alimenta
`VID_SOFTWARE::draw_impl`.

Se conserva `software_width=864` como baseline recomendado. Audio, música,
input Xbox, resolución física, objetos, efectos, lógica y VitaGL no se cambian.

## Análisis estático del SO canónico

Se analizó exclusivamente `demo/libzombie_shooter.so`, cuya SHA-256 permanece:

`5e5e2e1bbe86e126c3e2dce5ad97c2e9dc6282305ea7d3e24b49ee4769c5b5b7`

Símbolos/offsets relevantes verificados desde `.dynsym` y desensamblado ARM:

| Función | Offset SO | Tamaño observado |
| --- | ---: | ---: |
| `GRAPH::softwareTact(int)` | `0x410458` | 1148 bytes |
| `SPRITE_COLLECTOR::DrawLayer(...)` | `0x4F4B20` | 668 bytes |
| `VID_SOFTWARE::preparePalette(...)` | `0x513488` | 392 bytes |
| `VID_SOFTWARE::draw_impl(...)` | `0x513610` | 5180 bytes |
| `VID_SOFTWARE::DrawToVid(...)` | `0x514A4C` | 392 bytes |
| `VID_SOFTWARE::Draw(SPRITE const*)` | `0x514BFC` | 320 bytes |

Los cuatro puntos nuevos de instrumentación tienen exactamente el mismo prólogo
PC-independiente de 8 bytes (`f0 b5 03 af 2d e9 00 0f`), correspondiente a
`{0xaf03b5f0, 0x0f00e92d}`. La instalación exige símbolo + offset + prólogo +
espacio de arena RX correctos; ante cualquier mismatch se omite el hook.

### Call graph confirmado

`GRAPH::softwareTact` llama repetidamente a `SPRITE_COLLECTOR::DrawLayer`.
El collector llega a las celdas/sprites y finalmente a los métodos de `VID`.

En el backend software se verificó:

`VID_SOFTWARE::Draw`
→ `reloadIfNeeded`
→ locks/gamma
→ `VID_SOFTWARE::preparePalette`
→ `VID_SOFTWARE::draw_impl`

`VID_SOFTWARE::DrawToVid`
→ `reloadIfNeeded`
→ gamma
→ `VID_SOFTWARE::preparePalette`
→ `VID_SOFTWARE::draw_impl`
→ `cloneMirrorFrom`

El hallazgo estático más importante es que `draw_impl` es una función enorme de
5180 bytes. Dentro de ella el trabajo de rasterización restante está mayormente
inline. Las llamadas externas directas relevantes observadas son las dos rutas
`AsmDrawWithAlpha32` ya estudiadas en pase7 y conversiones puntuales de `Color`.
Esto explica por qué acelerar pequeñas familias auxiliares puede no mover el FPS:
el hotspot principal puede estar dentro de los loops inline de `draw_impl`.

## Instrumentación del pase8

Se añadieron cuatro sondas jerárquicas en `source/utils/raster_alpha.c` para no
introducir otro módulo/hot path paralelo:

- `VID_SOFTWARE::Draw`: muestra 1/32 llamadas;
- `VID_SOFTWARE::DrawToVid`: muestra 1/32 llamadas;
- `VID_SOFTWARE::preparePalette`: muestra 1/64 llamadas;
- `VID_SOFTWARE::draw_impl`: muestra 1/32 llamadas.

Cada llamada no muestreada hace únicamente un incremento atómico relaxed y una
rama. Sólo la llamada seleccionada consulta `sceKernelGetProcessTimeWide()` antes
y después de la función original. No se temporiza por píxel ni por scanline, no
hay logging dentro del draw y no se escriben instrucciones/flush de caché por
frame.

Los tiempos son INCLUSIVOS. El reporte emite por ventana:

`[PERF] software_breakdown name=... installed=... calls=... sampled_calls=... sampled_us=... sampled_avg_us=... estimated_inclusive_us=... max_sample_us_lifetime=... sample_period=... estimator=sampled_x_period`

`estimated_inclusive_us` es deliberadamente una estimación sencilla
`sampled_us * sample_period`; sirve para ordenar hotspots, no para afirmar una
partición exacta del frame. No se deben sumar `Draw`, `DrawToVid`, `preparePalette`
y `draw_impl`, porque se llaman unos dentro de otros.

Al arranque debe aparecer:

`software_breakdown_hooks installed_mask=15 expected_mask=15 ...`

Si la máscara no es15, el log indica que una sonda no fue instalada y no se debe
interpretar su contador como cobertura cero.

## Cómo interpretar el próximo log

La pregunta principal es comparar `draw_impl` contra su padre:

- Si `draw_impl` explica la mayor parte de `Draw`/`DrawToVid`, el pase9 debe
  subdividir o reemplazar únicamente sus loops inline dominantes.
- Si `preparePalette` resulta grande, conviene estudiar paleta/gamma/copia antes
  de tocar los inner loops.
- Si `Draw`/`DrawToVid` son caros pero `draw_impl` no, el coste está en locks,
  reload/setup/clone o alrededor del rasterizador.
- Si toda esta jerarquía explica poco de `softwareTact`, hay otra ruta de
  `VID_SOFTWARE`/collector aún no cubierta y se instrumenta esa ruta en lugar de
  optimizar a ciegas.

Los contadores alpha/palette del pase anterior permanecen visibles y ayudan a
separar cuánto trabajo llega a los kernels ya vectorizados.

## Verificación

### STATICALLY VERIFIED

- SHA-256 del SO canónico correcta.
- Los cuatro símbolos Pass8 existen en `.dynsym` y apuntan a los offsets exactos.
- Los cuatro prólogos de 8 bytes coinciden byte por byte.
- El desensamblado confirma el call graph `Draw/DrawToVid → preparePalette → draw_impl`.
- `draw_impl` mide 5180 bytes y concentra casi todo el rasterizado restante inline.
- La misma rutina `engine_probe_trampoline` ya probada genera los trampolines
  Thumb y rechaza prólogos distintos sin mutar el destino.

### REGRESSION VERIFIED

GitHub Actions ejecutó satisfactoriamente:
- JNI field/signature regression;
- Xbox/gamepad/DisplayMetrics/InputDevice;
- render-scale/settings;
- GL buffers/bindings;
- logger;
- texture update RGBA/RGB565;
- asset index;
- engine-probe emission;
- equivalencia ARM de `raster_palette`;
- equivalencia ARM de `raster_alpha`.

### BUILD VERIFIED

Run `36282800949`, commit `b31b43257d818a64d0f7f85fadfaa9665ffee7e0`:
- SDK funcional exacto `/usr/local/vitasdk`, GCC10.3.0 SoftFP;
- Debug: PASS y VPK válido;
- Release: PASS y VPK válido;
- `scripts/check_release_flags.py`: PASS;
- `git diff --check`: PASS;
- paquete de prueba y ELF matching generados.

Artifact:
`Zombie-Shooter-Pass8-b31b43257d818a64d0f7f85fadfaa9665ffee7e0`

### REAL VITA

**PENDING REAL VITA.** Esta build añade medición, no una optimización nueva del
rasterizado. No se afirma aumento de FPS hasta probar el Release físicamente.

## Próxima prueba física

Cuando haya Vita disponible:
1. usar Release;
2. mantener `software_width 864`;
3. dejar la música fuera de esta comparación (`music_mode 0` está bien);
4. repetir tutorial/nivel2 y una escena con zombies/disparos/explosiones;
5. conservar el log completo con varias ventanas `[PERF]`.

Una sola prueba debería indicar si el pase9 debe entrar directamente en
`draw_impl` o en una fase anterior. No introducir otro speedhack/NEON grande antes
de esa evidencia.
