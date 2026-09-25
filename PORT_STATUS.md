# Estado del port de Zombie Shooter para PS Vita

Actualizado: 2026-09-25.

Fuentes de continuidad:

- `PORTING_PLAN.md`: evidencia APK/JADX/SO y arquitectura del juego.
- `port_progress.md`: historial técnico bug-a-bug previo.
- `docs/HARDWARE_TEST_LOG.md`: historial cronológico de pruebas reales y resultados A/B.
- `docs/PERFORMANCE_PLAN.md`: orden vigente de investigación de rendimiento.

## Estado actual resumido

- **Boot/gameplay:** Vita real ha llegado anteriormente a logos, `LOADING`, tutorial y menú. Touch y audio funcionan.
- **Audio:** `IBufferQueue_Clear()` con timeout acotado está **REAL VITA VERIFIED**. No revertir.
- **MSAA:** `4x → NONE` fue probado y no produjo mejora significativa.
- **Release silenciosa:** probada en Vita real con PSVshell al máximo; siguió aproximadamente entre 1–7 FPS y `LOADING` entre 2–4 FPS. Después de ~6 minutos durante la primera carga ocurrió un crash.
- **Clocks:** maximizar PSVshell no resolvió el cuello; no priorizar más overclock.
- **Memoria observada:** durante `LOADING`, PSVshell mostró `MEM 365/365 MB`, `VMEM 112/112 MB`, `PHY 26/26 MB`.
- **Interpretación verificada:** PSVshell muestra usado (`total-free`) / total. Sin embargo VitaGL reserva grandes memblocks y subasigna internamente, así que 100% en PSVshell no prueba por sí solo que `vglMemFree()==0`.
- **Próxima build:** sólo añade telemetría de memoria; no cambia heap, thresholds ni pools. Commit de instrumentación: `168d08834c83aadb849c0bb5bff283e764927357`.
- **Crash separado de reentrada:** segunda entrada al tutorial presenta texturas ausentes y crash; sigue pendiente de analizar el log y `.psp2dmp` reales guardados por el usuario.

## Estado verificable

| Área | Estado | Evidencia / límite |
| --- | --- | --- |
| Arquitectura Android | VERIFICADO | APK `com.sigmateam.zombieshooter.free` 3.5.3; NativeActivity; ARMv7/Thumb-2 EABI5 soft-float. |
| SO utilizado | VERIFICADO | `libzombie_shooter.so`, SHA-256 `5e5e2e1bbe86e126c3e2dce5ad97c2e9dc6282305ea7d3e24b49ee4769c5b5b7`. |
| ABI/toolchain local | VERIFICADO | `/usr/local/vitasdk`, SoftFP. `/usr/local/vitasdk-hardfp` no se usa para este port. |
| ABI/toolchain CI | BUILD VERIFIED | GitHub Actions usa `vitasdk/vitasdk-softfp:nightly`, GCC 15.2.0 y comprueba `-mfloat-abi=softfp`. |
| Entrypoint/lifecycle | VERIFICADO | `GameActivity → CommonActivity → android.app.NativeActivity`; SO exporta `ANativeActivity_onCreate`. |
| Imports | COBERTURA ESTÁTICA VERIFICADA | 412 símbolos undefined únicos tienen entrada explícita en `dynlib.c`. |
| Constructores | REAL VITA VERIFIED | 41/41 `.init_array`; kuser pointers usados por Protobuf parcheados. |
| VitaGL/EGL | RENDER VERIFICADO | Logos, loading, tutorial y menú han renderizado. `eglSwapBuffers` ~0,2 ms histórico. |
| MSAA | A/B COMPLETADO | `SCE_GXM_MULTISAMPLE_NONE` se mantiene; no fue cuello dominante. |
| Audio | REAL VITA VERIFIED | Audio funcional; fix OpenSL ES evita el bloqueo indefinido en `IBufferQueue_Clear`. |
| Input touch | REAL VITA VERIFIED | Touch y AInputQueue funcionales. |
| Input físico | PENDIENTE | Vita es detectada como gamepad, pero mapping Xbox-like todavía no está verificado. |
| Assets | PRIMERA CARGA INESTABLE EN BUILD ACTUAL | Builds anteriores alcanzaron tutorial. Run #6 Release crasheó tras ~6 min en primera carga. Segunda entrada tiene además un bug separado conocido. |
| Logging Release | REAL VITA TESTED | Logging pesado fue reducido; no produjo mejora importante de FPS/carga. |
| Overclock | A/B REAL VITA TESTED | PSVshell al máximo no solucionó el bajo FPS. |
| PSVshell memoria | REAL VITA OBSERVED | Durante loading: MEM 365/365, VMEM 112/112, PHY 26/26. |
| VitaGL memoria interna | PENDING MEASUREMENT | La siguiente build registra `vglMemFree/vglMemTotal` para RAM/VRAM/PHYCONT. |
| Heap newlib | SIN CAMBIOS | `_newlib_heap_size_user = 256 MiB`; no reducir hasta ver telemetría. |
| Workflow manual | WORKFLOW VERIFIED | `workflow_dispatch`; Debug + Release; Pre-release. Run #6 exitoso de extremo a extremo. |
| Vita3K | NO USADO | Runtime se valida en Vita real. |
| Jugabilidad | AÚN NO | FPS, tiempo de carga y crashes impiden considerarlo jugable. |

## Memoria: hipótesis actual

El port usa actualmente:

```c
int _newlib_heap_size_user = 256 * 1024 * 1024;

vglInitExtended(0, 960, 544,
                6 * 1024 * 1024,
                SCE_GXM_MULTISAMPLE_NONE);
```

En la revisión de VitaGL fijada por el proyecto, ese `ram_threshold` hace que VitaGL reserve aproximadamente toda la USER RAM libre menos 6 MiB; los thresholds de CDRAM y PHYCONT de `vglInitExtended()` quedan en 0, por lo que también puede reservar prácticamente todo lo disponible de esos espacios.

Eso explica por qué PSVshell puede mostrar 100% aunque aún exista memoria libre dentro de los suballocators de VitaGL.

### Instrumentación actual

`source/utils/glutil.c` registra:

```text
[PERF] mem phase=before_vgl ...
[PERF] mem phase=after_vgl ...
[PERF] mem phase=runtime ...
```

Incluye:

```text
system free USER / CDRAM / PHYCONT
vglMemFree / vglMemTotal RAM
vglMemFree / vglMemTotal VRAM/CDRAM
vglMemFree / vglMemTotal PHYCONT
```

No se cambió aún ninguna política de asignación.

## Builds

### Local

```bash
export VITASDK=/usr/local/vitasdk
export PATH="$VITASDK/bin:$PATH"
cd ~/Zombie-Shooter-PS-Vita-Port

rm -rf build-session-debug
cmake -S . -B build-session-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-session-debug -j"$(nproc)"

rm -rf build-session-release
cmake -S . -B build-session-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-session-release -j"$(nproc)"
```

VPKs:

```text
build-session-debug/zombie_shooter.vpk
build-session-release/zombie_shooter.vpk
```

### Remoto manual

```text
GitHub → Actions → Manual Vita VPK Pre-release → Run workflow
```

Genera:

```text
Zombie-Shooter-Vita-Debug.vpk
Zombie-Shooter-Vita-Release.vpk
build-info.txt
SHA256SUMS.txt
```

No publica APK/XAPK, `libzombie_shooter.so` ni assets propietarios.

## Problemas abiertos y prioridad vigente

1. **Compilar y probar la build de telemetría de memoria.** No cambiar pools/heap todavía.
2. Determinar si los pools internos de VitaGL llegan realmente a cero o si el 100% de PSVshell es principalmente reserva anticipada.
3. Sólo con esa evidencia, ajustar un único parámetro de memoria por build (threshold/pool/heap).
4. Si memoria resulta sana, pasar a profiling agregado de I/O y waits/sleeps/sincronización.
5. Analizar aparte el `log_*.log` + `.psp2dmp` del crash conocido de segunda entrada al tutorial.
6. Retomar controles físicos Xbox-like una vez la carga y estabilidad sean razonables.
7. Shader cache / speedhacks / resolución quedan después del diagnóstico de memoria e I/O.

## Próxima prueba de Vita

Usar Release de la siguiente Pre-release y mantener el mismo perfil de PSVshell para comparabilidad.

Registrar:

```text
FPS logos
FPS LOADING
tiempo hasta crash o tutorial
PSVshell MEM / VMEM / PHY
últimas líneas [PERF] mem del log
```

Lo más importante son las líneas inmediatamente anteriores al crash, especialmente:

```text
vgl_free_total_kib ram=.../... vram=.../... phy=.../...
```

No interpretar el 100% de PSVshell como OOM definitivo hasta comparar esos valores internos.
