# Zombie Shooter Vita — plan de rendimiento

Actualizado: 2026-09-25.

Este documento define el orden vigente para subir FPS sin mezclar variables ni perder la capacidad de atribuir cada mejora o regresión a una causa concreta.

El historial cronológico de pruebas reales está en `docs/HARDWARE_TEST_LOG.md`.

## Estado actual confirmado

- Vita real alcanza el tutorial, touch funciona y el audio se reproduce.
- El fix de `IBufferQueue_Clear()` ya superó el bloqueo de audio principal en hardware.
- El usuario puede caminar por el tutorial y volver al menú principal.
- Existe un bug separado de segunda entrada al tutorial: faltan recursos y después ocurre un crash; está pendiente de análisis con log + `.psp2dmp` reales.
- `eglSwapBuffers` se había medido alrededor de `0,2 ms`; por sí solo no explica frames de cientos de milisegundos.
- La build Release con **MSAA NONE** siguió aproximadamente a **7 FPS al iniciar**, **4 FPS en `LOADING`** y `LOADING` tardó **casi 5 minutos**.
- Por tanto, quitar MSAA 4× **no produjo una mejora significativa** y no debe seguir tratándose como la hipótesis principal.
- Después de esa prueba se descubrió que Release todavía mantenía alto tráfico de `sceClibPrintf()` desde FalsoNDK, FalsoJNI y `SO_UTIL_VERBOSE`; una Release realmente silenciosa está preparada y pendiente de prueba física.

## Regla principal

Optimizar mediante pruebas A/B controladas:

```text
una variable importante
→ build
→ misma escena / mismo procedimiento
→ Vita real
→ registrar resultado
→ decidir el siguiente cambio
```

No habilitar grupos de speedhacks simultáneamente. Conservar siempre una build anterior conocida para comparar.

## Baseline de comparación actual

Build probada:

```text
Release
960×544
MSAA NONE
Pre-release: vita-test-4-121929e
```

Resultado:

```text
inicio  ≈ 7 FPS
LOADING ≈ 4 FPS
carga   ≈ 5 min
```

Este baseline es más útil que el antiguo rango genérico 1–7 FPS para las próximas pruebas de carga.

## Fase 0 — audio: COMPLETADA para el bloqueo principal

El dump previo situó la espera en:

```text
sound::SfxBuffer::play
→ IBufferQueue_Clear
→ cond wait
```

La variante local de OpenSL ES limita esa espera a 100 ms y el usuario confirmó en Vita real que los sonidos funcionan y `footsteps.wav` ya no detiene indefinidamente el juego.

No revertir este fix. Si aparecen muchos timeouts de 100 ms, medir count/total/max como una posible fuente de latencia residual.

## Fase 1 — Release realmente silenciosa: PRUEBA ACTUAL

### Hallazgo

Reducir `l_info/l_debug` del loader no bastaba. Había tres rutas adicionales:

1. FalsoNDK: `ALOGD/ALOGW/ALOGE` terminaban en `sceClibPrintf()` mediante su `fndk_log()` débil.
2. FalsoJNI: Release podía seguir emitiendo warnings.
3. `SO_UTIL_VERBOSE=1` estaba definido globalmente.

Durante `LOADING`, especialmente con tracing de assets, ese tráfico puede ser muy costoso.

### Estado de código preparado

Release ahora:

```text
logger del port: sólo [PERF] + error/fatal necesarios
FalsoNDK: fatal únicamente
FalsoJNI: FALSOJNI_DEBUG_NO
so_util: sin SO_UTIL_VERBOSE
```

Debug conserva el diagnóstico completo.

### Prueba requerida

Usar sólo Release y registrar:

```text
FPS al iniciar
FPS durante LOADING
tiempo total de LOADING
FPS tutorial 30–60 s
```

Comparar contra:

```text
7 FPS / 4 FPS / ~5 min
```

### Interpretación

- Mejora grande de tiempo de carga/FPS → logging era un cuello importante.
- Mejora pequeña → logging contribuía, pero no dominaba.
- Casi sin cambio → pasar inmediatamente a I/O + waits/CPU; no seguir quitando calidad gráfica a ciegas.

## Fase 2 — MSAA: COMPLETADA COMO A/B

Cambio probado:

```text
SCE_GXM_MULTISAMPLE_4X
→ SCE_GXM_MULTISAMPLE_NONE
```

El bridge EGL anuncia:

```text
EGL_SAMPLE_BUFFERS = 0
EGL_SAMPLES        = 0
```

Resultado en hardware: no hubo salto significativo; la Release quedó alrededor de 7 FPS al inicio y 4 FPS en `LOADING`.

### Decisión

Mantener `SCE_GXM_MULTISAMPLE_NONE` porque evita un coste que el juego no solicita, pero **no atribuirle el problema de rendimiento principal**.

No volver a repetir esta prueba salvo que cambie sustancialmente el renderer.

## Fase 3 — profiling de I/O y esperas

Si la Release silenciosa sigue muy lenta, ésta pasa a ser la prioridad inmediata.

Instrumentar agregados por ventanas de varios segundos, no logs por llamada.

### Assets / filesystem

Medir:

```text
AAssetManager_open: count / total_us / max_us
AAsset_read:        count / bytes / total_us / max_us
AAsset_seek:        count / total_us / max_us
fopen/read/seek equivalentes si el engine usa rutas directas
```

También registrar:

- número de assets buffered vs streamed;
- fallos de apertura;
- tiempo total en `sceIo*`/bridge libc cuando sea posible;
- descriptores/handles si reaparece agotamiento.

La pantalla `LOADING` de casi 5 minutos hace esta fase especialmente prioritaria.

### Esperas / sincronización

Medir sólo waits largos o agregados:

```text
pthread_cond_wait / timedwait
poll / epoll / looper waits
sleep / usleep / nanosleep wrappers
OpenSL Clear timeout count / total_us / max_us
cualquier bounded wait añadido por compatibilidad
```

No asumir que un wait es incorrecto sólo porque aparece; comparar frecuencia y tiempo acumulado.

## Fase 4 — clocks: CPU vs GPU

No fijar clocks dentro del port mientras se use PSVshell u otro governor.

Primero registrar al inicio:

```text
scePowerGetArmClockFrequency()
scePowerGetBusClockFrequency()
scePowerGetGpuClockFrequency()
scePowerGetGpuXbarClockFrequency()
```

Luego hacer una A/B manteniendo todo lo demás igual.

Interpretación:

- mejora grande al subir CPU, poca al subir GPU → CPU/engine/wrappers;
- mejora grande al subir GPU → cuello gráfico;
- casi ninguna mejora → waits/I/O/sincronización probablemente dominan.

No mezclar esta prueba con cambios de código.

## Fase 5 — profiling CPU del frame

Si I/O no explica el bajo FPS durante gameplay, medir trabajo de frame mediante contadores/agregados:

```text
glTexImage2D / glTexSubImage2D
glBufferData / glBufferSubData
glCompileShader / glLinkProgram
draw calls por frame
cambios de estado relevantes
input events procesados
OpenSL callbacks/enqueues
```

VitaGL ofrece `HAVE_PROFILING=1`; usar sólo en Debug/perfilado porque añade overhead.

## Fase 6 — memoria VitaGL / heap

El loader reserva actualmente:

```c
int _newlib_heap_size_user = 256 * 1024 * 1024;
```

Y VitaGL se inicia con `ram_threshold` de 6 MiB.

Antes de cambiar tamaños medir:

- memoria libre antes de VitaGL;
- `vglMemTotal(VGL_MEM_RAM)` / `vglMemFree(VGL_MEM_RAM)`;
- VRAM total/libre;
- circular pool / allocations / GC;
- fallos o stalls de memoria.

MetalSyntax documentó en otro port que un heap newlib de 256 MiB podía dejar a VitaGL sin el pool esperado. Zombie Shooter sí renderiza, así que no copiar esa solución sin evidencia local.

## Fase 7 — shader cache

Más adelante probar:

```text
HAVE_SHADER_CACHE=1
```

con cache bajo:

```text
ux0:data/zombieshooter/shader_cache/
```

Objetivo: reducir stutters de compilación/recompilación. No esperar que por sí solo arregle 4 FPS sostenidos en `LOADING`.

## Fase 8 — speedhacks VitaGL, uno por vez

Sólo después de tener baseline con logging mínimo y evidencia de que el coste gráfico/driver merece esta ruta.

Primer candidato razonable:

```text
HAVE_VERTEX_LAYOUT_CACHE=1
```

Después, cada uno por separado:

```text
DRAW_SPEEDHACK=2
BUFFERS_SPEEDHACK=1
SAMPLERS_SPEEDHACK=1
CIRCULAR_POOL_SPEEDHACK=1
```

Más adelante, con más cautela:

```text
DRAW_SPEEDHACK=1
INDICES_DRAW_SPEEDHACK=1
INDICES_SPEEDHACK=1
TEXTURES_SPEEDHACK=1
TEXTURE_UPLOADS_SPEEDHACK=1
MATH_SPEEDHACK=1
PRIMITIVES_SPEEDHACK=1
```

VitaGL advierte que varios speedhacks pueden causar glitches o crashes. Nunca activarlos todos a la vez.

## Fase 9 — resolución interna

Sólo si las mediciones demuestran cuello GPU persistente.

Bajar resolución afecta potencialmente:

```text
viewport
FBOs
NativeWindow/EGL queries
touch mapping
UI scaling
```

Dado que MSAA NONE no produjo un salto claro, bajar resolución **no es la siguiente prueba lógica**.

## Prioridad vigente

```text
1. Probar Release realmente silenciosa
2. I/O + waits/sleeps/sync profiling
3. A/B de clocks CPU vs GPU
4. Profiling CPU/frame
5. Memoria/pools VitaGL
6. Shader cache
7. Speedhacks uno por uno
8. Resolución interna sólo si se prueba GPU-bound
```

En paralelo, pero separado de estas pruebas:

```text
analizar log + .psp2dmp del crash de segunda entrada al tutorial
```

## Criterio de éxito intermedio

Antes de perseguir 30/60 FPS:

```text
primer tutorial estable varios minutos
audio continuo
LOADING muy por debajo de ~5 min
>15 FPS sostenidos como primer salto real
luego objetivo 20/30 FPS con profiling
```

## Formato obligatorio para cada resultado

Actualizar `docs/HARDWARE_TEST_LOG.md` con:

```text
build / commit / tag
cambio aislado
FPS antes
FPS después
tiempo LOADING antes/después
misma escena de prueba
regresión visual/funcional
estado: pending hardware / real Vita verified / no meaningful improvement / regression
conclusión soportada
siguiente prueba
```
