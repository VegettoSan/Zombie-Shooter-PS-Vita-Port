# Performance pass 1 — 2026-09-25

## Baseline y límites de evidencia

Baseline: `bd2dc1a8bd6067d0508381a5f4a8e1d8748b9969`. HEAD y origin/master
coincidían antes de editar. Respaldo local `backup/before-fps-pass-1-20260925`.
La rama anterior `backup/before-local-restore-20260925` no se borra.
Cambios de submódulos iniciales: parches versionados de FalsoJNI/FalsoNDK,
no cambios inesperados que debieran descartarse. No hubo pull/merge/rebase.

**REAL VITA VERIFIED**, según la prueba física reportada por el usuario:
Release funciona, logos/LOADING/tutorial OK, movimiento/disparos/explosiones y
sonidos OK, respuesta estable durante un buen rato; ~5 FPS sostenidos.
El fix JNI de IDs no nulos y fields inexistentes NULL está confirmado físicamente.
Meta inicial >15 FPS; posterior 20–30 FPS. Ninguna está demostrada para este pase.

## Logging Release

- Sin DEBUG_SOLOADER ni SO_UTIL_VERBOSE; FALSOJNI_DEBUGLEVEL=3.
- ZOMBIE_RELEASE_BUILD=1, ZOMBIE_BUILD_VARIANT="Release" e ID del binario.
- DEBUG/INFO/WARN/SUCCESS/WAIT compilados fuera por macros del port;
  entradas directas de esas prioridades también se descartan en el logger.
  Android INFO/WARN se descartan antes de formatear; FalsoNDK ALOGD/ALOGW
  tienen cuerpos vacíos en Release, evitando formatear mensajes descartados.
- ERROR se guarda en buffer de 8 KiB, sin consola ni sync inmediato por línea.
  Checkpoint al llegar a 64 líneas o >=1 segundo desde el último sync cuando
  llegan mensajes. No hay hilo/timer extra: en silencio se fuerza sync al cerrar
  cada ventana PERF (~5 s). No se promete persistencia de cada error ante un crash.
- FATAL conserva consola y fuerza escritura + sync inmediato, sin rate-limit.
  Android fatal y FalsoNDK fatal se encaminan al nivel fatal correcto.
- Rate-limit de errores: caché de 16 strings completos protegida por el mutex;
  primeros cuatro y después 8/16/32/64... Los mensajes nuevos y truncados no se
  suprimen. Repetidos expulsados de la caché vuelven a emitir sus primeras cuatro.
  No se usa hash con colisiones para decidir supresión. PERF no se suprime.
- Debug conserva console, traces y política crash-safe por warning/error/fatal.
- VitaGL Debug: SOFTFP_ABI=1 TEST=1 LOG_ERRORS=1. Release: SOFTFP_ABI=1;
  sólo se quitan sus diagnósticos de console, manteniendo validación de errores GL.
- `logger_force_sync()` y getters acumulativos incluyen líneas, syncs, tiempo
  de sync, tiempo total de logger (incluye espera por mutex) y repetidos suprimidos.
  No se conoce todavía cuántos ms ahorra realmente esta política en hardware.

## Bridge GL y revisión estática

1. Import estático glBindBuffer → glBindBuffer_soloader. dlsym usa la misma tabla;
   eglGetProcAddress ahora devuelve estos mismos wrappers antes de VitaGL,
   evitando escapar del traductor/profiler por lookup dinámico.
2. Bind resuelve guest→Vita una vez y actualiza current_array_guest/vita y
   current_element_guest/vita. Los caminos existentes usan el contexto GLES
   único del render; no se añade soporte nuevo de VAO/contextos múltiples.
3. DrawElements lee directamente current_element_vita y conserva el rechazo
   de punteros bajos no nulos. Sin glGetIntegerv ni spinlock en cada draw.
4. GetIntegerv de ARRAY_BUFFER_BINDING/ELEMENT_ARRAY_BUFFER_BINDING devuelve
   guest IDs directamente, sin VitaGL ni barrido inverso O(65535).
5. Delete limpia ambos bindings si son el nombre eliminado. El VitaGL fijado
   no desasocia buffers al liberar: el wrapper llama glBindBuffer(target,0)
   para evitar conservar una dirección liberada también en VitaGL.
6. Sólo la asignación de un nuevo guest ID busca un slot libre. Esa búsqueda
   de creación no se elimina ni se ejecuta al consultar bindings/dibujar.
7. IDs guest 1..65535 siguen cabiendo en uint16_t. Las direcciones VitaGL se
   guardan completas como GLuint de 32 bits, nunca se truncan a uint16_t.
8. Las lecturas/escrituras publicadas de tabla y bindings son relaxed de 32 bits;
   creación/eliminación conservan sincronización. No hay atomic RMW diagnóstico
   en draws/binds/gen. Logs Debug primeros N dejan de incrementar tras N.
9. SO canónico intacto; no se toca VitaGL source ni su política de memoria.

Prueba host usa el bridge real contra un mock de nombres-puntero de VitaGL,
incluye eliminación sin unbind implícito, guest uint16_t, implicit binding,
unbind, consulta ajena, rechazo de EBO inválido, agotamiento/reutilización y
10.000 draws sin consultas. Pasa O0/O3. La prueba JNI pasa antes y después.

## Métricas de cada ventana (~5 s)

`eglSwapBuffers_soloader` publica `[PERF]` y fuerza un checkpoint al terminar.
No hay mensajes por frame/draw/read. GL usa contadores simples del render,
sin reloj por draw/bind. Sólo uploads, Finish/Flush y swap se cronometran.

| Canal | Qué buscar |
| --- | --- |
| frame | frames, elapsed_ms, fps_x10, swap_avg_us, swap_max_us, frame_max_us |
| gl | draws, draw_arrays, binds, rejected |
| gl buffer_data / buffer_sub | calls, total_us, max_us de uploads |
| gl tex_image / tex_sub | calls, total_us, max_us de texturas |
| gl finish / flush | calls, total_us, max_us; Finish no se reemplaza por Flush |
| audio clear / destroy | calls, wait_calls, wait_us, max_us_lifetime, timeouts |
| audio output | calls, errors de sceAudioOutOutput |
| io asset_sampled | calls, bytes, timed_calls, measured_us, max_us_lifetime, errors |
| io fread / read | calls, bytes, timed_calls, measured_us, max_us_lifetime, errors |
| waits pollOnce / pollAll | calls, total_us, max_us_lifetime, positive/infinite_timeout_calls, requested_max_ms_lifetime |
| logger | lines, syncs, sync_total_us, suppressed_repeats, total_us |
| clocks (una vez al arrancar) | arm, bus, gpu, xbar efectivos; un error de getter queda visible como valor negativo |

Audio: counters relaxed sólo en llamadas/waits/output; getter acumulativo.
Se mide cada condición esperada y sus ETIMEDOUT, sin cambiar argumentos,
retornos, 100 ms, ownership, cancelación, track lifetime ni hilo de salida.
Los máximos audio/I/O/waits son acumulados de toda la ejecución, etiquetados
`_lifetime`. Totales y counts publicados son deltas de las ventanas. El primer
reporte incluye counters desde boot (también carga): comparar varias ventanas
estables posteriores. Counts/totales de 32 bits permiten resta modular entre
snapshots; no reiniciar contadores de escritores concurrentes.

I/O: lecturas pequeñas de assets se cronometran una de cada 256 y las de >=4 KiB
siempre. measured_us de asset_sampled **no es todo el tiempo de assets** y no se
extrapola automáticamente. fread/read importados se cronometran completos.
Se usan hasta 16 slots de hilo con un solo escritor por slot: registros iniciales
usan CAS, cada llamada usa loads/stores relaxed. Si se supera esa capacidad,
`dropped_thread_calls` lo señala y esa muestra queda fuera. Slots no se reciclan.
Las lecturas internas de FalsoNDK/SDK que no cruzan esos imports no están cubiertas.

Los tiempos de audio y waits pueden pertenecer a otros hilos y solaparse con
render; callbacks dentro de poll pueden ejecutar GL/I/O/logger. No se pueden
restar todos los totales como si fueran una partición exclusiva del frame.
Draws no están cronometrados, ni CPU interna del SO. Un gran residuo no demuestra
por sí solo que todo sea CPU del motor: necesitará muestreo de CPU con atribución.

## Clocks, waits y variables conservadas

Política mínima: leer cada clock; subir sólo un valor positivo menor que
ARM444/BUS222/GPU222/XBAR166. Valores superiores (por ejemplo PSVshell ARM500)
quedan intactos. Si falla un getter, no se escribe un clock basado en un valor
inventado. Se registra una vez el valor leído después, sin afirmar éxito del setter.

FalsoNDK fijado `7dceb3bb34de5c71a3c5b98ed30c57a0eb53c73a`:
ALooper pasa timeoutMillis al poll interno, puede acortarlo por mensajes pendientes;
epoll comprueba actividad esperando intervalos de 4167 us. eventfd bloqueante
lee con pausas de 4167 us; overflow de escritura usa 10000 us. Un timeout positivo
epoll puede esperar el timeout completo aun si hay menos eventos que maxevents:
es candidato si el juego pide uno grande, pero **no se cambia semántica aquí**.
No se demuestra un límite fijo de 200 ms sólo por ese intervalo; el import del SO
es ALooper_pollOnce. Desensamblado del SO: la única llamada directa localizada está en
`android::EventLoop::run`, SO+0x3cc868, y carga r0=-1 en SO+0x3cc85e antes
de llamar a ALooper_pollOnce (PLT 0x8c1e10, GOT 0x942324). Es un wait infinito
por eventos, no un timeout fijo grande que demuestre límite de FPS del render.
Los nuevos counters permiten corroborar los timeouts realmente pedidos.

Conservados: 960x544, MSAA4x, thresholds/memoria/heap, shaders/texturas, audio,
100 ms, asset thresholds/game.res, input, JNI, lifecycle, ALooper, frame limiter.
Sin DRAW/BUFFERS/SAMPLERS/CIRCULAR_POOL speedhacks ni VERTEX_LAYOUT_CACHE.
No se modifica `/usr/local/vitasdk-hardfp` ni el `.so`.

## Verificación y siguiente prueba

- JNI, GL y logger: O0/O3 host. Parche FalsoJNI/FalsoNDK aplicado desde pinned
  limpio produce exactamente los hashes versionados.
- Debug/Release se construyen secuencialmente desde limpio para no compartir
  objetos VitaGL de distintas variantes. Resultados finales: ver actualización al pie.
- Release requiere -O3 -DNDEBUG -mfloat-abi=softfp; check_release_flags.py revisa
  todos los flags.make, incluido FalsoNDK, y rechaza logging/tracing pesado.
- Workflow pasa GITHUB_SHA como BUILD ID. Local usa local-<sha>-<source-hash>
  para distinguir cambios aún sin commit. Sólo dos VPK, ningún tercer Perf.

Instalar el Release nuevo, mismos datos y recorrido. Guardar header BUILD,
clocks y al menos tres ventanas PERF del tutorial con movimiento/disparos/audio.
Comparar fps_x10/10 con ~5 FPS. Si finish_calls/frames es >=1, documentarlo como
candidato inmediato de A/B, sin sustituir Finish todavía. Ver audio wait_us /
wait_calls y timeouts; uploads/swap/logger y timeouts solicitados del looper.
Si sigue cerca de 5 FPS y estos datos no explican el coste, el siguiente objetivo
es perfil de CPU dentro del SO con atribución de hilos/GL, no speedhacks aleatorios.
No afirmar >15/20/30 FPS hasta la siguiente prueba en Vita real.

## Resultado local final

Debug y Release: **BUILD VERIFIED / VPK VERIFIED**, reconstruidos desde limpio
y ambos `unzip -t` sin errores. JNI antes/después y GL/logger: O0/O3 PASS.
Flags Release del loader y FalsoNDK verificados: -O3 -DNDEBUG SoftFP,
FALSOJNI_DEBUGLEVEL=3; sin DEBUG_SOLOADER/SO_UTIL_VERBOSE/tracing pesado.
Hash del SO después: idéntico al canónico. Parches probados desde revisiones limpias.
Outputs: build-session-debug/zombie_shooter.vpk y
build-session-release/zombie_shooter.vpk. El resultado del push y workflow
manual posterior al commit se entrega en el informe final; no anticipa FPS reales.
