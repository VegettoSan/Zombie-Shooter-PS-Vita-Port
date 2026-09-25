# Estado del port de Zombie Shooter para PS Vita

Actualizado: 2026-09-25.

Fuentes de continuidad:

- `PORTING_PLAN.md`: evidencia APK/JADX/SO y arquitectura del juego.
- `port_progress.md`: historial técnico bug-a-bug previo.
- `docs/HARDWARE_TEST_LOG.md`: historial cronológico de pruebas reales y resultados A/B recientes.
- `docs/PERFORMANCE_PLAN.md`: orden vigente para investigar rendimiento.

## Iteración de hardware actual

- **Boot/gameplay:** Vita real supera logos y `LOADING`, entra al tutorial, reproduce audio, permite caminar, salir al menú principal y volver a intentar cargar el tutorial.
- **Audio:** el cambio local de OpenSL ES que acota `IBufferQueue_Clear()` está **REAL VITA VERIFIED**. Los sonidos funcionan y `footsteps.wav` ya no deja el game thread detenido indefinidamente.
- **Rendimiento:** sigue siendo crítico. La prueba Release con MSAA desactivado mostró aproximadamente **7 FPS al iniciar**, **4 FPS en `LOADING`** y una carga de **casi 5 minutos**. No hubo una mejora significativa atribuible a MSAA.
- **Hipótesis de rendimiento actual:** se detectó que Release todavía imprimía mucho tráfico desde FalsoNDK, FalsoJNI y `SO_UTIL_VERBOSE`. Se preparó una Release realmente silenciosa; queda **PENDING HARDWARE** comparar sus FPS y tiempo de carga.
- **Crash separado:** después de salir del tutorial al menú y volver a entrar, algunas imágenes/texturas no cargan y posteriormente el juego crashea. El usuario conserva en PC el log y `.psp2dmp`; la causa sigue pendiente de análisis y no debe mezclarse con la investigación de FPS.
- **Build remoto:** el workflow manual de GitHub Actions ya compila Debug + Release con VitaSDK SoftFP y publica ambos VPK en un **Pre-release**. El run #4 fue exitoso de extremo a extremo.

## Estado verificable

| Área | Estado | Evidencia / límite |
| --- | --- | --- |
| Arquitectura Android | VERIFICADO | APK `com.sigmateam.zombieshooter.free` 3.5.3. ELF32 ARMv7/Thumb-2 EABI5 soft-float. Manifest: minSdk 24, targetSdk 35. |
| SO utilizado | VERIFICADO | `data/libzombie_shooter.so`, SHA-256 `5e5e2e1bbe86e126c3e2dce5ad97c2e9dc6282305ea7d3e24b49ee4769c5b5b7`; coincide con el split ARM. |
| ABI/toolchain local | VERIFICADO | SO y loader usan SoftFP. WSL: `/usr/local/vitasdk`, GCC 10.3.0 histórico del proyecto, `-mfloat-abi=softfp`. `/usr/local/vitasdk-hardfp` no debe usarse. |
| ABI/toolchain CI | BUILD VERIFIED | GitHub Actions usa `vitasdk/vitasdk-softfp:nightly`, GCC 15.2.0, y aborta si no detecta `-mfloat-abi=softfp`. |
| Entrypoint/lifecycle | VERIFICADO | `GameActivity` → `CommonActivity` → `android.app.NativeActivity`; metadata `android.app.lib_name=zombie_shooter`. El SO exporta `ANativeActivity_onCreate`; no exporta `JNI_OnLoad`, `android_main` ni `Java_*`. |
| JNI | PARCIALMENTE VERIFICADO | `IsInstanceOf(activity, Context)` funciona en hardware. `AssetPackManagerFactory.getInstance(Context)`/Play Asset Delivery sigue incompleto y `Wrong AES key length` permanece abierto. |
| Imports | COBERTURA ESTÁTICA VERIFICADA | Los 412 símbolos undefined únicos tienen entrada explícita en `dynlib.c`. Esto no garantiza semántica perfecta de todas las rutas. |
| Constructores | VERIFICADO EN HARDWARE | Se completan las 41 entradas de `.init_array`; los punteros Android kuser usados por Protobuf ya se parchean. |
| VitaGL/EGL | RENDER VERIFICADO | Logos, `LOADING`, tutorial y menú principal se renderizan. El mapeo compacto de nombres de VBO corrigió el truncado GLES. `eglSwapBuffers` ~0,2 ms no explica por sí solo el bajo FPS. |
| MSAA | A/B PROBADO; NO FUE CUELLO DOMINANTE | `SCE_GXM_MULTISAMPLE_4X` → `NONE` no produjo un salto claro: Release siguió ~7 FPS al inicio y ~4 FPS en `LOADING`. Se mantiene `NONE`, pero no se prioriza fill-rate como explicación principal. |
| Audio | FIX VERIFICADO EN HARDWARE | OpenSL ES reproduce audio y el bloqueo previo en `IBufferQueue_Clear` dejó de impedir avanzar. Queda pendiente medir cualquier coste residual de timeouts. |
| Input | TÁCTIL VERIFICADO; FÍSICO PENDIENTE | La cola de input y touch funcionan. La Vita es detectada como gamepad, pero el mapping tipo Xbox/XInput aún debe corregirse y verificarse. |
| Assets | TUTORIAL/MENÚ VERIFICADOS; REENTRADA PENDIENTE | El primer ingreso carga suficientes recursos. La segunda entrada presenta recursos ausentes y crash pendiente de log/dump. |
| Filesystem | VERIFICADO | Base `ux0:data/zombieshooter/` con `libzombie_shooter.so`, `assets/` y `logs/`. |
| Logging Debug | INTENCIONALMENTE VERBOSO | Debug conserva loader/FalsoJNI/FalsoNDK/so-util tracing para dumps y diagnóstico. |
| Logging Release | CAMBIO PREPARADO; PENDING HARDWARE | Release conserva `[PERF]` y errores críticos del loader, silencia FalsoNDK no fatal, usa `FALSOJNI_DEBUG_NO` y no define `SO_UTIL_VERBOSE`. |
| Build Debug local | BUILD VERIFIED | `build-session-debug/zombie_shooter.vpk`. |
| Build Release local | BUILD VERIFIED | `build-session-release/zombie_shooter.vpk`. |
| Workflow manual | WORKFLOW VERIFIED | `.github/workflows/manual-prerelease.yml`; `workflow_dispatch`; genera Debug + Release y publica Pre-release. Run #4 exitoso. |
| Vita3K | NO USADO | La validación runtime de este port se hace en PS Vita real. |
| Vita real | TUTORIAL + MENÚ VERIFICADOS | Boot, loading, gameplay inicial, audio, touch y retorno al menú confirmados. Reentrada sigue abierta. |
| Jugabilidad | AÚN NO | Existe gameplay inicial, pero FPS y crash de reentrada impiden considerar el port jugable. |
| Rendimiento | INVESTIGACIÓN ABIERTA | Prioridad actual: probar Release silenciosa; si no cambia, perfilar I/O, waits/sleeps, CPU/engine y sincronización antes de VitaGL speedhacks. |

## Cambios técnicos principales confirmados

- `source/patch.c`: puente kuser de Android, parches de punteros globales usados por Protobuf e instrumentación dirigida.
- `source/utils/init.c`: ejecución registrada de las 41 entradas de `.init_array`.
- `source/reimpl/bionic_compat.c/.h`: compatibilidad Bionic/FORTIFY y wrappers requeridos por el SO.
- `source/reimpl/pthr.c/.h`: layout Bionic de rwlock, `pthread_once` y manejo de inicializadores estáticos.
- `source/dynlib.c`: cobertura explícita para los 412 imports del SO.
- `source/java.c`: superficie Java/FalsoJNI requerida por el juego; contexto Activity validado.
- `source/main.c`: NativeActivity/AssetManager/lifecycle y diagnóstico de presents/input.
- `lib/falso_ndk/android/AAssetManager.cpp`: buffering de `.vid` y assets pequeños para reducir agotamiento de handles.
- `source/utils/glutil.c` / `source/reimpl/egl.c`: VitaGL, bridge EGL, cadence y traducción compacta de IDs GLES; actualmente MSAA NONE.
- `source/utils/logger.c/.h`: un log por ejecución; Debug detallado; Release reduce tráfico del logger del port.
- `source/reimpl/fndk_log.c`: Debug conserva FalsoNDK; Release silencia mensajes no fatales para la prueba de rendimiento.
- `lib/opensles_clear/IBufferQueue.c`: `IBufferQueue_Clear` con timeout acotado de 100 ms; fix confirmado en hardware.
- `CMakeLists.txt`: SoftFP, VitaGL vendorizada, sustitución de `IBufferQueue.o`; Debug y Release mantienen roles distintos de diagnóstico vs gameplay/FPS.
- `.github/workflows/manual-prerelease.yml`: build manual SoftFP de ambos VPK + Pre-release.

## Build reproducible local

```bash
export VITASDK=/usr/local/vitasdk
export PATH="$VITASDK/bin:$PATH"
cd ~/Zombie-Shooter-PS-Vita-Port

cmake -S . -B build-session-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-session-debug -j"$(nproc)"

cmake -S . -B build-session-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-session-release -j"$(nproc)"
```

VPKs:

```text
~/Zombie-Shooter-PS-Vita-Port/build-session-debug/zombie_shooter.vpk
~/Zombie-Shooter-PS-Vita-Port/build-session-release/zombie_shooter.vpk
```

## Build remoto manual

Desde GitHub:

```text
Actions
→ Manual Vita VPK Pre-release
→ Run workflow
```

Genera:

```text
Zombie-Shooter-Vita-Debug.vpk
Zombie-Shooter-Vita-Release.vpk
build-info.txt
SHA256SUMS.txt
```

El workflow no contiene ni publica APK/XAPK, `libzombie_shooter.so` ni assets del juego.

## Datos requeridos

Conservar exactamente:

```text
ux0:data/zombieshooter/libzombie_shooter.so
ux0:data/zombieshooter/assets/...
ux0:data/zombieshooter/logs/...
```

No hace falta volver a copiar datos para cada VPK si el árbol de assets no cambió.

## Requisitos de Vita real

- `kubridge.skprx` v0.3.1 hotfix o posterior bajo `*KERNEL`, normalmente `ur0:tai/kubridge.skprx`, seguido de reinicio.
- `libshacccg.suprx` obtenido legalmente con ShaRKBR33D en `ur0:data/libshacccg.suprx` o `ur0:data/external/libshacccg.suprx`.
- No se ha demostrado que iTLS-Enso u otro plugin adicional sea necesario.

## Problemas abiertos y prioridad vigente

1. **Probar en hardware la Release silenciosa** y comparar contra el baseline: inicio ~7 FPS, `LOADING` ~4 FPS, carga ~5 min.
2. Si casi no mejora, **medir I/O y esperas**: `AAsset_open/read/seek`, sleeps/polls/condvars, OpenSL timeouts y tiempo CPU del engine mediante agregados, no spam por llamada.
3. **Analizar el crash de reentrada** con el `log_*.log` y `.psp2dmp` reales antes de aplicar fixes de assets/memoria especulativos.
4. **Controles físicos tipo Xbox/XInput**: corregir device ID, deadzone, axes y mapping una vez retomemos input; touch debe permanecer intacto.
5. Sólo después de medir CPU/GPU/I/O considerar cache de shaders, pools VitaGL o speedhacks uno por uno.
6. Play Asset Delivery / `commonAssets;fast-follow` y `Wrong AES key length` siguen pendientes, pero no son la prioridad mientras el primer tutorial ya carga.

## Próxima medición de rendimiento

Usar la primera entrada al tutorial y no reentrar durante la prueba.

Registrar:

```text
FPS al iniciar
FPS durante LOADING
tiempo total de LOADING
FPS en la misma zona del tutorial durante 30–60 s
```

Comparar contra:

```text
Release anterior, MSAA NONE:
inicio  ≈ 7 FPS
LOADING ≈ 4 FPS
carga   ≈ 5 min
```

La nueva variable aislada es **overhead de logging en Release**. No mezclarla todavía con speedhacks, resolución o cambios de clocks.
