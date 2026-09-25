# Estado del port de Zombie Shooter para PS Vita

Actualizado: 2026-09-25. El mapa de evidencia APK/JADX/SO está en `PORTING_PLAN.md` y el historial bug-a-bug en `port_progress.md`.

## Iteración de hardware actual

- **Current boot stage:** Vita real supera los logos y `LOADING`, entra al nivel tutorial y el táctil responde (`log_0011.log`–`log_0013.log`).
- **Problema funcional actual:** el juego corre aproximadamente a **1–7 FPS reales** y, al avanzar en el tutorial, el render puede quedar detenido durante minutos sin un crash espontáneo.
- **Evidencia del bloqueo:** en el dump asociado a `log_0013.log`, el hilo que reproduce `wav/footsteps.wav` queda en `sound::SfxBuffer::play` → `IBufferQueue_Clear` → espera de condición, mientras el hilo `OpenSLES Playback` está dentro de `sceAudioOutOutput`. `eglSwapBuffers` mide aproximadamente 0,2 ms, por lo que el bajo FPS observado no se explica por el swap/present en sí.
- **Cambio de esta iteración:** la build sustituye únicamente `IBufferQueue.o` de OpenSL ES por una variante donde `Clear()` espera como máximo 100 ms. Si el mixer no confirma el vaciado, la petición queda pendiente y se registra `[AUDIO]` en lugar de suspender indefinidamente el hilo del juego.
- **Siguiente prueba física:** instalar el VPK Debug actual, entrar al tutorial, mover al personaje hasta disparar `footsteps.wav` y comprobar si los presents siguen avanzando. Devolver el `log_*.log` más reciente y un `.psp2dmp` nuevo sólo si vuelve a detenerse/crashear.

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
| VitaGL/EGL | RENDER VERIFICADO | Logos, `LOADING` y el nivel tutorial se renderizan en Vita real. El mapeo compacto de nombres de VBO corrigió el truncado de nombres GLES. `eglSwapBuffers` no aparece como cuello de botella inmediato. |
| Audio | BLOQUEO IDENTIFICADO; FIX PENDIENTE DE HARDWARE | OpenSL ES reproduce hasta el tutorial. El dump de `log_0013` identifica una espera indefinida en `IBufferQueue_Clear`; la build actual limita esa espera a 100 ms. |
| Input | TÁCTIL VERIFICADO; FÍSICO PENDIENTE | La cola de input funciona y el tutorial responde al táctil. El crash previo de MotionAPIV14 fue superado. Mapping físico completo aún no está confirmado durante gameplay. |
| Assets | SUFICIENTES PARA TUTORIAL | Buffering de `.vid` y assets pequeños superó el agotamiento de handles y permitió entrar al tutorial. Siguen faltando rutas `commonAssets;fast-follow`. |
| Filesystem | VERIFICADO | Base `ux0:data/zombieshooter/` con `libzombie_shooter.so` y `assets/`; NativeActivity usa esa base para rutas internas/externas/OBB. |
| Build Debug | BUILD VERIFIED | Se genera en `build-session-debug/zombie_shooter.vpk` con SoftFP y símbolos/diagnóstico. |
| Build Release | BUILD VERIFIED | Se genera en `build-session-release/zombie_shooter.vpk` con SoftFP. |
| VPK | VPK VERIFIED | El proyecto genera `eboot.bin`, `param.sfo` y LiveArea con Title ID `ZOMB00001`. |
| Vita3K | NO USADO | El flujo de este port se valida en PS Vita real. |
| Vita real | TUTORIAL VERIFICADO | Boot, logos, loading, entrada al tutorial y touch están confirmados. Rendimiento y estabilidad de audio siguen abiertos. |
| Boot / Render / Menú / Gameplay | GAMEPLAY INICIAL VERIFICADO | Ya existe primer frame y se alcanza un nivel tutorial. Aún no se considera jugable por 1–7 FPS y el bloqueo de audio observado. |
| Rendimiento | INVESTIGACIÓN ABIERTA | 1–7 FPS en tutorial. Swap ~0,2 ms; el cuello principal está fuera de `eglSwapBuffers`. Antes de aplicar speedhacks se debe separar coste de CPU/engine, audio, logging, I/O y GPU. |

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
- `lib/opensles_clear/IBufferQueue.c`: variante local de `IBufferQueue_Clear` con timeout acotado de 100 ms; pendiente validación física.
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

1. **Validar el timeout de OpenSL ES** en hardware y comprobar que `footsteps.wav` ya no detenga el hilo del juego.
2. **Perfilar el 1–7 FPS** sin asumir que el problema es `eglSwapBuffers`: separar CPU/engine, GPU, logging, asset I/O y audio.
3. **Revisar configuración de VitaGL para rendimiento**, especialmente MSAA 4x a 960x544, memoria y flags de Release, mediante pruebas A/B controladas.
4. Implementar sólo si resulta necesario la parte útil de Play Asset Delivery / `commonAssets;fast-follow` y resolver `Wrong AES key length` con evidencia.
5. Confirmar controles físicos, audio continuo, saves y progresión una vez la cadencia de frame sea razonable.

### Siguiente prueba

Instalar **Debug**, entrar al tutorial, mover al personaje hasta reproducir pasos y observar durante al menos 30–60 segundos. El objetivo inmediato es confirmar si aparecen mensajes `[AUDIO] OpenSLES buffer Clear timed out` y, sobre todo, si el contador `[PERF] present` continúa avanzando después del primer `footsteps.wav`.
