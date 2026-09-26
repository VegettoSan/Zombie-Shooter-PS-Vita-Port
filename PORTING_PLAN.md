# Zombie Shooter Vita: mapa confirmado

## Baseline físico pase5: log_0009 y pase6 (2026-09-26)

Usuario completó tutorial/nivel2 con zombies, disparos y explosiones; build
local-35ea6e0-9c21ebbd26 verificada en ese recorrido. Sondas de las cinco fases
funcionan: software49,30ms/frame en ventanas20–48 (~10,05FPS),60,89ms en61–66
(~8,65FPS). Map incluye software/graph: no sumar tiempos. Índice6 carpetas sinfallos.
Pase6 acelera ocho rutinas palette SOFT_DRAW relacionadas con esa fase, mediante
punteros locales y bloquesNEON exactos; filas<32 conservan camino original directo.
No cambia escena/resolución/efectos. Equivalencia contra ARM canónico:792casos por
O0/O3, dispatcher/trampolines/ABI y regresiones pasan. Debug/Release/VPK nuevos
local-35ea6e0-99f6b9bfcf verificados; uso real de rutas rápidas y nuevosFPS pendientes Vita.
Detalle: docs/PERFORMANCE_PASS_6_2026-09-26.md. SO/NativeActivity/SoftFP intactos.


## Baseline físico pase 4: log_0008 (2026-09-26)

Release local-8282fe9-64dee46461: usuario confirma Loading inicial ~2 minutos;
FPS prácticamente iguales (+1 ocasional). Índice3 carpetas/1271 entradas/16 KiB,
0 fallos de construcción. El coste de opens fallidos del tramo inicial bajó;
no se registra mejora sostenida de FPS. Ventanas39–46:8,473FPS, TexSub21,583ms/frame,
opens agregados395ms/40,363s; draws CPU muestreados pequeños, no medición GPU.
Pase5: ampliar índice a tres carpetas con fallos lentos observados y medir cinco
fases del motor con trampolines exactos y guardados. Nueva build local-35ea6e0-9c21ebbd26,
Debug/Release/VPK verificados; ejecución de sondas y FPS nuevos pendientes Vita.
Informe docs/PERFORMANCE_PASS_5_2026-09-26.md; SO/ABI/NativeActivity intactos.


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
Informe: `docs/PERFORMANCE_PASS_2_2026-09-25.md`.
Nueva build: pendiente Vita real; no afirmar 20–30 FPS ni carga menor todavía.


- APK/XAPK: `com.sigmateam.zombieshooter.free`, versión 3.5.3; split ARM `config.armeabi_v7a.apk`. `GameActivity` es la actividad `MAIN`/`LAUNCHER` y hereda de `CommonActivity`, que hereda de `android.app.NativeActivity`. El manifiesto declara `android.app.lib_name=zombie_shooter`, GLES 2.0, orientación horizontal, `minSdk=24` y `targetSdk=35`.
- Arranque Java (JADX): `CommonActivity.onCreate()` llama a `NativeActivity.onCreate()` antes de registrar listeners y configurar la ventana. `onStart()` y `onResume()` también llaman primero a `super`. La carga de `libzombie_shooter.so` corresponde al framework `NativeActivity` mediante la metadata; no hay un `System.loadLibrary("zombie_shooter")` propio ni un `GLSurfaceView.Renderer` del juego.
- SO: SHA-256 `5e5e2e1bbe86e126c3e2dce5ad97c2e9dc6282305ea7d3e24b49ee4769c5b5b7`, idéntico al split. ELF32 ARMv7/Thumb-2 EABI5 soft-float con VFPv3/NEON. Exporta `ANativeActivity_onCreate`; no exporta `JNI_OnLoad`, `android_main` ni `Java_*`. El motor registra `nativeOnActivityResult` por JNI y entra en `android::ApplicationNative`/`android::EventLoop` desde el ciclo NativeActivity.
- Renderer: imports EGL/GLESv2, `eglSwapBuffers`, clases nativas `ESContext`; VitaGL es el backend actual. Assets: `AAssetManager_open`/`AAsset_*` y ficheros del árbol `assets/` (incluye `game.res`). Audio: OpenSL ES (`slCreateEngine`, buffer queue) y WAV/OGG propios. Input: `AInputQueue`/`ALooper`, eventos táctiles y gamepad. Hilos: `pthread_create` y bucle nativo separado.
- Los ports MetalSyntax comprobados (Shadow Guardian, Dungeon Hunter 2, Sacred Odyssey y The Impossible Game) llaman a `JNI_OnLoad` o exports `Java_*`/renderer desde el host; no comparten el arranque NativeActivity de este binario. Se usa la metodología de `psvita-port-toolkit-cli`, sin copiar bootstrap, firmas ni offsets.

## Próximo checkpoint

La Vita real ya supera `LOADING` y muestra un nivel tutorial (`log_0011.log`–`log_0014.log`). `log_0014.log` confirma que el límite de 100 ms de `IBufferQueue_Clear` permite seguir jugando después de los primeros bloqueos de audio. El juego permanece a 4–7 FPS, sin sonido, y termina congelado; el watchdog Debug provoca el dump tras 48 s sin presents. `eglSwapBuffers` tarda sólo ~0,2 ms. El dump sitúa el hilo del juego en `CAudioPlayer_PreDestroy`, a la espera de que el mezclador desligue un track. El checkpoint actual acota también esa espera, registra arranque/salida/error del hilo `OpenSLES Playback` y evita destruir un player que el mezclador podría seguir leyendo. Pendiente prueba física. La causa de la falta de sonido y del rendimiento bajo sigue abierta.

Input: el juego recibe eventos sintéticos de joystick y teclas, pero el usuario informa que no se mueve con los botones/joysticks y L lanza granadas. El log muestra que FalsoJNI no implementa `android.view.InputDevice.getDevice(int)` ni `RegistryEnumerator.enumerateKeys(Activity)`; no se ha demostrado aún qué consulta bloquea la configuración de ejes. Se conserva el mapeo actual hasta verificar la cadena Java/nativa exacta.

La prueba `log_0008.log` confirma que FalsoJNI ya responde verdadero a `IsInstanceOf(activity, android/content/Context)`. La inicialización de Play Asset Delivery avanza hasta `AssetPackManagerFactory.getInstance(Context)`, que FalsoJNI no implementa; devuelve un manager nulo y las consultas a `commonAssets` no obtienen ubicación. Aparecen 1.503 errores `showWait`. `Wrong AES key length` persiste (215 veces); su causa sigue abierta.

`log_0009.log` confirmó que `vid/empty.vid` abría y luego dejaba de abrir con 59 assets abiertos (50 `.vid`). El buffer de `.vid` resolvió ese punto en `log_0010.log`; el buffer ampliado a assets de hasta 256 KiB permitió llegar al tutorial en `log_0011.log`. El checkpoint actual mide presents, tiempo de swap y cola de input para localizar los 5 FPS y la congelación antes de cambiar el renderer o el control.

Además siguen faltando 396 rutas solicitadas de `commonAssets;fast-follow`; los 495 archivos declarados no aparecen en ninguno de los 20 APK del XAPK ni en `data/assets`. `config.es.apk` sólo aporta recursos Android de idioma. El origen del contenido fast-follow aún no está demostrado. Véase `docs/ASSET_LAYOUT.md`.
