# Estado del port de Zombie Shooter para PS Vita

Actualizado: 2026-09-25. Todo el trabajo y los artefactos permanecen locales. El mapa de evidencia APK/JADX/SO está en `PORTING_PLAN.md`.

## Iteración de hardware actual

- **Current boot stage:** `log_0010.log` muestra imágenes detrás de `LOADING`, pero la carga no termina y no hay crash.
- **Bloqueo observado:** el buffer de `.vid` permitió abrir `vid/115.vid` y `menus/main.men`. Con 58 streams no `.vid` activos, fallan PNG del menú presentes en los datos y abiertos antes; el límite de handles reaparece.
- **Cambio de esta iteración:** además de los `.vid`, se almacenan en memoria los assets de hasta 256 KiB; los grandes no `.vid` siguen en streaming. Se actualizó el parche del submódulo para la subida manual.
- **Siguiente prueba:** instalar el VPK Debug nuevo sin volver a copiar datos; comprobar si abren `menus/img/2555_07.png` y `2544.png`, y si desaparece `LOADING`.

## Estado verificable

| Área | Estado | Evidencia / límite |
| --- | --- | --- |
| Arquitectura Android | VERIFICADO | ELF32 ARM, EABI5, ARMv7/Thumb-2. Manifiesto: minSdk 24, targetSdk 35; el valor 24 del puente es una elección de compatibilidad. |
| SO utilizado | VERIFICADO | `data/libzombie_shooter.so`, 9,773,412 bytes, SHA-256 `5e5e2e1bbe86e126c3e2dce5ad97c2e9dc6282305ea7d3e24b49ee4769c5b5b7`. Coincide con el split APK ARM. |
| ABI | VERIFICADO | Tanto el SO Android como `build-release/so_loader` declaran EABI5 soft-float. Toolchain: `/usr/local/vitasdk`, GCC 10.3.0, `-mfloat-abi=softfp`. No se usó `/usr/local/vitasdk-hardfp`. |
| Entrypoint | VERIFICADO | `ANativeActivity_onCreate` (0x005296a9 en el SO). Manifest: `GameActivity` es `MAIN`/`LAUNCHER`, hereda de `CommonActivity` → `android.app.NativeActivity` y declara `android.app.lib_name=zombie_shooter`; JADX confirma que `onCreate/onStart/onResume` llaman a `super`. No hay `JNI_OnLoad`, `android_main` ni `Java_*` exportados. |
| JNI | CONTEXT VERIFICADO; PLAY ASSET DELIVERY PENDIENTE | `log_0009.log` confirma `IsInstanceOf(activity, Context): true`. `AssetPackManagerFactory.getInstance(Context)` sigue ausente; `Wrong AES key length` persiste. |
| Imports | COBERTURA ESTÁTICA VERIFICADA | 412 símbolos undefined únicos del SO; 412/412 tienen entrada explícita en `dynlib.c`. Se añadieron wrappers Bionic/FORTIFY, red, señales, tiempo, eventfd/looper/input, pthread rwlock y símbolos de streams. La cobertura no demuestra semántica perfecta de todas las rutas. |
| Constructores | VERIFICADO EN HARDWARE | Los constructores Protobuf 16/17 usaban punteros Android kuser `0xffff0fa0`/`0xffff0fc0` almacenados en `.data`; ahora se parchean ambos símbolos exportados. La ejecución real completa las 41 entradas de `.init_array`. |
| VitaGL/EGL | LOGOS VERIFICADOS; MENÚ PENDIENTE | VitaGL dibuja los logos Sigma y del juego, y `LOADING`; después queda negro. El mapeo de nombres GLES compactos se verificó en hardware. |
| Audio | ENLAZADO, NO PROBADO | OpenSL ES de VitaSDK y codecs requeridos se enlazan; `slCreateEngine` y los IID usados están resueltos. |
| Input | COLA CREADA; CRASH DE JOYSTICK SUPERADO EN HARDWARE | `onInputQueueCreated` retorna correctamente. El dump de `log_0002` localiza un puntero nulo de MotionAPIV14 en un evento joystick. La resolución dinámica corregida pasó la prueba `log_0003`; el mapping físico durante gameplay aún requiere prueba. |
| Assets | VID VERIFICADOS; PNG PENDIENTES | `log_0010.log` lee más de 800 `.vid` con buffer y alcanza `menus/main.men`; después falla al reabrir PNG del menú con 58 streams no `.vid` activos. `commonAssets` sigue ausente. |
| Filesystem | VERIFICADO ESTÁTICAMENTE | Ruta central: `ux0:data/zombieshooter/`; contiene `libzombie_shooter.so` y `assets/`. `ANativeActivity` usa esta ruta como internal/external/OBB y un `AAssetManager` no nulo. |
| Build Debug | BUILD VERIFIED | `build-session-debug/zombie_shooter.vpk`; build SoftFP y ZIP íntegro; SHA-256 `bcdbc3b0ef82eb9b75357bdde064fa5b50638a627208ea379c472c7052cf4fd8`. |
| Build Release | BUILD VERIFIED | `build-session-release/zombie_shooter.vpk`; build SoftFP y ZIP íntegro; SHA-256 `4f44fb2632123ca4f098eeadc4ec0800f2935ff9185d935a8b8e8503aa7e7ed1`. |
| VPK | VPK VERIFIED | Ambos VPK pasan `unzip -t`; contienen `eboot.bin`, `param.sfo` y LiveArea. Título `ZOMB00001`. |
| Vita3K | NO USADO | Esta iteración está destinada exclusivamente a PS Vita real. |
| Vita real | LOADING CON IMÁGENES; MENÚ PENDIENTE | `log_0010.log` confirma el buffer de `.vid` y un nuevo límite de streams PNG. El buffer de assets pequeños sólo está verificado por build. |
| Boot / Render / Menú / Gameplay | LOGOS Y LOADING VERIFICADOS; MENÚ Y JUEGO PENDIENTES | Loader, VitaGL y NativeActivity avanzan; todavía no hay menú ni gameplay confirmados. |

## Cambios técnicos principales

- `source/patch.c`: puente kuser de Android, parche directo de los punteros globales de Protobuf e instrumentación no invasiva de la ruta de arranque y de cada componente del motor. La dirección virtual `vtable+28` del diagnóstico se revalidó en `libzombie_shooter.so`.
- `source/utils/init.c`: ejecución registrada de las 41 entradas de `.init_array`, sin el bypass temporal desde el índice 16.
- `source/reimpl/bionic_compat.c/.h`: compatibilidad para imports Bionic y FORTIFY; los servicios no disponibles fallan de forma explícita en vez de simular datos válidos.
- `source/reimpl/pthr.c/.h`: layout Bionic de rwlock, corrección de `pthread_once` y destrucción segura de mutex/condiciones con inicialización estática Bionic.
- `source/dynlib.c`: tabla completa para los 412 imports requeridos.
- `source/java.c`: métodos Java, incluido el `quit()V` requerido por `onStartApplication()`, class loader falso y versión de Android coherentes con APK 3.5.3.
- `lib/falso_jni/FalsoJNI.c`: `PopLocalFrame(result)` conserva y devuelve `result`, conforme al contrato JNI usado por `jnipp::Environment::findClass_safe`.
- `source/main.c`: `ANativeActivity`/AssetManager válidos, lifecycle inmediato y espera final sin doble presentación de frames.
- `lib/falso_ndk/android/ANativeActivity.cpp`: `ANativeActivity_finish` conserva los objetos propiedad del framework y registra la solicitud en vez de provocar use-after-free.
- `lib/falso_ndk/android/AAssetManager.cpp`: reduce el log de lecturas diminutas a hitos cada 16 KiB y registra el total al cerrar.
- `source/utils/glutil.c` y `source/reimpl/egl.c`: inicialización VitaGL única y contexto EGL persistente.
- `source/utils/logger.c`: un log independiente por ejecución en `ux0:data/zombieshooter/logs/log_NNNN.log`, sincronizado tras cada línea crítica; fallback en `log_fallback.txt` si no puede abrirse `logs/`.
- `lib/falso_ndk/FalsoNDK_Utils.cpp`: se respeta el contrato `noreturn` después de abortar.
- `CMakeLists.txt`: incluye la compatibilidad Bionic y trazas FalsoJNI para Debug.

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

No es necesario eliminar otros directorios de build. El VPK final queda en:

```text
~/Zombie-Shooter-PS-Vita-Port/build-session-release/zombie_shooter.vpk
```

## Instalación y datos requeridos

1. Para diagnóstico, instalar `build-session-debug/zombie_shooter.vpk`.
2. Copiar el contenido completo de `data/` a `ux0:data/zombieshooter/`, conservando exactamente:

```text
ux0:data/zombieshooter/libzombie_shooter.so
ux0:data/zombieshooter/assets/...
```

Son 2,433 archivos y 106,804,517 bytes en el árbol local actual.

## Requisitos de una Vita real

- `kubridge.skprx` v0.3.1 hotfix o posterior. Colocarlo normalmente en `ur0:tai/` y añadir `ur0:tai/kubridge.skprx` bajo `*KERNEL` en `ur0:tai/config.txt`; reiniciar la consola. El loader rechaza expresamente v0.1, v0.2 y v0.3 conocidos.
- `libshacccg.suprx`, instalado legalmente con ShaRKBR33D, en `ur0:data/libshacccg.suprx` (también se acepta `ur0:data/external/libshacccg.suprx`).
- No se ha demostrado que iTLS-Enso u otro plugin adicional sea necesario para este port.

## Problemas conocidos y siguiente prueba

`log_0004` y su dump prueban una carrera: `Registry::loadValue` accede al backend antes de que otro hilo termine de construirlo. La espera acotada cubre ese acceso observado. El buffer de índices `0xfc08` detectado en `log_0003` sigue pendiente de análisis con las trazas `[GL]`; en `log_0004` no se alcanzó ese punto. No hay primer frame ni gameplay confirmados.

Instalar **Debug**, ejecutar `ZOMB00001` hasta ver menú, otro crash o 3 minutos de splash/pantalla negra. Devolver el `log_*.log` de índice más alto en `ux0:data/zombieshooter/logs/` y un `.psp2dmp` nuevo si cae. La pregunta concreta es si aparece `[REGISTRY] loadValue waited=... backend=...` y si luego se llega a las líneas `[GL]`.
