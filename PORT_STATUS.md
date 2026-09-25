# Estado del port de Zombie Shooter para PS Vita

Actualizado: 2026-09-25.

Fuentes de continuidad:

- `PORTING_PLAN.md`: evidencia APK/JADX/SO y arquitectura del juego.
- `port_progress.md`: historial técnico bug-a-bug previo.
- `docs/HARDWARE_TEST_LOG.md`: historial cronológico de pruebas reales y resultados A/B.
- `docs/PERFORMANCE_PLAN.md`: orden vigente de investigación de rendimiento.
- `docs/CRASH_REPORT_2026-09-25_NEW_ZONE.md`: análisis del Data Abort ocurrido al entrar en una zona nueva.

## Estado actual resumido

- **Boot/gameplay:** Vita real llega a logos, `LOADING`, tutorial y gameplay. Touch y audio funcionan.
- **Audio:** fix de `IBufferQueue_Clear()` está **REAL VITA VERIFIED**. No revertir.
- **Rendimiento:** tutorial sigue alrededor de ~5 FPS; en el log analizado `eglSwapBuffers` promedia aproximadamente 0,2 ms, por lo que el present no explica el frame lento.
- **MSAA:** `4x → NONE` fue probado y no produjo mejora significativa.
- **Logging Release:** la supuesta build Release silenciosa necesita revalidación. El último log contiene trazas completas Debug/FalsoJNI/FalsoNDK y no contiene la telemetría `[PERF] mem` esperada.
- **Clocks:** `source/utils/init.c` fuerza `ARM 444 / BUS 222 / GPU 222 / XBAR 166` al arrancar. Por tanto las pruebas hechas con PSVshell configurado al máximo NO son todavía pruebas válidas de clocks máximos reales: el port los sobrescribe.
- **Memoria:** PSVshell ha mostrado `MEM 365/365`, `VMEM 112/112`, `PHY 26/26`, pero no existe todavía una captura válida de `vglMemFree/vglMemTotal` de la build correcta. No declarar OOM.
- **Crash nueva zona:** reproducido después de cargar el tutorial y jugar. Core: `Data abort exception` dentro de `libzombie_shooter.so`, PC `SO+0x00515AE4`, LR `SO+0x00515A5F`, DFAR inválido `0x07D36E9C`.
- **Assets/commonAssets:** inmediatamente antes del crash faltan `music/rain.ogg` y varios `vid/*.vid`; también se repite el flujo JNI `commonAssets` con `method ID 0 not found`. Correlación fuerte, causalidad aún no demostrada.
- **Crash de segunda entrada:** sigue siendo un caso separado hasta cruzar su propio log/dump.

## Estado verificable

| Área | Estado | Evidencia / límite |
| --- | --- | --- |
| Arquitectura Android | VERIFICADO | APK `com.sigmateam.zombieshooter.free` 3.5.3; NativeActivity; ARMv7/Thumb-2 EABI5 soft-float. |
| SO utilizado | VERIFICADO | `libzombie_shooter.so`, SHA-256 `5e5e2e1bbe86e126c3e2dce5ad97c2e9dc6282305ea7d3e24b49ee4769c5b5b7`. |
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
| Logging Release | REVALIDACIÓN NECESARIA | Último `log_0003.log` es Debug-like y no contiene `[PERF] mem`; identidad/configuración de build debe hacerse explícita en runtime. |
| Overclock | TEST ANTERIOR INVALIDADO COMO A/B | El port fuerza 444/222/222/166 después de abrirse, sobrescribiendo PSVshell. |
| PSVshell memoria | REAL VITA OBSERVED | MEM/VMEM/PHY aparecen llenos desde la perspectiva del sistema. |
| VitaGL memoria interna | NO MEDIDA TODAVÍA | Cero líneas `[PERF] mem phase=` en el log analizado. |
| Heap newlib | SIN CAMBIOS | `_newlib_heap_size_user = 256 MiB`; no reducir sin telemetría válida. |
| Workflow manual | WORKFLOW VERIFIED | Run #8 compiló Debug + Release y publicó `vita-test-8-9a8be03`. |
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

No asumir todavía que los archivos faltantes son la causa directa. El próximo paso de root-cause es desensamblar `libzombie_shooter.so + 0x00515AE4` y determinar qué puntero/registro genera el acceso a `DFAR`.

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

## Clocks: corrección importante

El port ejecuta actualmente:

```c
scePowerSetArmClockFrequency(444);
scePowerSetBusClockFrequency(222);
scePowerSetGpuClockFrequency(222);
scePowerSetGpuXbarClockFrequency(166);
```

Por tanto, una configuración de PSVshell superior puede ser reducida al iniciar el juego. Antes de cualquier nuevo A/B de clocks, cambiar la política para no bajar frecuencias ya configuradas o limitarse a registrar las actuales.

## Memoria: estado correcto

El port usa actualmente:

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

pero el log real analizado contiene **cero** líneas de ese tipo. No modificar todavía heap/thresholds/pools basándose en la lectura 100% de PSVshell.

## Problemas abiertos y prioridad vigente

1. **Desensamblar el Android SO en `+0x00515AE4`** y determinar la instrucción/dirección base que causa el Data Abort.
2. Rastrear si ese objeto/puntero proviene del flujo de `RESOURCE`/`VID`/`commonAssets` o de corrupción independiente.
3. Añadir identidad inequívoca a cada build en runtime (`Debug/Release` + commit/build ID).
4. Corregir la política de clocks para no sobrescribir un overclock mayor de PSVshell durante benchmarks.
5. Repetir telemetría de memoria con una build cuya identidad aparezca en el log; sólo entonces decidir si tocar heap/pools.
6. Continuar profiling CPU/I/O/waits si memoria resulta sana.
7. Mantener separado el crash de segunda entrada al tutorial.
8. Retomar controles Xbox-like después de estabilidad/rendimiento básicos.

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

Genera Debug + Release. No publica APK/XAPK, Android SO ni assets propietarios.

## Regla de continuidad

No convertir correlaciones en fixes sin evidencia:

```text
Data Abort en SO
+ DFAR inválido
+ commonAssets no resuelto
+ recursos de zona ausentes
```

es evidencia suficiente para priorizar ese camino, pero no demuestra aún que “copiar los `.vid`” o “aumentar memoria” sea el fix. Primero identificar la instrucción en `SO+0x00515AE4`.
