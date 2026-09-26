# Hardware test — stale eboot detected — 2026-09-25

Estado: **TEST INVALIDATED FOR RUN #9 / STALE EBOOT CONFIRMED BY LOG**

## Intención de la prueba

El usuario pretendía probar la Release del run #9:

```text
tag: vita-test-9-2ca93af
commit: 2ca93af12b396ab1d2addcb3cbb4af48665c9a23
archivo: Zombie-Shooter-Vita-Release.vpk
```

PSVshell fue configurado al máximo:

```text
CPU: 500 MHz
ES4/GPU: 222 MHz
BUS: 222 MHz
XBR: 166 MHz
```

Observación del usuario:

```text
pantalla VitaGL: 60 FPS
tras cerrarse VitaGL: ~1 FPS
logos Sigma Team/Zombie Shooter: sube hasta ~7 FPS
LOADING: cae progresivamente hasta ~2–4 FPS
LOADING dura ~6 minutos
tutorial: ~3–5 FPS
```

PSVshell durante el tutorial:

```text
MEM:  360 / 365 MB
VMEM: 112 / 112 MB
PHY:   26 / 26 MB
```

Archivo recibido:

```text
log_0001.log
29557 líneas
```

## Evidencia de que NO se ejecutó el eboot del run #9

El run #9 fue diseñado para producir obligatoriamente una cabecera como:

```text
=== BUILD variant=Release id=2ca93af ===
```

y registrar:

```text
[PERF] clocks_preserved ...
[PERF] mem phase=before_vgl ...
[PERF] mem phase=after_vgl ...
[PERF] mem phase=runtime ...
```

Ninguna de esas líneas aparece en el log recibido.

En cambio el log contiene la conducta del eboot anterior:

```text
Clocks set: ARM 444 / BUS 222 / GPU 222 / XBAR 166
```

Además contiene tráfico Debug masivo:

```text
~25840 líneas [debug]
~17122 apariciones FalsoJNI
~11490 apariciones FalsoNDK
```

Por tanto el proceso que realmente arrancó en Vita NO corresponde al eboot de run #9.

## Consecuencia sobre PSVshell

Aunque PSVshell estaba configurado a CPU 500 MHz, el eboot ejecutado volvió a llamar la política antigua y redujo CPU a 444 MHz al iniciar.

Por eso esta prueba NO valida:

- clocks máximos reales,
- Release silenciosa del run #9,
- nueva telemetría de memoria,
- rendimiento del commit 2ca93af.

No usar sus FPS como baseline del run #9.

## Rendimiento observado del eboot antiguo

El log sigue mostrando que `eglSwapBuffers` no es el coste dominante. Ejemplos:

```text
fps_x10=48..50 (~4.8–5.0 FPS)
swap_avg_us ~= 194..202 us
frame_max_us ~= 250000..284000 us
```

Esto conserva la conclusión previa: el frame lento ocurre mayoritariamente antes del swap.

## Acción siguiente segura

Forzar actualización del eboot sin tocar los datos del juego:

1. No borrar `ux0:data/zombieshooter/`.
2. En VitaShell ir a `ux0:app/ZOMB00001/`.
3. Renombrar el eboot actual a `eboot_old.bin` como respaldo temporal.
4. Borrar cualquier VPK Release antiguo de la Vita para evitar confusión por nombres idénticos.
5. Copiar/descargar exactamente `Zombie-Shooter-Vita-Release.vpk` del pre-release `vita-test-9-2ca93af`.
6. Instalarlo.
7. Confirmar que apareció un nuevo `ux0:app/ZOMB00001/eboot.bin`.
8. El eboot del artifact run #9 extraído del VPK tiene tamaño exacto `1,946,084` bytes y SHA-256 `0deeb53acc15c82fb6aa329440cfd60100a42cdb59dfb936e7a51cf9de01ad3d`.
9. El VPK Release del run #9 tiene tamaño `2,027,639` bytes y SHA-256 `42bd2ef6ebf3c9fa348507f01540c37948c60ac7f8fc180fab95d07b4c11aa7e`.
10. Sólo considerar válida la siguiente prueba si el nuevo log contiene `=== BUILD variant=Release id=2ca93af ===`.

## Regla de continuidad

No atribuir esta ejecución a una regresión del run #9. La evidencia del propio log demuestra que el eboot ejecutado era anterior a los cambios de identidad, preservación de clocks y telemetría de memoria.