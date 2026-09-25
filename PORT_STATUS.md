# Estado del port de Zombie Shooter para PS Vita

Actualizado: 2026-09-25. El mapa de evidencia APK/JADX/SO está en `PORTING_PLAN.md` y el historial bug-a-bug en `port_progress.md`.

## Iteración de hardware actual

- **Current boot stage:** Vita real supera los logos y `LOADING`, entra al nivel tutorial, reproduce audio, permite desplazarse por el tutorial, salir al menú principal y volver a intentar cargar el tutorial.
- **Audio:** el cambio local de OpenSL ES que acota `IBufferQueue_Clear()` ya fue validado por el usuario en Vita real. Los sonidos se reproducen y `footsteps.wav` ya no deja el juego detenido indefinidamente.
- **Rendimiento:** el tutorial sigue funcionando aproximadamente a **1–7 FPS reales**. `eglSwapBuffers` se había medido alrededor de 0,2 ms, por lo que el present por sí solo no explica la baja cadencia.
- **Nuevo problema reproducible reportado:** después de salir del tutorial al menú principal y volver a entrar, algunas imágenes/texturas no cargan y posteriormente el juego crashea. El usuario conserva en PC el log y el crash dump de esa ejecución; la causa queda **pendiente de análisis con esos archivos**, por lo que no se atribuye todavía a assets, memoria, VitaGL ni lifecycle.
- **Siguiente frente de trabajo:** avanzar optimizaciones de bajo riesgo que no cambien lifecycle/assets/audio mientras el crash de reentrada queda reservado para una sesión de Codex con el log y `.psp2dmp` reales.

## Estado verificable

| Área | Estado | Evidencia / límite |
| --- | --- | --- |
| Arquitectura Android | VERIFICADO | APK `com.sigmateam.zombieshooter.free` 3.5.3. ELF32 ARMv7/Thumb-2 EABI5 soft-float. Manifest: minSdk 24, targetSdk 35. |
| SO utilizado | VERIFICADO | `data/libzombie_shooter.so`, SHA-256 `5e5e2e1bbe86e126c3e2dce5ad97c2e9dc6282305ea7d3e24b49ee4769c5b5b7`; coincide con el split ARM. |
| ABI/toolchain | VERIFICADO | SO y loader usan soft-float. Toolchain: `/usr/local/vitasdk`, GCC 10.3.0, `-mfloat-abi=softfp`. `/usr/local/vitasdk-hardfp` no debe usarse para este port. |
| Entrypoint/lifecycle | VERIFICADO | `GameActivity` → `CommonActivity` → `android.app.NativeActivity`; metadata `android.app.lib_name=zombie_shooter`. El SO exporta `ANativeActivity_onCreate`; no exporta `JNI_OnLoad`, `android_main` ni `Java_*`. |
| JNI | PARCIALMENTE VERIFICADO | `IsInstanceOf(activity, Context)` ya funciona en hardware. `AssetPackManagerFactory.getInstance(Context)`/Play Asset Delivery sigue incompleto y `Wrong AES key length` permanece abierto. |
| Imports | COBERTURA ESTÁTICA VERIFICADA | Los 412 símbolos undefined únicos tienen entrada explícita en `dynlib.c`. Esto no garantiza semántica perfecta de todas las rutas. |
| Constructores | VERIFICADO EN HARDWARE | Se completan las 41 entradas de `.init_array`; los punteros Android kuser usados por Protobuf ya se parchean. |
| VitaGL/EGL | RENDER VERIFICADO | Logos, `LOADING`, tutorial y menú principal se renderizan en Vita real. El mapeo compacto de nombres de VBO corrigió el truncado de nombres GLES. `eglSwapBuffers` no aparece como cuello de botella inmediato. |
| Audio | FIX VERIFICADO EN HARDWARE | OpenSL ES reproduce audio durante gameplay y el bloqueo observado previamente en `IBufferQueue_Clear` dejó de impedir avanzar por el tutorial tras aplicar el timeout acotado. Queda pendiente optimizar/validar semántica a largo plazo, pero el bloqueo principal está superado. |
| Input | TÁCTIL VERIFICADO; FÍSICO PENDIENTE | La cola de input funciona y el tutorial responde al táctil. El crash previo de MotionAPIV14 fue superado. Mapping físico completo aún no está confirmado durante gameplay. |
| Assets | TUTORIAL/MENÚ VERIFICADOS; REENTRADA PENDIENTE | El primer ingreso al tutorial carga suficientes recursos y permite jugar. Al volver a entrar desde el menú, el usuario reporta imágenes/texturas ausentes antes de un crash; hace falta revisar el log/dump de esa ejecución antes de concluir la causa. |
| Filesystem | VERIFICADO | Base `ux0:data/zombieshooter/` con `libzombie_shooter.so` y `assets/`; NativeActivity usa esa base para rutas internas/externas/OBB. |
| Build Debug | BUILD VERIFIED | Se genera en `build-session-debug/zombie_shooter.vpk` con SoftFP y símbolos/diagnóstico. |
| Build Release | BUILD VERIFIED | Se genera en `build-session-release/zombie_shooter.vpk` con SoftFP. |
| VPK | VPK VERIFIED | El proyecto genera `eboot.bin`, `param.sfo` y LiveArea con Title ID `ZOMB00001`. |
| Vita3K | NO USADO | El flujo de este port se valida en PS Vita real. |
| Vita real | TUTORIAL + MENÚ VERIFICADOS | Boot, logos, loading, gameplay inicial, audio, touch y retorno al menú están confirmados. Reentrada al tutorial presenta recursos ausentes y crash pendiente de análisis. |
| Boot / Render / Menú / Gameplay | GAMEPLAY INICIAL VERIFICADO | Ya existe primer frame, tutorial navegable y menú principal. Aún no se considera jugable por 1–7 FPS y el crash de reentrada. |
| Rendimiento | INVESTIGACIÓN ABIERTA | 1–7 FPS en tutorial. Swap ~0,2 ms; el cuello principal está fuera de `eglSwapBuffers`. Se debe separar coste de CPU/engine, logging, I/O, GPU y cualquier espera residual de audio. |

## Cambios técnicos principales

- `source/patch.c`: puente kuser de Android, parches de punteros globales usados por Protobuf e instrumentación dirigida del motor.
- `source/utils/init.c`: ejecución registrada de las 41 entradas de `.init_array`.
- `source/reimpl/bionic_compat.c/.h`: compatibilidad Bionic/FORTIFY y wrappers requeridos por el SO.
- `source/reimpl/pthr.c/.h`: layout Bionic de rwlock, `pthread_once` y manejo seguro de inicializadores estáticos.
- `source/dynlib.c`: cobertura explícita para los 412 imports del SO.
- `source/java.c`: superficie Java/FalsoJNI requerida por el juego; contexto Activity ya validado en hardware.
- `source/main.c`: NativeActivity/AssetManager/lifecycle válidos y diagnóstico de presents/input.
- `lib/falso_ndk/android/AAssetManager.cpp`: buffering de `.vid` y assets pequeños para evitar agotamiento de handles.
- `source/utils/glutil.c` / `source/reimpl/egl.c`: VitaGL único, contexto EGL persistente, instrumentación de cadence y traducción de nombres GLES compactos.
- `source/utils/logger.c`: un log por ejecución, sync inmediato para errores y batching para trazas normales.
- `lib/opensles_clear/IBufferQueue.c`: variante local de `IBufferQueue_Clear` con timeout acotado de 100 ms; su objetivo principal de evitar el bloqueo indefinido ya fue confirmado en Vita real.
- `CMakeLists.txt`: build SoftFP, vitaGL vendorizada y sustitución únicamente de `IBufferQueue.o` dentro de OpenSL ES.

## Build reproducible

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

## Datos requeridos

Conservar exactamente:

```text
ux0:data/zombieshooter/libzombie_shooter.so
ux0:data/zombieshooter/assets/...
ux0:data/zombieshooter/logs/...
```

No hace falta volver a copiar los datos para cada VPK si el árbol de assets no cambió.

## Requisitos de Vita real

- `kubridge.skprx` v0.3.1 hotfix o posterior bajo `*KERNEL`, normalmente desde `ur0:tai/kubridge.skprx`, seguido de reinicio.
- `libshacccg.suprx` obtenido legalmente con ShaRKBR33D en `ur0:data/libshacccg.suprx` o `ur0:data/external/libshacccg.suprx`.
- No se ha demostrado que iTLS-Enso u otro plugin adicional sea necesario.

## Problemas abiertos y prioridad

1. **Subir FPS con cambios A/B de bajo riesgo**: separar primero coste de logging/Debug y luego eliminar el MSAA 4x que el bridge EGL no solicita.
2. **Analizar el crash de reentrada al tutorial** usando el `log_*.log` y `.psp2dmp` reales que el usuario ya conserva; no aplicar fixes especulativos antes de leerlos.
3. **Perfilar el 1–7 FPS** separando CPU/engine, GPU, asset I/O y esperas residuales; `eglSwapBuffers` por sí solo no es el cuello observado.
4. Implementar sólo si resulta necesario la parte útil de Play Asset Delivery / `commonAssets;fast-follow` y resolver `Wrong AES key length` con evidencia.
5. Confirmar controles físicos, saves y progresión una vez la cadencia de frame sea razonable.

### Próxima medición de rendimiento

Usar una zona reproducible del primer tutorial durante 30–60 segundos. Mantener una build Debug para crashes y preparar una build Perf separada con tracing mínimo. Comparar después MSAA 4x contra `SCE_GXM_MULTISAMPLE_NONE` sin mezclar otros speedhacks.