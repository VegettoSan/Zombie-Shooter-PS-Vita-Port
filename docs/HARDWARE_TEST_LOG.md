# Actualización 2026-09-25: nuevo baseline funcional

## Baseline físico pase 3: log_0007 (2026-09-26)

Build local-8282fe9-b3b1dfa9b9, MSAA NONE y RGB565 optimizado confirmados.
Usuario: logos ~28 FPS; Loading inicial ~3 minutos/2–8 FPS; tutorial10–12,
~9 con muchos objetos; menú12–15; nivel2 carga <=2 s y gameplay8–9.
Carga inicial: tramo 2–24 tiene 4376 opens fallidos/33,47 s y 2346 exitosos/53,70 s.
Gameplay estable de nivel2 casi no lee assets: el disco no explica sus FPS bajos.
Pase 4: índice de ausencia de carpetas read-only y medición del render/clear CPU.
Informe docs/PERFORMANCE_PASS_4_2026-09-26.md. Nuevos FPS/cargas pendientes Vita real.

## Baseline físico del pase 2: log_0006

Release `local-8282fe9-cb0ab2d552` probado en Vita real: logos ~28 FPS,
tutorial 9–11 FPS completado, menú ~11 FPS, nivel 2 ~8 FPS. Loading inicial
largo y ~4 FPS; carga de nivel 2 más corta. Clocks ARM500/BUS222/GPU222/XBAR166.
La mejora del pase 2 queda REAL VITA VERIFIED / GAMEPLAY VERIFIED para ese recorrido.
El log contiene 103 ventanas. La gran subida RGBA 1480x838 cuesta ~19 ms,
frente a ~75–80 ms del pase 1. COW optimizado no copia bytes antiguos en ella.
Sólo el primer reporte registra shaders: 18 enlaces, 5,863 s; posteriores cero.
Las aperturas de assets dominan el coste de I/O durante la carga, incluyendo
muchos intentos fallidos. Pools VitaGL tienen espacio libre, sin fallos COW.
Pase 3: MSAA NONE por indicación del usuario, preservación nativa RGB565 y
medición separada de aperturas exitosas/fallidas con rutas lentas acotadas.
Informe: `docs/PERFORMANCE_PASS_3_2026-09-25.md`. Nueva build pendiente Vita real.

## Nuevo baseline físico: log_0005, Release 8282fe9 (pase 1)

El usuario confirma VitaGL logo 60 FPS, Sigma Team/Zombie Shooter ~11 FPS,
LOADING 2–4 FPS y unos 6 minutos, tutorial 6–7 FPS (explosiones abundantes ~4).
Movimiento, disparos, explosiones y varias zonas del tutorial funcionan sin crash.
Clocks máximos confirmados por el log: ARM500/BUS222/GPU222/XBAR166.
PSVshell: MEM360/365 MiB, VMEM112/112, PHY26/26; no demuestra agotamiento
interno de VitaGL, que reserva pools por adelantado.

75 ventanas PERF: no Finish/Flush, sin timeouts Clear/Destroy. Las últimas 12
promedian 5,92 FPS con 83,47 ms/frame en TexSubImage, el 49,4% de su tiempo.
Swap ~0,2 ms; audio y logger tienen costes mucho menores en esas ventanas.
El siguiente pase optimiza preservación de texturas sin perder copy-on-write y
mide alloc/copia, shapes/callers, memoria interna, asset opens/seeks y shaders.
Informe: `PERFORMANCE_PASS_2_2026-09-25.md`.
Nueva build: pendiente Vita real; no afirmar 20–30 FPS ni carga menor todavía.


Baseline: `bd2dc1a8bd6067d0508381a5f4a8e1d8748b9969`, ya publicado en master.
Fuente: nueva prueba física reportada por el usuario en el documento de este pase.

| Elemento | Resultado real PS Vita |
| --- | --- |
| Release / VitaGL / logos / LOADING | OK |
| Tutorial / movimiento / disparos / explosiones | OK |
| Sonidos / audio | OK |
| Estabilidad inicial durante recorrido prolongado del tutorial | OK |
| JNI signatures/WINDOW_SERVICE, crash DFAR 0x776F | **REAL VITA VERIFIED**: corregido |
| FPS sostenidos observados | aproximadamente 5 FPS |

No implica estabilidad de todas las zonas ni mejoras medidas del pase FPS 1.
La build siguiente conserva audio, timeout de 100 ms, 960x544 y MSAA 4x.
Prueba siguiente: mismo recorrido en Release y devolver un log completo con
header BUILD y varias ventanas `[PERF]`. Ver `PERFORMANCE_PASS_1_2026-09-25.md`.

Los registros siguientes son históricos. MSAA NONE/quiet Release antiguos,
notas sin audio y pendientes del fix JNI no describen el baseline bd2dc1a.
La restricción histórica local-only no aplica a esta iteración: el usuario
pidió explícitamente commit/push y Manual Vita VPK Pre-release.

---

> Archivo histórico conservado de `2809a6951cec83a460be5777f388edf04a76f259`, respaldado en `backup/before-local-restore-20260925`. Sus observaciones de hardware siguen siendo evidencia de esos builds; las descripciones de código, flags, clocks, MSAA y workflow corresponden a esa rama y no implican que estén activas en master restaurado. Estado actual: `PORT_STATUS.md`.

# Zombie Shooter Vita — registro persistente de pruebas en hardware

Actualizado: 2026-09-25.

Este archivo es el historial cronológico de pruebas reales en PS Vita. Su objetivo es impedir que futuras sesiones repitan hipótesis descartadas, reviertan fixes ya confirmados o mezclen varias variables sin saber cuál cambió el resultado.

## Reglas de uso

Para cada cambio relevante registrar:

```text
fecha / build o commit
objetivo
cambio aislado
resultado observado en Vita real
estado: PENDING HARDWARE / REAL VITA VERIFIED / NO MEANINGFUL IMPROVEMENT / REGRESSION
conclusión que sí está soportada
qué NO puede concluirse todavía
siguiente prueba controlada
```

No marcar un fix como confirmado porque compile. `BUILD VERIFIED` y `REAL VITA VERIFIED` son estados distintos.

---

## Baseline funcional previo a rendimiento

### OpenSL ES — bloqueo en `IBufferQueue_Clear`

**Síntoma anterior**

El tutorial podía renderizar, pero el game thread terminaba suspendido durante reproducción de SFX. Un dump situó la espera en:

```text
sound::SfxBuffer::play
→ IBufferQueue_Clear
→ espera de condición
```

**Cambio**

`lib/opensles_clear/IBufferQueue.c` sustituye únicamente `IBufferQueue.o` de la biblioteca OpenSL ES del SDK. `Clear()` mantiene la petición de limpieza pero limita la espera del acknowledgement del mixer a 100 ms, en vez de bloquear indefinidamente.

**Resultado en Vita real**

REAL VITA VERIFIED.

- Los sonidos se reproducen.
- El usuario pudo caminar por el tutorial.
- Pudo salir del tutorial al menú principal.
- Pudo intentar volver a entrar al tutorial.
- El bloqueo indefinido observado anteriormente dejó de impedir avanzar.

**Conclusión**

No revertir este fix durante optimizaciones de FPS. Todavía puede existir coste por timeouts de audio; eso debe medirse por separado y no confundirse con el bloqueo ya resuelto.

---

## Bug separado: segunda entrada al tutorial

**Secuencia reproducible reportada**

```text
primer tutorial
→ salir al menú
→ entrar al tutorial otra vez
→ faltan algunas imágenes/texturas
→ crash
```

**Estado**

PENDING ROOT CAUSE.

El usuario conserva en su PC el `log_*.log` y el `.psp2dmp` de esa ejecución. No se debe atribuir el fallo a assets, memoria, VitaGL, lifecycle o audio hasta analizar esos archivos reales.

**Regla**

No mezclar este crash con pruebas A/B de rendimiento. Para FPS usar por ahora sólo la primera entrada al tutorial.

---

## Rendimiento — baseline inicial

La build diagnóstica alcanzaba aproximadamente:

```text
1–7 FPS durante el tutorial
```

`eglSwapBuffers` se había medido alrededor de `0,2 ms`, por lo que el present por sí solo no explica frames de cientos de milisegundos.

Esto orienta el profiling hacia trabajo anterior al swap: CPU/engine, wrappers Android, logging, I/O, sincronización, cargas de recursos, audio u otras esperas.

---

## Prueba A/B 1 — MSAA 4× → NONE

### Cambio

En `source/utils/glutil.c`:

```text
SCE_GXM_MULTISAMPLE_4X
→ SCE_GXM_MULTISAMPLE_NONE
```

Se mantuvo resolución 960×544. No se activaron speedhacks ni se bajó resolución.

### Build probada

Pre-release manual generado por GitHub Actions:

```text
tag: vita-test-4-121929e
commit: 121929e4789ff834a45ff9a0f66c64a8000bb684
archivo: Zombie-Shooter-Vita-Release.vpk
nota: FPS test - MSAA NONE
```

### Resultado en Vita real — 2026-09-25

```text
inicio aproximado: 7 FPS
pantalla LOADING:   4 FPS
LOADING:            casi 5 minutos
```

El usuario reportó que la carga seguía siendo extremadamente lenta y que no observó un salto de rendimiento claro respecto al baseline.

### Estado

**NO MEANINGFUL IMPROVEMENT observado.**

### Conclusión soportada

MSAA 4× no era el cuello de botella dominante del rendimiento observado. Mantener `SCE_GXM_MULTISAMPLE_NONE` es razonable porque el bridge EGL anuncia `EGL_SAMPLE_BUFFERS=0` / `EGL_SAMPLES=0`, pero no seguir persiguiendo fill-rate o resolución como primera hipótesis basándose en este test.

### No concluir todavía

- No demuestra que la GPU nunca sea limitante durante gameplay.
- No demuestra que VitaGL esté completamente fuera del cuello.
- No usar esta prueba para justificar múltiples speedhacks a la vez.

---

## Hallazgo posterior — Release todavía tenía logging pesado

Después de la prueba MSAA NONE se revisaron las rutas de logging y se detectó que sólo el logger principal del loader estaba reducido en Release.

### Rutas que seguían activas

1. FalsoNDK genera `ALOGD/ALOGW/ALOGE`; el proyecto ya tenía un bridge fuerte `fndk_log()` en `source/main.c` que los redirige al logger del port.
2. El patch de `AAssetManager` contiene trazas de apertura/cierre/progreso de assets; durante `LOADING` pueden ejecutarse muchas veces.
3. FalsoJNI sin override de Release usa por defecto `FALSOJNI_DEBUG_WARN`, y los warnings imprimen por consola.
4. `SO_UTIL_VERBOSE=1` estaba definido globalmente, no sólo en Debug.

Este hallazgo justificó una Release silenciosa para separar el coste de logging del resto de la carga.

---

## Prueba A/B 2 — Release realmente silenciosa

### Cambios preparados

Estado de código actual:

- `source/utils/logger.h`: Release conserva agregados `[PERF]` y errores/fatales del logger del port.
- `source/main.c`: sigue siendo la única implementación válida de `fndk_log()` para FalsoNDK.
  - Debug: `l_debug/l_warn/l_error` conservan el stream de diagnóstico.
  - Release: `l_debug/l_warn` se eliminan por macros; errores/fatales se conservan.
- `source/reimpl/fndk_log.c`: queda intencionalmente sin implementación y documenta que no debe volver a definir `fndk_log()`.
- `CMakeLists.txt`:
  - `SO_UTIL_VERBOSE=1` sólo en Debug.
  - Debug mantiene `FALSOJNI_DEBUGLEVEL=0` y trazas de thread/stall.
  - Release usa `FALSOJNI_DEBUGLEVEL=4` (`FALSOJNI_DEBUG_NO`).
  - Release conserva optimización y no añade una tercera variante de VPK.

Commits relevantes de esta preparación/corrección:

```text
58fe5125026952d1085db7ff188db6f48c73ab77
81638600fab54e0ba5c2a0fd5c824ef5f9c77748
9baebfa6d8b6df4e0666d45e0f85a788b706f006
```

### Build probada

GitHub Actions run #6 completó correctamente Debug + Release y publicó:

```text
tag: vita-test-6-86055f8
commit: 86055f83e8b3877323c2c571a223e3b045d104a0
archivo probado: Zombie-Shooter-Vita-Release.vpk
nota: FPS test - quiet Release retry
```

### Resultado en Vita real — PSVshell al máximo

El usuario hizo la prueba con el perfil de overclock de PSVshell al máximo.

```text
logos Sigma Team / juego: empieza ~1 FPS
luego:                    llega hasta ~7 FPS
LOADING:                  ~2–4 FPS
tiempo en LOADING:        ~6 minutos
resultado final:          crash antes de entrar al tutorial
```

Durante `LOADING`, PSVshell mostraba:

```text
MEM:  365 MB / 365 MB
VMEM: 112 MB / 112 MB
PHY:   26 MB /  26 MB
```

### Estado

**REAL VITA TESTED / NO MEANINGFUL PERFORMANCE IMPROVEMENT / CRASH DURING LOADING.**

### Conclusión soportada

- Quitar el logging de Release no produjo una mejora importante de FPS ni del tiempo de carga.
- Llevar los clocks al máximo tampoco resolvió el problema; por tanto, falta de frecuencia bruta no parece ser la explicación dominante de los 1–7 FPS.
- El crash de esta ejecución ocurrió durante la primera carga, después de unos 6 minutos. No debe confundirse automáticamente con el crash conocido de segunda entrada al tutorial.

### Interpretación de PSVshell verificada contra su código

PSVshell dibuja la cifra izquierda como `total - free` y la derecha como `total`. Por tanto, los valores `365/365`, `112/112` y `26/26` significan que esos espacios estaban reportados como completamente asignados/reservados desde la perspectiva del sistema.

Correspondencia:

```text
MEM  = main/user RAM
VMEM = CDRAM
PHY  = physically contiguous RAM
```

Esto NO demuestra por sí solo que VitaGL se haya quedado internamente sin memoria, porque VitaGL reserva grandes memblocks y luego subasigna dentro de ellos.

### Hallazgo en la configuración actual de VitaGL

El port usa:

```c
vglInitExtended(0, 960, 544, 6 * 1024 * 1024,
                SCE_GXM_MULTISAMPLE_NONE);
```

En la revisión de VitaGL fijada por el proyecto, `vglInitExtended()` termina usando `vglInitWithCustomThreshold()` y reserva aproximadamente:

```text
RAM:     toda la USER RAM libre menos 6 MiB
CDRAM:   toda la CDRAM libre (threshold 0)
PHYCONT: toda la PHYCONT libre (threshold 0)
```

Por eso el 100% de PSVshell es coherente con la política actual de reserva y no es todavía prueba de una fuga.

### Siguiente prueba controlada — telemetría de memoria

Antes de reducir heap/pools se mantiene exactamente la misma política de memoria y se añade únicamente telemetría agregada:

```text
sceKernelGetFreeMemorySize:
  USER / CDRAM / PHYCONT libres del sistema

VitaGL:
  vglMemFree / vglMemTotal para RAM
  vglMemFree / vglMemTotal para VRAM/CDRAM
  vglMemFree / vglMemTotal para PHYCONT
```

Se registra antes de inicializar VitaGL, justo después y cada ~5 s junto a `[PERF]` durante render.

Commit de instrumentación:

```text
168d08834c83aadb849c0bb5bff283e764927357
```

**PENDING BUILD / PENDING HARDWARE.**

No cambiar todavía `_newlib_heap_size_user = 256 MiB`, el threshold de 6 MiB ni los tamaños de pools. Primero distinguir:

```text
A) PSVshell 100%, pero vglMemFree todavía alto
   → reserva anticipada; buscar headroom externo / heap / I/O / waits.

B) PSVshell 100% y vglMemFree cae cerca de cero
   → agotamiento real de pools; después ajustar/rebalancear memoria.
```

---

## GitHub Actions — build manual de prueba

Se creó `.github/workflows/manual-prerelease.yml` como herramienta manual del usuario.

Características:

```text
trigger: workflow_dispatch únicamente
VitaSDK: vitasdk/vitasdk-softfp:nightly
verifica: -mfloat-abi=softfp
compila: Debug + Release
genera: dos VPK
publica: un GitHub Pre-release
```

Archivos esperados del Pre-release:

```text
Zombie-Shooter-Vita-Debug.vpk
Zombie-Shooter-Vita-Release.vpk
build-info.txt
SHA256SUMS.txt
```

El workflow no incluye APK/XAPK, `libzombie_shooter.so` ni assets propietarios.

### Historial de bring-up del workflow

**Run #1**

Falló en validación porque el YAML buscaba `libSceShaccCgExt_stub.a`; la imagen actual usa `libSceShaccCgExt.a`.

**Run #2**

Superó la validación del SDK y falló compilando el FalsoNDK fijado por el proyecto con GCC 15.2.0: `ASensor.cpp` usa `errno`, `EAGAIN` y `EINTR` sin `<cerrno>`.

Se añadió un shim de compatibilidad CI que incluye `<cerrno>` sin actualizar indiscriminadamente el submódulo.

**Run #3**

FalsoNDK compiló. El enlace final falló con:

```text
undefined reference to `_getentropy_r'
```

La imagen SoftFP nightly contiene un newlib que referencia `_getentropy_r` pero no lo resolvía en ese enlace. Se añadió un objeto de compatibilidad sólo para CI usando `sceKernelGetRandomNumber`, equivalente a la implementación Vita de newlib.

**Run #4**

**BUILD VERIFIED / WORKFLOW VERIFIED.**

```text
Debug VPK      SUCCESS
Release VPK    SUCCESS
metadata       SUCCESS
artifact       SUCCESS
Pre-release    SUCCESS
```

Pre-release generado:

```text
vita-test-4-121929e
```

**Run #5 — quiet Release preparation**

Falló en el enlace de Debug con:

```text
multiple definition of `fndk_log'
source/reimpl/fndk_log.c
source/main.c
```

Causa: al intentar silenciar FalsoNDK se añadió una segunda implementación fuerte de `fndk_log()`, sin detectar que `source/main.c` ya tenía el bridge del proyecto.

Corrección: mantener `source/main.c::fndk_log()` como única implementación. `source/reimpl/fndk_log.c` queda sin símbolo y documenta explícitamente que no debe añadir otro bridge.

```text
commit del fix: 9baebfa6d8b6df4e0666d45e0f85a788b706f006
BUILD: corregido y verificado en run #6
HARDWARE: PENDING
```

**Run #6 — quiet Release retry**

**BUILD VERIFIED / WORKFLOW VERIFIED.**

```text
SoftFP/toolchain              SUCCESS
patches FalsoJNI/FalsoNDK     SUCCESS
GCC15 compatibility           SUCCESS
_getentropy_r compatibility   SUCCESS
Debug VPK                     SUCCESS
Release VPK                   SUCCESS
metadata                      SUCCESS
artifact backup               SUCCESS
Pre-release                   SUCCESS
```

Pre-release generado:

```text
vita-test-6-86055f8
commit: 86055f83e8b3877323c2c571a223e3b045d104a0
```

No eliminar los shims de GCC15/getentropy sin volver a comprobar que la imagen nightly ya resolvió ambos problemas.

---

## Reglas que futuras sesiones deben respetar

- Audio `IBufferQueue_Clear`: fix confirmado en hardware; no revertir por limpieza/refactor.
- MSAA NONE: ya probado; no esperar que por sí solo arregle los 1–7 FPS.
- Quiet Release: probada con PSVshell al máximo; no produjo mejora significativa y terminó en crash durante primera carga tras ~6 min.
- PSVshell 100% MEM/VMEM/PHY no equivale automáticamente a `vglMemFree()==0`; medir los pools internos antes de cambiar tamaños.
- No reducir `_newlib_heap_size_user`, thresholds o pools de VitaGL sin leer la telemetría de memoria de la siguiente prueba.
- Segunda entrada al tutorial: bug real, causa todavía no demostrada; analizar log/dump antes de tocar recursos por ese bug.
- Debug = diagnóstico completo. Release = gameplay/performance con tracing mínimo.
- `fndk_log()` tiene un único propietario: `source/main.c`. No crear una segunda definición.
- No crear una tercera build `Perf` salvo que exista una necesidad nueva claramente justificada; el flujo actual mantiene sólo Debug y Release.
- Una variable importante por prueba A/B.
- No activar lotes de VitaGL speedhacks sin baseline.
- Codex trabaja sólo en local y no publica a GitHub ni dispara Actions/Releases. El workflow manual es una herramienta del usuario.
