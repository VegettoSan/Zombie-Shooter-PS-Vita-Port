# Progreso del port

## Checkpoint actual: crash Release JNI al salir del logo VitaGL

- Hardware: Release de run 12 cae; Debug de run 12 arranca según el usuario.
- Dump `psp2core-1790390763-0x000005349b-eboot.bin.psp2dmp`: PC=0x8100B834 en GetObjectArrayElement, LR=0x983CBB31 en checkPackageCertificate, DFAR=0x776F; índice 0 y base 0x776F.
- Confirmado: signatures desconocido retorna fieldID NULL=0 y se confunde con WINDOW_SERVICE="window". Debug lee longitud 0 en los datos siguientes; Release lee 105 e intenta usar los bytes +4 como puntero.
- Fix mínimo: reservar IDs no NULL en las tablas y devolver NULL para fields object ausentes. Se versiona en falso_jni.patch y su lock; no cambian flags, VitaGL ni el SO.
- Regresión sobre fuentes reales: falla antes; pasa con O0/O3 después, conservando WINDOW_SERVICE, SDK_INT=24 y arrays válidos. Builds Debug/Release y VPK ZIP verificados; falta probar este cambio en Vita.
- Evidencia y desensamblado: `docs/CRASH_RELEASE_JNI_2026-09-25.md`. Siguiente prueba: Release nuevo hasta menú/tutorial; último log y dump si cae.

## Checkpoint actual: segunda espera OpenSL ES durante el tutorial

- Vita real `log_0014.log`: el límite previo de `IBufferQueue_Clear` se activó cuatro veces y permitió continuar moviéndose y disparando. El juego siguió sin sonido, a 4–7 FPS, antes de congelarse. Tras 48 s sin un present, el watchdog Debug generó deliberadamente `psp2core-1790322024-0x0001423379-eboot.bin.psp2dmp`.
- El dump sitúa el hilo del juego en `CAudioPlayer_PreDestroy` → `object_cond_wait` durante la destrucción de un reproductor. En el dump anterior el hilo `OpenSLES Playback` esperaba en `sceAudioOutOutput`; el dump actual no permite afirmar por sí solo por qué dejó de reconocer el track. `eglSwapBuffers` promedia ~0,2 ms, así que no explica los 4–7 FPS.
- Cambio acotado: `CAudioPlayer_PreDestroy` espera como máximo 100 ms. Si el backend ya salió, desliga el track bajo el lock del mezclador; si sigue activo, rehúsa destruir el reproductor para evitar una lectura de memoria liberada. El backend de Vita registra apertura del puerto, inicio/salida del hilo y errores de audio. Las tres modificaciones sustituyen objetos de una copia local de `libOpenSLES.a`; el SDK instalado queda intacto.
- Input observado: la palanca muestra la guía Xbox, los controles probados no mueven al personaje y L lanza granadas. FalsoJNI no resuelve `InputDevice.getDevice(int)` ni `RegistryEnumerator.enumerateKeys(Activity)` en el log. No hay aún evidencia suficiente para reasignar ejes/botones; el mapeo no cambia en este checkpoint.
- Debug y Release compilan con SoftFP y generan VPK. Pendiente Vita real: verificar si el hilo de audio abre el puerto o sale con error, si `CAudioPlayer_PreDestroy` vuelve a bloquearse y si los presents continúan tras mover/disparar. Sonido, velocidad y mando siguen abiertos.

## Checkpoint actual: espera infinita al vaciar la cola OpenSL ES

- Vita real `log_0013.log`: el watchdog Debug causó deliberadamente el dump tras 45 segundos sin un present. Los últimos tres `AAsset_read` de `wav/footsteps.wav` retornaron correctamente. La pausa ocurre después de la lectura, no dentro de ella.
- En `psp2core-1790320198-0x0000fb3453-eboot.bin.psp2dmp`, el hilo que reproduce el efecto está en `sound::SfxBuffer::play` (SO + `0x4d3b0a`) → `IBufferQueue_Clear` → `object_cond_wait`; el hilo `OpenSLES Playback` está dentro de `sceAudioOutOutput`. El hilo del bucle Android espera eventos. La función `IBufferQueue_Clear` del OpenSL ES de VitaSDK esperaba la confirmación del mezclador sin límite.
- Cambio único: sustituir sólo `IBufferQueue.o` del archivo OpenSL ES durante la build. `Clear` espera como máximo 100 ms, deja el pedido de vaciado pendiente para conservar la propiedad de los buffers y registra `[AUDIO]` si expira. La causa por la que `sceAudioOutOutput` no regresa en ese momento aún requiere prueba física.
- Debug y Release compilan con SoftFP; ambos VPK generados. Siguiente prueba: entrar al tutorial, mover el personaje hasta disparar `footsteps.wav` y comprobar si los presents continúan y si aparecen timeouts `[AUDIO]`. Los 5–7 FPS previos siguen sin explicación suficiente; no se afirma que este cambio los mejore.

## Checkpoint actual: congelación del render durante el tutorial

- Vita real `log_0012.log`: `eglSwapBuffers` tarda ~0,2 ms, pero la cadencia real cae a 1–4 FPS. Hay pausas de 29 y 18 segundos que se recuperan. Tras abrir `wav/footsteps.wav`, los presents se detienen en 1154 durante al menos 157 segundos; el hilo lifecycle sigue vivo y la cola de input continúa recibiendo/consumiendo eventos. PSVshell también deja de actualizar su contador. El cuello de botella no está dentro de `eglSwapBuffers`.
- `footsteps.wav` es PCM mono 16 bits a 22.050 Hz. El último progreso de lectura está en 53.150/53.312 bytes, justo al final del chunk `data`, antes de metadatos RIFF. La coincidencia se repite, pero aún no prueba si la lectura, el audio o una llamada GL posterior bloquea el hilo.
- Cambio diagnóstico: registrar las primeras lecturas, final y EOF de ese WAV; medir cuánto tiempo ocupa `sceIoSyncByFd` cada 5 segundos. Si Debug lleva 45 segundos sin un present después de 1.000 frames, escribe una marca `[CRASH]` y provoca un dump deliberado para inspeccionar los PCs de todos los hilos. Release no provoca ese dump.
- Debug y Release compilan con SoftFP. Próxima prueba física: reproducir la congelación con el Debug, devolver `log_0013.log` y el nuevo `.psp2dmp`. El dump debe localizar el hilo de render en el punto real de bloqueo.

## Checkpoint actual: tutorial visible y diagnóstico de rendimiento

- Vita real `log_0011.log`: el juego sale de `LOADING`, muestra un nivel tipo tutorial y el táctil responde. El usuario observa 5–7 FPS y luego una imagen congelada sin crash. El buffer de assets pequeños del checkpoint anterior permitió llegar al juego; aún no confirma una partida fluida.
- El log termina con el hilo lifecycle vivo durante más de un minuto tras la última actividad registrada del motor. No había contador de frames, de modo que aún no se sabe si el render se detuvo o siguió dibujando la misma imagen. Se ven consultas JNI nulas de `commonAssets`, `getPreferences` y `InputDevice.getDevice`, pero ninguna coincide de forma única con el bloqueo.
- Cambio diagnóstico: `eglSwapBuffers` registra cada 5 segundos FPS, tiempo medio/máximo del swap y el intervalo máximo entre frames. El hilo lifecycle registra el total de presents y la edad del último frame. FalsoNDK registra cada 5 segundos la profundidad de la cola de input y sus contadores de entrada/salida cuando hay eventos. Sin cambios en lifecycle, assets ni gráficos.
- Debug y Release compilan con SoftFP. Pendiente Vita real: mover el personaje unos segundos, esperar a que se congele y conservar el log completo. La pregunta es si dejan de aumentar los presents, si `eglSwapBuffers` consume el tiempo de frame o si crece la cola táctil.

## Checkpoint actual: límite de streams durante la carga de menú

- Vita real `log_0010.log`: el buffer de `.vid` funciona; `vid/115.vid` y `menus/main.men` abren y el usuario ve imágenes detrás de `LOADING`. No se repite `showWait`. La carga se detiene después de que `menus/img/2555_00.png` a `_06.png` abren, pero `_07.png` y archivos siguientes fallan aunque están en los datos y `_07.png` abrió antes.
- Justo antes de ese fallo hay 58 streams activos **no `.vid`**: principalmente 27 PNG y 22 `.men`; por ello el límite de handles reaparece en otro tipo de asset. El log termina con heartbeats del hilo lifecycle y sin crash. `commonAssets` y el error AES siguen pendientes, pero no explican la secuencia exacta de fallas de archivos existentes.
- Cambio: mantener en memoria todos los assets de hasta 256 KiB además de los `.vid` de hasta 2 MiB; los archivos grandes no `.vid` continúan en streaming. El subconjunto de assets pequeños del APK suma como máximo 46,2 MiB, y el resto de `.vid` añade menos de 20 MiB antes de duplicados. El buffer existente ya soporta lectura, seek y longitud restante.
- Debug y Release compilan con SoftFP y ambos VPK pasan `unzip -t`. Pendente Vita real: confirmar que `menus/img/2555_07.png`, `_08.png`, `_09.png` y `2544.png` abren, y si desaparece LOADING.

## Checkpoint actual: agotamiento de handles de assets

- Vita real `log_0009.log`: `vid/empty.vid` abre y se lee completo, pero más tarde la misma ruta falla. Justo antes del primer fallo de `vid/115.vid` hay 59 `AAsset` abiertos sin cerrar (50 `.vid`), y desde ese punto no hay más aperturas exitosas; `menus/main.men` también falla aunque existe en el APK y en Vita se copió el mismo árbol. Esto contradice la hipótesis previa de una copia incompleta como explicación principal.
- Causa probable, pendiente de confirmar en Vita: agotamiento de slots de `FILE*`/descriptores por los `.vid` que el motor mantiene abiertos. No se modifica la búsqueda de `commonAssets`, que continúa sin implementación ni archivos locales.
- Cambio en FalsoNDK: cada `.vid` de hasta 2 MiB se carga en memoria al abrir y se cierra de inmediato su `FILE*`; `AAsset_read`, `AAsset_seek` y longitud restante funcionan sobre el buffer. Todos los `.vid` del APK son menores de 2 MiB. Si una apertura aún falla, se registran `errno` y `sceIoGetstat` de forma limitada para distinguir archivo ausente de límite de handles.
- Debug y Release compilan con SoftFP. Próxima prueba: confirmar trazas `[ASSET] buffered .vid`, que `vid/115.vid` y `menus/main.men` abren, y observar si aparece el menú. El mismo ZIP de datos de la iteración anterior sirve.

## Checkpoint actual: rutas de assets faltantes en Vita

- Vita real `log_0008.log`: `IsInstanceOf(activity, android/content/Context): true` confirma el parche anterior. Luego falta `AssetPackManagerFactory.getInstance(Context)` en FalsoJNI; el manager queda nulo y `commonAssets` se consulta repetidamente. Pantalla negra tras `LOADING`, sin crash; 1.503 errores `showWait` y 215 `Wrong AES key length`.
- De 1.256 rutas directas distintas que `AAssetManager_open` no pudo abrir en Vita, 790 existen en el `data/assets` local actual. El APK base y ese árbol local coinciden exactamente en nombres y CRC de sus 2.432 assets. `log_0009.log` mostró que algunas de esas rutas abren antes de dejar de abrirse; la hipótesis de una copia incompleta queda descartada como explicación principal.
- Las 466 rutas restantes no existen localmente; 396 pertenecen al paquete `commonAssets;fast-follow`. `config.es.apk` no trae assets. Los 1.133 archivos de `data/res` son recursos Android, sin lectura nativa observada en este log.
- Cambio de este checkpoint: `scripts/package_vita_data.py` produce `build-session-debug/vita-data.zip` con `zombieshooter/assets/` y `libzombie_shooter.so` para extraer en `ux0:data/`. No se modificó el loader mientras se comprueba la copia de datos. ZIP de 2.433 entradas íntegro; los VPK Debug y Release anteriores permanecen íntegros.
- Prueba siguiente: copiar el ZIP de datos a Vita, reutilizar el VPK Debug y comprobar que `vid/115.vid` y `vid/empty.vid` abren. Si el menú sigue negro, el siguiente límite es Play Asset Delivery y los archivos `commonAssets` ausentes.

## Checkpoint actual: carga superada; prueba de Context para AssetPackManager

- Vita real `log_0007.log`: el juego sale de `LOADING` y queda en negro sin crash; ~5 FPS durante la carga y ~9 FPS después. El log de VitaGL muestra avisos de shaders, sin fallo fatal.
- Bloqueo observado: `AssetPackManager_init failed: ASSET_PACK_INITIALIZATION_FAILED` tras `IsInstanceOf(activity, android/content/Context): false`; siguen 2.808 `BundleManager not initialized` y miles de `SCRIPT Can't find function "showWait"`. La relación con la pantalla negra es plausible, aún no probada.
- Cambio acotado en FalsoJNI: responder verdadero sólo al objeto Activity sintético y a la clase exacta `android/content/Context`. El resto de `IsInstanceOf` mantiene su comportamiento anterior. Esta nueva ruta de AssetPackManager requiere prueba en Vita.
- `Wrong AES key length` persiste 215 veces a pesar de `getContentResolver` y `Settings.Secure.getString(android_id)`. No se considera corregido; `getPreferences` sigue sin implementar y debe investigarse con evidencia si falla la prueba actual.
- `.gitignore` excluye builds, APK/XAPK, `.so`, assets, logs y dumps. Los cambios de los submódulos están reproducidos en `patches/` para la subida manual del repositorio principal; `.gitignore` no controla archivos ya rastreados.
- Debug y Release compilan; VPK generados. Próxima prueba: verificar en el log `IsInstanceOf(activity, android/content/Context): true`, resultado de `AssetPackManager_init` y si aparece menú. Devolver log y dump sólo si hay crash.

## Checkpoint actual: LOADING visible, paquete fast-follow ausente y coste del log

- Vita real `log_0006.log`: aparecen el logo Sigma, el logo del juego y `LOADING`, sin crash. El mapeo GL anterior queda confirmado: `guest=5` enlaza con `vita=0x82D30C38` y no hay `skip draw`.
- `AssetPackManager_init` falla una vez al rechazar el `Context` de FalsoJNI; después `BundleManager not initialized` se repite 594 veces. `bundles.config` declara `commonAssets;fast-follow`: ninguno de los 495 nombres aparece en los 20 APK internos del XAPK ni en `data/assets`. El usuario informa que la instalación Android de ese XAPK funciona, así que esos archivos podrían descargarse al ejecutar o el motor podría avanzar sin ellos. La prueba siguiente debe distinguirlo; no se considera demostrado que su ausencia sea la causa única del atasco.
- También se repite `Wrong AES key length` (92 veces), precedido por `getContentResolver()` y `Settings.Secure.getString(android_id)` ausentes en JNI. Se implementaron esos dos métodos con un identificador hexadecimal estable de 64 bits, como en Android. La prueba debe decir si desaparece el error AES y si avanza `LOADING`; no se alteró el cifrado del SO.
- Rendimiento observado: 2–10 FPS, con 14.158 líneas en unos cuatro minutos. El logger hacía `sceIoSyncByFd` después de cada línea. Cambio acotado: avisos, errores y fallos siguen sincronizando inmediatamente; las líneas normales se sincronizan cada 32. Es una hipótesis de coste I/O, pendiente de medir en Vita.
- Debug y Release compilados con `/usr/local/vitasdk` SoftFP; los dos VPK pasan `unzip -t`. Próxima prueba: comparar velocidad de `LOADING`, confirmar si desaparece `Wrong AES key length` y observar si el motor supera el estado de carga. En paralelo, localizar en la instalación Android funcional el origen de `commonAssets`.

## Checkpoint actual: nombres GLES de buffers de 16 bits

- Vita real `log_0005.log`: no hay crash; el juego pasa el splash de VitaGL pero queda negro. VitaGL genera el VBO de índices `0x82D2FC08`; `opengles::drawBatch_nogamma_VBO` lo vuelve a enlazar como `0xFC08`, y el guard anterior omite repetidamente esos draws.
- Causa confirmada: el tercer parámetro de `drawBatch_nogamma_VBO` en este SO es `unsigned short` (símbolo y desensamblado en SO + `0x0047922c`); en SO + `0x0047941e` se pasa a `settings::ELEMENT_ARRAY_BUFFER`. VitaGL devuelve un puntero de 32 bits como nombre de VBO, que el motor trunca a 16 bits. Android GLES usa nombres numéricos pequeños.
- Cambio: `glGenBuffers` entrega al SO nombres compactos de 1 a 65535; `glBindBuffer` y `glDeleteBuffers` los traducen a los nombres puntero de VitaGL. `glGetIntegerv` traduce los bindings en dirección inversa. Se conserva el guard de `glDrawElements` como diagnóstico ante un nombre inválido nuevo.
- Debug y Release compilan con SoftFP; ambos VPK pasan `unzip -t`. Pendiente prueba en Vita: confirmar en `[GL]` que el binding del draw de SO + `0x0047941e` corresponde al VBO generado, que no se omiten draws y si aparece imagen/menú.
- Siguiente bloqueo observado, aún sin parche: `AssetPackManager_init` falla porque `IsInstanceOf(Context)` de FalsoJNI devuelve falso; el motor repite `BundleManager not initialized` y no encuentra `showWait`. También faltan múltiples assets `.bmp` en el paquete local. La prueba GL delimitará cuánto de la pantalla negra proviene del render y cuánto de los bundles.

## Iteración anterior: carrera de construcción de Registry

- `log_0004.log` termina antes de cualquier traza `[GL]`, al empezar la lectura de `game.cfg`. La pantalla sigue en el splash de VitaGL; no es el mismo punto de fallo que el dump anterior.
- Dump `psp2core-1790308685-0x000022336d-eboot.bin.psp2dmp`: data abort en `core::Registry::loadValue` (SO + `0x003eb11e`). `Registry` en `0x8250e0c8` tiene vtable válida pero `backend` en `self+12` vale cero. La pila de otro hilo llega a SO + `0x003e93a3`, dentro de `Registry::Registry`, justo antes de almacenar ese backend en SO + `0x003e93a8`. La llamada pendiente es `RegistryPrivate::RegistryPrivate`, que registra natives JNI y está escribiendo el log.
- Causa confirmada: el constructor publica el singleton en SO + `0x003e9394` antes de inicializar `self+12`; otro hilo entra a `loadValue` en esa ventana. Es una carrera de construcción, distinta de los errores `getPreferences` que aparecen más tarde en otros runs.
- Cambio: hook del símbolo verificado `Registry::loadValue` que sólo cuando `self+12` es nulo espera hasta 5 segundos por el constructor y registra la espera. Luego ejecuta el método original. Si no se inicializa, registra timeout y deja que el dump delimite la causa. No se cambió el lifecycle ni el binario Android.
- Checkpoint Debug/Release compilado; ambos VPK íntegros. Pregunta para Vita: ¿se registra la espera `[REGISTRY]` y avanza después al primer draw o al siguiente bloqueo?

## Iteración anterior: nombre inválido de buffer de índices en VitaGL

- `log_0003.log` confirma que `dlsym` resolvió `AMotionEvent_getButtonState` y `AMotionEvent_getAxisValue`; el crash anterior de `PC=0` en joystick no se repitió. `ndk_step.txt` permanece en `idle_loop`, lo que no identifica el hilo de render.
- Dump `psp2core-1790308044-0x0000193289-eboot.bin.psp2dmp`: data abort en `glDrawElements+0xaa` (loader `0x8117b73a`), al dereferenciar `cur_vao->index_array_unit=0xfc08`. La llamada del SO llega desde `opengles::drawBatch_nogamma_VBO` con 4 índices `GL_UNSIGNED_SHORT`. VitaGL usa una dirección de `vbo` como nombre de buffer; `0xfc08` no es una dirección Vita válida. El origen de ese valor aún no está demostrado.
- Cambio diagnóstico y protector: wrappers acotados para `glGenBuffers`, `glBindBuffer(GL_ELEMENT_ARRAY_BUFFER)` y `glDrawElements`. Registran las primeras operaciones y cualquier nombre bajo inválido (hasta 16 avisos); sólo se omite un draw cuando su binding de índices es una dirección baja no nula. Se conserva la implementación VitaGL para los casos válidos. Esto puede evitar el crash, pero no demuestra render correcto.
- Checkpoint Debug/Release compilado y VPK verificados. Pregunta para Vita: ¿qué `glGenBuffers` o `glBindBuffer` introduce el nombre inválido y si el juego progresa al omitir ese draw?

## Iteración anterior: puntero nulo de MotionAPIV14 en joystick

- Prueba real `log_0002.log` y `psp2core-1790307190-0x0000d02627-eboot.bin.psp2dmp`: `onStartApplication()` y `core::Application::initialize()` retornan 1; `game.res` y `maps/logo.lgd` llegan al final. El splash da paso a una pantalla negra y luego a un prefetch abort.
- Dump: hilo `pthread` con `PC=0`, `LR=0x983cf1df` (SO + `0x3cf1de`). La llamada de `input::AndroidJoystickControl::handleEvent` en SO + `0x3cf1da` entra a `input::MotionAPIV14::axisValue`; su wrapper termina en `bx r3`, con el puntero `axisValue` del objeto en cero. Es un crash confirmado de input, no un fallo demostrado de VitaGL.
- La versión exacta del SO resuelve `AMotionEvent_getButtonState` y `AMotionEvent_getAxisValue` por `dlsym`. El log registra que el primero falta; el constructor de `MotionAPIV14Impl` borra ambos punteros cuando uno falta. Además, `dlsym_soloader` devolvía la dirección del campo de la tabla en lugar de la dirección de la función.
- Corrección: se implementa/exporta `AMotionEvent_getButtonState` con valor cero para los eventos Motion sintéticos (los botones Vita se emiten como KeyEvent), y `dlsym_soloader` devuelve la dirección real con log breve de resolución. Sin cambios en firmas u offsets del SO.
- Checkpoint: Debug y Release compilados, ambos VPK íntegros. Pendiente prueba en Vita real. Pregunta: ¿se evita el salto a `PC=0` y aparece el primer frame/menú? Si sigue negro o cae, el log y el nuevo dump delimitarán el siguiente bloqueo.

## Iteración anterior: traza por cada byte leído del asset

- Síntoma en Vita real: el hilo ya pasa `onStartApplication()` y `core::Application::initialize()` con resultado 1, pero permanece en el splash mientras lee `assets/maps/logo.lgd`.
- Evidencia: `log_0001.log` tiene 24 362 llamadas `AAsset_read` del mismo handle, sólo 60 095 bytes solicitados de un fichero de 1 729 255 bytes. Cada llamada se registraba y sincronizaba inmediatamente en disco. `ndk_step.txt` queda en `idle_loop`; no hay salida nativa ni crash en el log.
- Causa confirmada: instrumentación excesiva de lecturas minúsculas en `lib/falso_ndk/android/AAssetManager.cpp`; introduce un coste de I/O persistente por byte. También hay errores JNI de preferencias, resolución de contenido y servicios, pero la inicialización del motor retornó éxito; quedan como siguientes hipótesis, sin parches especulativos en este checkpoint.
- Cambio: se registra progreso cada 16 KiB, cierre con bytes leídos y sólo avisos para lecturas inválidas/fallidas. Semántica de `AAsset_read` intacta.
- Estado: corrección compilada, pendiente de nueva prueba en Vita real. Pregunta: ¿termina la lectura de `logo.lgd` y aparece el primer frame o el siguiente bloqueo identificable?
