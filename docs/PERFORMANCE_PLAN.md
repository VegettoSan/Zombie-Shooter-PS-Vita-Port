> Archivo histórico conservado de `2809a6951cec83a460be5777f388edf04a76f259`, respaldado en `backup/before-local-restore-20260925`. Sus observaciones de hardware siguen siendo evidencia de esos builds; las descripciones de código, flags, clocks, MSAA y workflow corresponden a esa rama y no implican que estén activas en master restaurado. Estado actual: `PORT_STATUS.md`.

# Zombie Shooter Vita — plan de rendimiento

Actualizado: 2026-09-25.

Este documento define el orden vigente para subir FPS sin mezclar variables ni perder la capacidad de atribuir cada mejora o regresión a una causa concreta.

El historial cronológico de pruebas reales está en `docs/HARDWARE_TEST_LOG.md`.

## Regla principal

Trabajar mediante pruebas A/B controladas:

```text
una variable importante
→ build
→ misma escena / mismo procedimiento
→ Vita real
→ registrar resultado
→ decidir el siguiente cambio
```

No activar varios speedhacks a la vez. No cambiar heap, pools, resolución y clocks en una misma build.

## Estado actual confirmado

### Funcionalidad

- Vita real llega a logos, `LOADING`, tutorial y menú en builds anteriores.
- Touch funciona.
- Audio funciona; el fix de `IBufferQueue_Clear()` está REAL VITA VERIFIED y no debe revertirse.
- Existe un crash separado en la segunda entrada al tutorial con texturas ausentes; su causa sigue pendiente de log + `.psp2dmp`.

### Rendimiento

Baseline anterior con MSAA NONE:

```text
inicio  ≈ 7 FPS
LOADING ≈ 4 FPS
carga   ≈ 5 min
```

Prueba de Release silenciosa + PSVshell al máximo:

```text
logos:    ~1 FPS inicialmente, luego hasta ~7 FPS
LOADING:  ~2–4 FPS
carga:    ~6 min
resultado: crash durante primera carga
```

Conclusiones soportadas:

- MSAA 4× → NONE no produjo una mejora significativa.
- Quitar el logging pesado de Release tampoco produjo un salto significativo.
- Llevar los clocks al máximo tampoco solucionó el bajo FPS ni la carga extremadamente larga.
- Por tanto, no priorizar ahora fill-rate, resolución, más overclock ni speedhacks gráficos.

## Hallazgo de memoria — PRIORIDAD ACTUAL

Durante la última prueba, PSVshell mostró:

```text
MEM:  365 / 365 MB
VMEM: 112 / 112 MB
PHY:   26 /  26 MB
```

La semántica de PSVshell se verificó contra su código: la cifra izquierda es `total - free` y la derecha es `total`, por lo que los tres espacios estaban completamente asignados/reservados desde el punto de vista del sistema.

Esto no implica todavía que los pools internos de VitaGL estén agotados.

### Por qué la configuración actual explica el 100% de PSVshell

El port usa:

```c
vglInitExtended(0, 960, 544, 6 * 1024 * 1024,
                SCE_GXM_MULTISAMPLE_NONE);
```

En la revisión de VitaGL fijada por el proyecto, `vglInitExtended()` usa thresholds y termina reservando aproximadamente:

```text
USER RAM: toda la memoria libre menos 6 MiB
CDRAM:    toda la memoria libre disponible
PHYCONT:  toda la memoria libre disponible
```

VitaGL después subasigna desde esos bloques. Por eso PSVshell puede indicar 100% aunque todavía exista espacio libre dentro de VitaGL.

Además el loader mantiene:

```c
int _newlib_heap_size_user = 256 * 1024 * 1024;
```

No modificar aún ninguno de estos valores.

## Fase 1 — telemetría de memoria: PRUEBA ACTUAL

Commit de instrumentación:

```text
168d08834c83aadb849c0bb5bff283e764927357
```

No cambia tamaños ni thresholds. Añade únicamente métricas `[PERF]`.

### Antes de VitaGL

Registrar:

```text
sceKernelGetFreeMemorySize:
USER / CDRAM / PHYCONT libres
```

### Justo después de VitaGL

Registrar:

```text
system free USER / CDRAM / PHYCONT
vglMemFree / vglMemTotal:
  VGL_MEM_RAM
  VGL_MEM_VRAM
  VGL_MEM_PHYCONT
```

### Runtime

La misma fotografía se escribe aproximadamente cada 5 segundos junto al agregado de presents.

Formato esperado:

```text
[PERF] mem phase=before_vgl ...
[PERF] mem phase=after_vgl ...
[PERF] mem phase=runtime ... vgl_free_total_kib ram=A/B vram=C/D phy=E/F
```

### Interpretación

Caso A:

```text
PSVshell 100%
vglMemFree todavía alto/estable
```

Interpretación: gran parte del 100% es reserva anticipada. Investigar después headroom externo, newlib heap, asignaciones fuera de VitaGL, I/O y waits.

Caso B:

```text
PSVshell 100%
vglMemFree cae progresivamente cerca de 0 antes del crash
```

Interpretación: agotamiento real de uno o más pools. La siguiente A/B debe ajustar sólo el reparto/threshold correspondiente.

Caso C:

```text
pool interno conserva espacio
pero system free es ~0 y aparece un fallo de una asignación externa
```

Interpretación: VitaGL está reservando demasiado del espacio que necesita otro subsistema. Primer candidato posterior: aumentar únicamente el threshold de RAM de VitaGL, por ejemplo de 6 MiB a 32 MiB, pero sólo después de esta evidencia.

## Fase 2 — ajuste de memoria, sólo después de telemetría

No ejecutar todavía.

Candidatos, uno por build:

1. Aumentar `ram_threshold` manteniendo CDRAM/PHYCONT iguales.
2. Si el problema es VRAM/CDRAM interno, usar `vglInitWithCustomThreshold` o `vglInitWithCustomSizes` para reservar margen medido.
3. Revisar `_newlib_heap_size_user = 256 MiB` sólo si la evidencia apunta al heap/user RAM.
4. Buscar crecimiento monotónico de texturas/buffers si un pool cae continuamente hasta cero.

Nunca reducir el heap o pools sólo porque PSVshell marque 100%; VitaGL reserva memoria por diseño.

## Fase 3 — I/O y esperas

Si la telemetría demuestra que hay memoria suficiente, pasar a profiling agregado de:

```text
AAssetManager_open: count / total_us / max_us
AAsset_read:        count / bytes / total_us / max_us
AAsset_seek:        count / total_us / max_us
fopen/read/seek equivalentes
```

y:

```text
pthread_cond_wait / timedwait
poll / epoll / looper waits
sleep / usleep / nanosleep
OpenSL Clear timeout count / total_us / max_us
```

No hacer logs por llamada.

## Fase 4 — CPU/frame

Si memoria e I/O no explican el FPS sostenido:

```text
glTexImage2D / glTexSubImage2D
glBufferData / glBufferSubData
glCompileShader / glLinkProgram
draw calls por frame
state changes relevantes
OpenSL callbacks/enqueues
```

`eglSwapBuffers` ya se había medido alrededor de 0,2 ms, por lo que el present por sí solo no explica frames de cientos de milisegundos.

## Fase 5 — shader cache

Sólo después de resolver el cuello dominante de carga/frame:

```text
HAVE_SHADER_CACHE=1
```

con cache bajo:

```text
ux0:data/zombieshooter/shader_cache/
```

Esto puede reducir stutter de compilación, pero no se espera que explique por sí solo 2–4 FPS sostenidos durante `LOADING`.

## Fase 6 — speedhacks VitaGL, uno por vez

No activar todavía.

Primer candidato cuando exista evidencia de coste driver/gráfico:

```text
HAVE_VERTEX_LAYOUT_CACHE=1
```

Después, por separado:

```text
DRAW_SPEEDHACK=2
BUFFERS_SPEEDHACK=1
SAMPLERS_SPEEDHACK=1
CIRCULAR_POOL_SPEEDHACK=1
```

VitaGL advierte que speedhacks pueden introducir glitches/crashes. Nunca habilitarlos como paquete.

## Fase 7 — resolución interna

Última opción si se demuestra un cuello GPU.

Bajar resolución puede afectar viewport, FBOs, NativeWindow/EGL, touch y UI. La prueba de MSAA y la prueba con clocks máximos no justifican todavía esta ruta.

## Prioridad vigente

```text
1. Memory telemetry (sin cambiar tamaños)
2. Ajuste de pools/headroom sólo si la telemetría lo demuestra
3. I/O + waits/sync
4. CPU/frame profiling
5. Shader cache
6. VitaGL speedhacks uno por uno
7. Resolución interna sólo si se prueba GPU-bound
```

En paralelo, pero separado:

```text
analizar log + .psp2dmp del crash conocido de segunda entrada al tutorial
```

## Criterio de éxito intermedio

```text
primera carga estable
sin crecimiento de memoria hasta crash
LOADING muy por debajo de ~5–6 min
>15 FPS sostenidos como primer salto real
luego objetivo 20/30 FPS con profiling
```

Cada prueba real debe actualizar `docs/HARDWARE_TEST_LOG.md` antes de abrir otra hipótesis.
