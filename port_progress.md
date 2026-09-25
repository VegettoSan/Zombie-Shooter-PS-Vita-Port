# Progreso del port

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
