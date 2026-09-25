# Estado del port de Zombie Shooter para PS Vita

Actualizado: 2026-09-25.

Fuentes de continuidad:

- `PORTING_PLAN.md`: evidencia APK/JADX/SO y arquitectura del juego.
- `port_progress.md`: historial técnico bug-a-bug previo.
- `docs/HARDWARE_TEST_LOG.md`: historial cronológico de pruebas reales y resultados A/B.
- `docs/PERFORMANCE_PLAN.md`: orden vigente de investigación de rendimiento.
- `docs/CRASH_REPORT_2026-09-25_NEW_ZONE.md`: análisis del Data Abort ocurrido al entrar en una zona nueva.
- `docs/RELEASE_LOGGING_DIAGNOSIS_2026-09-25.md`: por qué la Release del run #8 no produjo el contrato de logs esperado y cómo se hace verificable ahora.

## Estado actual resumido

- **Boot/gameplay:** Vita real llega a logos, `LOADING`, tutorial y gameplay. Touch y audio funcionan.
- **Audio:** fix de `IBufferQueue_Clear()` está **REAL VITA VERIFIED**. No revertir.
- **Rendimiento:** tutorial sigue alrededor de ~5 FPS; en el log analizado `eglSwapBuffers` promedia aproximadamente 0,2 ms, por lo que el present no explica el frame lento.
- **MSAA:** `4x → NONE` fue probado y no produjo mejora significativa.
- **Logging Release:** el usuario confirmó que instaló `Zombie-Shooter-Vita-Release.vpk` del run #8. Aun así `log_0003.log` fue Debug-like y no incluyó `[PERF] mem`. No atribuirlo a que el usuario eligió el VPK equivocado. El build contract anterior no era verificable retrospectivamente.
- **Identidad de build:** CMake ahora define explícitamente `ZOMBIE_DEBUG_BUILD` o `ZOMBIE_RELEASE_BUILD`, FalsoJNI usa un nivel target-scoped y cada log nuevo escribe `=== BUILD variant=<...> id=<sha> ===`.
- **CI:** el workflow nuevo inspecciona `flags.make` y falla si Debug/Release no tienen exactamente las definiciones esperadas. El artifact guarda `build-flags.txt`.
- **Clocks:** el port ya NO fuerza `444/222/222/166`. Conserva el perfil elegido por PSVshell y registra `[PERF] clocks_preserved ...`.
- **Memoria:** PSVshell ha mostrado `MEM 365/365`, `VMEM 112/112`, `PHY 26/26`, pero no existe todavía una captura válida de `vglMemFree/vglMemTotal` de una build identificada inequívocamente como Release. No declarar OOM.
- **Crash nueva zona:** reproducido después de cargar el tutorial y jugar. Core: `Data abort exception` dentro de `libzombie_shooter.so`, PC `SO+0x00515AE4`, LR `SO+0x00515A5F`, DFAR inválido `0x07D36E9C`.
- **SO canónico:** `demo/libzombie_shooter.so`, tamaño `9,773,412` bytes, queda protegido como copia de referencia de análisis; no eliminar, mover, reemplazar, strippear ni parchear directamente.
- **Crash static analysis:** el workflow nuevo genera `crash-zone-disasm.txt` alrededor de `SO+0x00515AE4` y lo guarda sólo en el artifact del run para análisis.
- **Assets/commonAssets:** inmediatamente antes del crash faltan `music/rain.ogg` y varios `vid/*.vid`; también se repite el flujo JNI `commonAssets` con `method ID 0 not found`. Correlación fuerte, causalidad aún no demostrada.
- **Crash de segunda entrada:** sigue siendo un caso separado hasta cruzar su propio log/dump.

## Estado verificable

| Área | Estado | Evidencia / límite |
| --- | --- | --- |
| Arquitectura Android | VERIFICADO | APK `com.sigmateam.zombieshooter.free` 3.5.3; NativeActivity; ARMv7/Thumb-2 EABI5 soft-float. |
| SO utilizado | VERIFICADO | `demo/libzombie_shooter.so`, tamaño `9,773,412`; coincide con el SO cargado por la Vita. SHA-256 histórico esperado `5e5e2e1bbe86e126c3e2dce5ad97c2e9dc6282305ea7d3e24b49ee4769c5b5b7`. |
| ABI/toolchain local | VERIFICADO | `/usr/local/vitasdk`, SoftFP. HardFP no se usa para Zombie Shooter. |
| ABI/toolchain CI | BUILD VERIFIED | GitHub Actions usa `vitasdk/vitasdk-softfp:nightly`, GCC 15.2.0 y comprueba `-mfloat-abi=softfp`. |
| Entrypoint/lifecycle | VERIFICADO | `GameActivity → CommonActivity → android.app.NativeActivity`; SO exporta `ANativeActivity_onCreate`. |
| Imports | COBERTURA ESTÁTICA VERIFICADA | 412 símbolos undefined únicos tienen entrada explícita en `dynlib.c`. |
| Constructores | REAL VITA VERIFIED | 41/41 `.init_array`; kuser pointers de Protobuf parcheados. |
| VitaGL/EGL | RENDER VERIFICADO | Logos, loading, tutorial y gameplay renderizan. Swap ~0,2 ms en el log reciente. |
| MSAA | A/B COMPLETADO | `SCE_GXM_MULTISAMPLE_NONE` se mantiene; no fue cuello dominante. |
| Audio | REAL VITA VERIFIED | Audio funcional; fix OpenSL ES evita el bloqueo indefinido en `IBufferQueue_Clear`. |
| Input touch | REAL VITA VERIFIED | Touch y AInputQueue funcionales. |
| Input físico | PENDIENTE | Vita es detectada como gamepad, mapping Xbox-like aún no verificado. |
| Gameplay | PARCIAL | Tutorial jugable a bajo FPS; zona nueva provoca crash reproducido. |
| Crash nueva zona | EVIDENCIA DE CORE | Data Abort en `SO+0x00515AE4`, DFAR `0x07D36E9C`; no stack overflow. |
| Assets/PAD | SOSPECHA FUERTE | `commonAssets` no resuelto y recursos de zona faltantes aparecen inmediatamente antes del crash; falta demostrar causalidad en desensamblado. |
| Logging Release run #8 | CONTRATO FALLIDO EN HARDWARE | Usuario confirma Release instalado; el log fue Debug-like y no tuvo `[PERF] mem`. Causa exacta retrospectiva no demostrable porque el run no guardó flags reales. |
| Logging Release actual | FIX PREPARED / PENDING CI + HARDWARE | Identidad explícita y aserciones de `flags.make`; cada log estampa variante + build ID. |
| Overclock anterior | TEST INVALIDADO COMO A/B | Builds anteriores reducían clocks a 444/222/222/166 al arrancar. |
| Overclock actual | FIX PREPARED / PENDING HARDWARE | El port preserva PSVshell y sólo lee/registra frecuencias. |
| PSVshell memoria | REAL VITA OBSERVED | MEM/VMEM/PHY aparecen llenos desde la perspectiva del sistema. |
| VitaGL memoria interna | NO MEDIDA VÁLIDAMENTE TODAVÍA | Run #8 no produjo `[PERF] mem`; repetir sólo con cabecera `variant=Release`. |
| Heap newlib | SIN CAMBIOS | `_newlib_heap_size_user = 256 MiB`; no reducir sin telemetría válida. |
| Workflow manual | WORKFLOW VERIFIED / NUEVAS ASERCIONES PENDING | Run #8 compiló y publicó; próximo run debe validar identidad Debug/Release y generar análisis del SO. |
| Vita3K | NO USADO | Runtime se valida en Vita real. |

## Crash de nueva zona — evidencia mínima de continuidad

Ejecución real:

```text
LOADING completa
tutorial ~5 FPS
movimiento + disparos funcionales
entrada a zona nueva
→ crash
```

Core:

```text
thread: pthread
stop reason: 0x30004 Data abort exception
LOAD_ADDRESS: 0x98000000
PC:   0x98515AE4 = SO + 0x00515AE4
LR:   0x98515A5F = SO + 0x00515A5F
DFSR: 0x000000F5
DFAR: 0x07D36E9C
stack peak: 7224 bytes
stack current: 456 bytes
```

Direcciones adicionales plausibles del SO encontradas alrededor del stack:

```text
SO + 0x00515F5F
SO + 0x00515961
SO + 0x0050BB99
```

Últimos recursos fallidos antes del crash:

```text
music/rain.ogg
vid/2010.vid
vid/1071.vid
vid/413.vid
vid/2020.vid
```

El engine reporta para los `.vid`:

```text
Can't open
Can't load PAL section
Can't load DATA section
Can't read cadr data
```

No asumir todavía que los archivos faltantes son la causa directa. El workflow siguiente extrae una ventana mínima de desensamblado del SO canónico para identificar la instrucción de `+0x00515AE4`.

## Rendimiento: evidencia actual

Del `log_0003.log` recibido:

```text
101 ventanas [PERF] present
~1955 frames
~560.235 segundos agregados
FPS agregado aproximado: 3.49
swap promedio ponderado: ~200 us
```

Ejemplo:

```text
frames=14 elapsed_ms=5077 fps_x10=27
swap_avg_us=191
swap_max_us=224
frame_max_us=3238205
```

Conclusión soportada: el cuello principal ocurre antes del `eglSwapBuffers`; no priorizar resolución/fill-rate sólo por los FPS observados.

## Build identity / logging: contrato nuevo

CMake ya no usa `NDEBUG` como identidad del port. Define explícitamente:

```text
Debug:
  ZOMBIE_DEBUG_BUILD=1
  FALSOJNI_DEBUGLEVEL=0
  ZOMBIE_BUILD_VARIANT="Debug"

Release:
  ZOMBIE_RELEASE_BUILD=1
  FALSOJNI_DEBUGLEVEL=4
  ZOMBIE_BUILD_VARIANT="Release"
```

Todo log nuevo debe empezar con:

```text
=== BUILD variant=Release id=<short-sha> ===
```

cuando se instala la Release.

CI falla antes de publicar si `flags.make` no confirma esas definiciones. Esto convierte la identidad de build en evidencia, no en una suposición.

## Clocks: política actual

Se eliminaron los setters internos que imponían:

```text
ARM 444 / BUS 222 / GPU 222 / XBAR 166
```

El port ahora preserva el perfil externo y registra:

```text
[PERF] clocks_preserved arm=... bus=... gpu=... xbar=...
```

La siguiente prueba con PSVshell al máximo será la primera A/B válida de clocks desde que se detectó el override.

## Memoria: estado correcto

El port sigue usando:

```c
int _newlib_heap_size_user = 256 * 1024 * 1024;

vglInitExtended(0, 960, 544,
                6 * 1024 * 1024,
                SCE_GXM_MULTISAMPLE_NONE);
```

PSVshell mostrando 100% no equivale automáticamente a agotamiento del allocator interno de VitaGL porque VitaGL reserva memblocks y subasigna internamente.

La instrumentación preparada intenta registrar:

```text
[PERF] mem phase=before_vgl ...
[PERF] mem phase=after_vgl ...
[PERF] mem phase=runtime ...
```

La próxima medición sólo se acepta si el mismo log confirma `variant=Release`.

## Problemas abiertos y prioridad vigente

1. Ejecutar un workflow nuevo y verificar que CI confirma las definiciones Debug/Release.
2. Leer `crash-zone-disasm.txt` del artifact y resolver la instrucción en `SO+0x00515AE4`.
3. Determinar si el puntero que genera `DFAR=0x07D36E9C` proviene del flujo `RESOURCE`/`VID`/`commonAssets` o de corrupción independiente.
4. Instalar la nueva Release y confirmar en el log: variante, build ID, clocks preservados y `[PERF] mem`.
5. Sólo con esa evidencia decidir si tocar heap/pools o continuar a CPU/I/O/waits.
6. Mantener separado el crash de segunda entrada al tutorial.
7. Retomar controles Xbox-like después de estabilidad/rendimiento básicos.

## Builds

### Local

```bash
vita-softfp
cd ~/Zombie-Shooter-PS-Vita-Port

rm -rf build-session-debug
cmake -S . -B build-session-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-session-debug -j"$(nproc)"

rm -rf build-session-release
cmake -S . -B build-session-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-session-release -j"$(nproc)"
```

### Remoto manual

```text
GitHub → Actions → Manual Vita VPK Pre-release → Run workflow
```

Genera Debug + Release. El Pre-release no publica APK/XAPK, Android SO ni assets propietarios. El artifact de Actions añade `build-flags.txt` y `crash-zone-disasm.txt` para diagnóstico interno.

## Regla de continuidad

No convertir correlaciones en fixes sin evidencia:

```text
Data Abort en SO
+ DFAR inválido
+ commonAssets no resuelto
+ recursos de zona ausentes
```

es suficiente para priorizar ese camino, pero no demuestra aún que “copiar los `.vid`” o “aumentar memoria” sea el fix. Primero identificar la instrucción en `SO+0x00515AE4`.
