# Zombie Shooter Vita — plan de rendimiento

Actualizado: 2026-09-25.

Este documento define el orden recomendado para subir FPS sin mezclar demasiadas variables ni perder la capacidad de atribuir cada mejora/regresión a una causa concreta.

## Estado de partida

- Vita real alcanza el tutorial, el touch responde y el audio se reproduce correctamente.
- El usuario confirmó que el parche de `IBufferQueue_Clear()` superó el bloqueo anterior: puede caminar por el tutorial, escuchar sonidos, salir al menú principal y volver a intentar entrar.
- Cadencia observada: aproximadamente **1–7 FPS** en la build de diagnóstico.
- `eglSwapBuffers` ronda ~0,2 ms, por lo que el tiempo no está concentrado en el present.
- Existe un problema separado pendiente de análisis: tras volver a entrar al tutorial desde el menú, faltan algunas imágenes/texturas y después ocurre un crash. El usuario conserva el log y `.psp2dmp`; no se debe mezclar ese bug con los cambios de rendimiento hasta revisar esos archivos.

## Regla principal

Optimizar en pruebas A/B controladas, una variable importante por vez.

No habilitar grupos de speedhacks simultáneamente. Conservar siempre una build anterior conocida para comparar.

## Fase 0 — audio: COMPLETADA para el bloqueo principal

El dump previo situó la espera en:

```text
sound::SfxBuffer::play
→ IBufferQueue_Clear
→ cond wait
```

La variante local de OpenSL ES limita esa espera a 100 ms y el usuario confirmó en Vita real que los sonidos funcionan y que `footsteps.wav` ya no detiene indefinidamente el juego.

Esto no significa que el backend de audio esté completamente optimizado: si aparecen muchos timeouts de 100 ms, aún pueden afectar FPS. Por tanto, conservar la métrica de timeouts cuando se perfile una build Debug, pero el bloqueo ya no debe impedir comenzar la optimización de frame rate.

## Fase 1 — baseline de rendimiento / coste del diagnóstico

Medir siempre la misma zona del tutorial durante al menos 30 segundos.

Registrar cuando la build lo permita:

- FPS medio y rango.
- `present frames` / `fps_x10`.
- `swap_avg_us` / `swap_max_us`.
- edad máxima entre presents.
- número y tiempo acumulado de log syncs.
- número/tiempo acumulado de timeouts de OpenSL ES.

Comparar:

1. Debug diagnóstico actual.
2. Release normal.
3. Una configuración `ZOMBIE_PERF_BUILD` de bajo overhead.

### Importante: logging

La build de diagnóstico define `DEBUG_SOLOADER` y `SO_UTIL_VERBOSE=1`, y el logger hace `sceClibPrintf` + escritura a archivo para cada línea habilitada. Aunque los sync normales ya se agrupan, eso puede distorsionar bastante un juego que ahora produce mucha instrumentación.

La build `Perf` debe:

- quitar `DEBUG_SOLOADER`/`SO_UTIL_VERBOSE` de las rutas calientes;
- conservar errores/fatales;
- compilar el código del port con optimización alta;
- construir VitaGL sin su capa de comprobaciones de debug;
- servir sólo para medir rendimiento/estabilidad, mientras Debug sigue siendo la build para dumps y trazas detalladas.

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

## Fase 3 — clocks, sin pisar governors externos

No fijar clocks a ciegas dentro del port mientras se use PSVshell u otro governor, porque un valor hardcodeado puede incluso bajar un perfil externo más alto.

Primero registrar al inicio:

- `scePowerGetArmClockFrequency()`
- `scePowerGetBusClockFrequency()`
- `scePowerGetGpuClockFrequency()`
- `scePowerGetGpuXbarClockFrequency()`

Después hacer una prueba A/B con un perfil alto conocido y el mismo recorrido del tutorial.

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

MetalSyntax documentó en otro port que un heap newlib de 256 MiB puede dejar a VitaGL sin el pool esperado. Zombie Shooter sí renderiza, así que no asumir el mismo fallo; medir primero.

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

Si después de logging + MSAA el FPS sigue bajo, instrumentar agregados, NO logs por llamada.

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
```

`NO_DEBUG=1` se puede usar en la build Perf antes de estos speedhacks porque elimina comprobaciones de VitaGL, pero no debe confundirse con un arreglo funcional: si una regresión sólo ocurre sin comprobaciones, volver a Debug para investigarla.

Los propios docs de VitaGL avisan que varios speedhacks pueden causar crashes o glitches. Nunca habilitarlos todos simultáneamente.

## Fase 8 — resolución interna

Sólo si las mediciones prueban un cuello GPU persistente después de quitar MSAA y ajustar clocks.

Una resolución interna menor con upscale puede dar una mejora grande, pero Zombie Shooter ya asume 960×544 en el bridge EGL/NativeWindow; bajar resolución exige revisar viewport, FBOs, touch y query de surface. No es la primera optimización.

## Prioridad recomendada actual

```text
1. Perf build: quitar overhead de tracing
2. MSAA 4x -> NONE
3. Medir clocks / CPU vs GPU
4. Memoria/pools VitaGL
5. Shader cache
6. Profiling dirigido
7. Speedhacks uno por uno
8. Resolución interna si sigue GPU-bound
```

El crash de reentrada al tutorial se mantiene en una pista separada y debe analizarse con el log/dump real antes de tocar recursos de forma especulativa.

## Criterio de éxito intermedio

Antes de perseguir 30/60 FPS, la meta inmediata es:

```text
- primer tutorial estable durante varios minutos
- audio continuo sin congelación
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
