# Zombie Shooter Vita — plan de rendimiento

Actualizado: 2026-09-25.

Este documento define el orden recomendado para subir FPS sin mezclar demasiadas variables ni perder la capacidad de atribuir cada mejora/regresión a una causa concreta.

## Estado de partida

- Vita real alcanza el tutorial y el touch responde.
- Cadencia observada: aproximadamente 1–7 FPS en Debug.
- `eglSwapBuffers` ronda ~0,2 ms, por lo que el tiempo no está concentrado en el present.
- `log_0013` + `.psp2dmp` localizaron una espera en `sound::SfxBuffer::play` → `IBufferQueue_Clear`, con el hilo OpenSL ES dentro de `sceAudioOutOutput`.
- La build actual limita `IBufferQueue_Clear` a 100 ms. Esa modificación aún requiere validación física.

## Regla principal

Optimizar en pruebas A/B controladas, una variable importante por vez.

No habilitar grupos de speedhacks simultáneamente. Conservar siempre una build anterior conocida para comparar.

## Fase 0 — cerrar el bloqueo de audio

Antes de tomar decisiones de FPS:

1. Probar el Debug actual en Vita real.
2. Entrar al tutorial y provocar `footsteps.wav`.
3. Confirmar si `[PERF] present` sigue avanzando después de cualquier `[AUDIO] OpenSLES buffer Clear timed out`.
4. Si el timeout aparece repetidamente, medir cuántas llamadas y cuánto tiempo total consume `IBufferQueue_Clear`.

El timeout de 100 ms es un mecanismo de seguridad, no necesariamente la solución final. Si ocurre varias veces por segundo puede reducir por sí solo la cadencia del juego. En ese caso la solución correcta será hacer el clear realmente no bloqueante/asíncrono o corregir por qué el mixer no confirma, conservando la propiedad segura de los buffers.

## Fase 1 — baseline de rendimiento

Medir siempre la misma zona del tutorial durante al menos 30 segundos.

Registrar:

- FPS medio y rango.
- `present frames` / `fps_x10`.
- `swap_avg_us` / `swap_max_us`.
- edad máxima entre presents.
- número y tiempo acumulado de log syncs.
- número/tiempo acumulado de timeouts de OpenSL ES.

Comparar primero Debug vs Release SIN otros cambios funcionales.

### Importante: logging

Actualmente `DEBUG_SOLOADER` y `SO_UTIL_VERBOSE=1` se definen globalmente y el logger hace `sceClibPrintf` + escritura al archivo para cada línea. Aunque los sync normales ya se agrupan, una build de rendimiento debe medir cuánto distorsiona esto.

Crear posteriormente una configuración `Perf` o una opción CMake en la que:

- sólo errores/fatales se escriban inmediatamente;
- `[PERF]`, `[AUDIO]` y checkpoints importantes sigan disponibles;
- trazas detalladas de JNI/assets/threads queden apagadas;
- no se pierda la posibilidad de generar dump si el juego se bloquea.

## Fase 2 — eliminar MSAA 4x innecesario

Estado actual:

```c
vglInitExtended(0, 960, 544, 6 * 1024 * 1024,
                SCE_GXM_MULTISAMPLE_4X);
```

Pero el puente EGL anuncia:

```text
EGL_SAMPLE_BUFFERS = 0
EGL_SAMPLES        = 0
```

La primera optimización gráfica A/B debe ser:

```c
SCE_GXM_MULTISAMPLE_NONE
```

Razones:

- elimina un coste de multisampling que el juego ni siquiera cree tener;
- reduce ancho de banda y trabajo de fragmentos;
- es mucho más controlable que activar varios speedhacks.

Probar la misma zona del tutorial con MSAA 4x y NONE. Conservar NONE si mejora FPS y no introduce defectos relevantes.

## Fase 3 — clocks, sin pisar PSVshell

No fijar clocks a ciegas dentro del port mientras se usa PSVshell u otro governor, porque un valor hardcodeado puede incluso bajar un perfil externo más alto.

Primero registrar al inicio:

- `scePowerGetArmClockFrequency()`
- `scePowerGetBusClockFrequency()`
- `scePowerGetGpuClockFrequency()`
- `scePowerGetGpuXbarClockFrequency()`

Después hacer una prueba A/B con un perfil alto conocido en PSVshell y el mismo recorrido del tutorial.

Interpretación aproximada:

- gran mejora al subir CPU, poca al subir GPU → cuello CPU/engine/wrappers;
- gran mejora al subir GPU → cuello gráfico;
- casi ninguna mejora → esperas/I/O/audio/sincronización probablemente dominan.

## Fase 4 — memoria VitaGL / heap

El loader reserva actualmente:

```c
int _newlib_heap_size_user = 256 * 1024 * 1024;
```

Y VitaGL se inicia con un `ram_threshold` de 6 MiB. Antes de cambiar tamaños, registrar:

- memoria libre antes de VitaGL;
- `vglMemTotal(VGL_MEM_RAM)` / `vglMemFree(VGL_MEM_RAM)`;
- VRAM total/libre;
- avisos de circular pool/allocations que caigan en VRAM;
- fallos o ciclos de garbage collection.

MetalSyntax ya documentó en otro port que un heap newlib de 256 MiB puede dejar a VitaGL sin el pool esperado. Zombie Shooter sí renderiza, así que no asumir el mismo fallo; medir primero.

Sólo después considerar:

- reducir heap newlib;
- `vglInitWithCustomSizes`;
- ajustar pools.

## Fase 5 — shader cache

El proyecto no activa actualmente el cache automático de shaders de VitaGL.

Probar más adelante:

```text
HAVE_SHADER_CACHE=1
```

con una ruta específica bajo:

```text
ux0:data/zombieshooter/shader_cache/
```

Objetivo: reducir stutters de compilación/recompilación. No esperar que esto por sí solo arregle un FPS sostenido de 1–7.

## Fase 6 — profiling dirigido del frame

Si después de audio + logging + MSAA el FPS sigue bajo, instrumentar agregados, NO logs por llamada.

Medir por ventanas de varios segundos:

- `IBufferQueue_Clear`: count / total_us / max_us.
- OpenSL enqueue/callbacks relevantes.
- `AAsset_open/read/seek`: count / bytes / total_us / max_us.
- `glTexImage2D` / `glTexSubImage2D`.
- `glBufferData` / `glBufferSubData`.
- `glCompileShader` / `glLinkProgram`.
- draw count por frame.
- waits/polls/condvars que superen un umbral alto.

VitaGL también ofrece `HAVE_PROFILING=1` para tiempo CPU dentro de draw calls; usarlo sólo en una build diagnóstica si aporta datos accionables.

## Fase 7 — speedhacks VitaGL, uno por vez

Sólo después de tener baseline estable.

Candidatos razonables para A/B:

```text
HAVE_VERTEX_LAYOUT_CACHE=1
DRAW_SPEEDHACK=2
BUFFERS_SPEEDHACK=1
SAMPLERS_SPEEDHACK=1
CIRCULAR_POOL_SPEEDHACK=1
```

Luego, con más cautela:

```text
DRAW_SPEEDHACK=1
INDICES_DRAW_SPEEDHACK=1
INDICES_SPEEDHACK=1
TEXTURES_SPEEDHACK=1
TEXTURE_UPLOADS_SPEEDHACK=1
MATH_SPEEDHACK=1
PRIMITIVES_SPEEDHACK=1
NO_DEBUG=1
```

Los propios docs de VitaGL avisan que varios de estos flags pueden causar crashes o glitches. Nunca habilitarlos todos simultáneamente.

## Fase 8 — resolución interna

Sólo si las mediciones prueban un cuello GPU persistente después de quitar MSAA y ajustar clocks.

Una resolución interna menor con upscale puede dar una mejora grande, pero Zombie Shooter ya asume 960×544 en el bridge EGL/NativeWindow; bajar resolución exige revisar viewport, FBOs, touch y query de surface. No es la primera optimización.

## Prioridad recomendada

```text
0. Validar fix OpenSL ES
1. Debug vs Release / coste de logging
2. MSAA 4x -> NONE
3. Clocks medidos con PSVshell
4. Memoria/pools VitaGL
5. Shader cache
6. Profiling dirigido
7. Speedhacks uno por uno
8. Resolución interna si sigue GPU-bound
```

## Criterio de éxito intermedio

Antes de perseguir 30/60 FPS, la meta inmediata es:

```text
- tutorial estable durante varios minutos
- sin congelación de audio
- >15 FPS sostenidos como primer salto verificable
- después apuntar a 20/30 FPS con profiling real
```

Cada mejora debe quedar documentada en `port_progress.md` con:

```text
build / cambio
FPS antes
FPS después
misma escena de prueba
regresión visual o funcional
estado: pending hardware / hardware confirmed
```
