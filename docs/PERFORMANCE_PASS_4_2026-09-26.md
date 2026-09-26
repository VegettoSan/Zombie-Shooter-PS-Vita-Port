# Performance pass 4: búsquedas de assets y coste de gameplay

## Baseline físico del pase 3

Release probado `local-8282fe9-b3b1dfa9b9`; log_0007 SHA-256 `471ea2cbf78ecf156617a7146459396efedc661afc3149a819d1893bf6fe177b`.
MSAA NONE confirmado en log; clocks ARM500/BUS222/GPU222/XBAR166.
Usuario: logos ~28 FPS; Loading inicial ~3 minutos, 2–8 FPS; tutorial 10–12,
~9 con muchos objetos; menú 12–15; nivel 2 carga <=2 segundos, gameplay 8–9.
Se registra como REAL VITA VERIFIED / GAMEPLAY VERIFIED para el recorrido descrito.
No se afirma estabilidad indefinida ni 20–30 FPS.

105 ventanas registradas. Ventanas 2–24: 118,345 s, 633 frames; 2.346 aperturas
exitosas/53,699 s y 4.376 fallidas/33,470 s. Es un tramo de carga inferido por
la secuencia de actividad, no una duración completa del Loading (sin marcadores).
Costes agregados de hilos, no sumarlos a GL como una partición exacta de CPU.
Ejemplos de fallos lentos: menus/img/shop_scroll.png 48,363 ms,
menus/img/character_button_02.png 48,094 ms, vid/1160.vid 57,940 ms.
Callers de apertura: 0x983DB713 / 0x983E38BD.

RGB565 del pase 3 sí funciona: preserved_bytes=0 en todas las ventanas, sin
fallos de asignación. Últimas 4 ventanas: 8,349 FPS, TexSub19,451 ms/frame;
20 opens exitosos/67,051 ms y 20 fallidos/15,038 ms en 20,243 segundos.
Ventanas 70–86: 7,858 FPS, 675 frames/85,904 s; I/O open combinado ~508 ms.
Por tanto los FPS bajos sostenidos no se explican por disco en ese tramo.
Muchos objetos aumentan trabajo de lógica/render; no probar por correlación cuál
función domina. Se mantiene la escena completa y se mide el coste restante.
Shaders: sólo primer reporte (18 enlaces/5,884 s), posteriores cero.

## Cambio funcional: índice de ausencia en carpetas de assets

Nuevo source/utils/asset_index.c/.h, llamado desde AAssetManager_open_perf:
- Indexa vid/, menus/img/ y menus/items/ mediante una enumeración nativa completa
  la primera vez que se consulta cada carpeta. No indexa saves, logs ni música.
- Guarda hashes de nombres ASCII plegados a minúsculas, ordenados para búsqueda
  binaria. Una colisión produce una apertura adicional; nunca una falsa ausencia.
- Sólo un nombre ausente en una enumeración completa evita el fopen inútil.
  Un nombre presente usa EXACTAMENTE AAssetManager_open anterior: mismos bytes,
  buffering, tamaño, seek, reads, ownership y FD/.m4a. No precarga payloads.
- Rutas ambiguas, subcarpetas, Unicode, alias de puntos/espacios, errores de
  Dopen/Dread/Dclose, OOM o >8192 entradas vuelven al camino anterior. Una lista
  parcial jamás se toma como completa. Descriptores de enumeración siempre cerrados.
- Tope 192 KiB (tres carpetas), sin reservas nuevas de VitaGL ni cachés grandes.
  Catálogo local de data/assets: 349+521+401 entradas ASCII admitidas, sin superar
  el tope. La carpeta real de Vita se enumera por sí misma, sin asumir ese catálogo.
- Índice por proceso sobre assets empaquetados de sólo lectura. Si se añaden o
  renombran archivos en esas carpetas durante una sesión vía FTP, reiniciar el juego
  para enumerarlas otra vez. Borrar un archivo listado conserva el fallo del open
  original; ninguna copia de contenido queda cacheada. No modificar assets en vivo.
- Mutex protege construcción/snapshots y readers concurrentes; no cambia prioridades.
  Enumerar tiene un coste inicial, registrado para comparar con el trabajo ahorrado.

[PERF] asset_index cada ~5 s: lookups/present/absent/fallback, builds/failures,
listed/build_us y entries/bytes residentes. Los contadores se reinician al reportar,
los índices completos permanecen para la sesión. asset_open_fail conserva las
llamadas descartadas (más rápidas); no ocultar fallos ni inventar contenido faltante.

## Medición adicional de gameplay, sin recortar contenido

- Una muestra de cada 16 draws GLES aceptados: draw_sample calls/total_us/max_us.
  El contador no se reinicia por ventana; evita tomar sólo el mismo primer draw.
  No son tiempos de todos los draws ni se deben sumar como coste completo del frame.
  No queries ni locks por draw; no logs por objeto. Rechazos de EBO siguen intactos.
- render_thread: ID/status y delta de runClocks nativo, preemptions/affinity/priority.
  Se consulta sólo al reportar; primer delta no válido. No interpretar unidades
  como porcentaje CPU hasta corroborarlas; no sumar espera de otros hilos al render.
- bulk_fill: guest memset >=256 KiB, manteniendo llamada/resultado de memset original.
  Disassembly cleanLockBuffer SO+0x46bcb0 borra width*height*4, buffer actual1480x838.
  Thunk 0x8b8a1c pasa a memset PLT0x8ebc90. En el loader memset YA redirige a
  sceClibMemset; sustituirlo por esa misma función no sería una optimización nueva.
  Se mide antes de justificar otros cambios. No se parchea el SO ni se omite el clear.

## Archivos y validación

CMakeLists añade asset_index.c; perf.c/h, glutil.c, gl_buffers.inc, mem.c/h y
memset import en dynlib.c agregan mediciones. Nuevo test asset_index y stub de
sampling en test GL. Workflow incluye el test de índices; notas de progreso actualizadas.
VitaGL y demás submódulos mantienen exactamente los parches/hashes del pase 3.

- STATICALLY VERIFIED: índice real O0/O3, presencia/case/ausencia, colisiones,
  reutilización, conservador ante paths, lectura/cierre/OOM/capacidad, 4 readers
  concurrentes. Texturas RGBA/RGB565 O0/O3, JNI/GL/logger O0/O3 pasan.
- BUILD VERIFIED: Debug y Release desde limpio, SDK GCC10.3.0 SoftFP; flags
  Release -O3 sin logging pesado, SDK HardFP intacto, prepare_build/hashes OK.
- VPK VERIFIED: ZIP y copias Windows con SHA-256 idéntico.
- SO inmutable: 5e5e2e1bbe86e126c3e2dce5ad97c2e9dc6282305ea7d3e24b49ee4769c5b5b7.
- REAL VITA / GAMEPLAY VERIFIED: baseline pase 3; esta build nueva pendiente.
  No cifra prometida de mejora de carga o FPS. La mejora funcional es de lookup;
  el coste sostenido de gameplay queda por atribuir con la nueva instrumentación.

Nueva build Debug/Release: `local-8282fe9-64dee46461`.
VPK originales: build-session-debug/zombie_shooter.vpk y build-session-release/zombie_shooter.vpk.
Copias: C:\Users\Mortar\Downloads\Zombie-Shooter-Performance-Pass-4.
Trabajo local: sin commit/push/workflow remoto, siguiendo AGENTS.md.
Conservados NativeActivity/JNI, audio/input, resolución960x544 y buffer interno,
MSAA NONE, datos y saves, clocks, heap/pools y escenas/efectos.

## Prueba siguiente

Instalar Release sin borrar datos/saves. Cerrar completamente la sesión anterior
antes de iniciarlo. Mismos clocks y recorrido: cronometrar Loading inicial, tutorial
con pocos/muchos objetos, menú y nivel 2. No editar los assets durante la prueba.
Devolver log completo con el ID nuevo, tiempos de carga y FPS del mismo recorrido.
Comparar asset_index absent/build_us/bytes y open_fail_us, y atribuir gameplay
mediante draw_sample/bulk_fill/render_thread junto a TexSub. Hasta tener hardware
no afirmar que el disco explique los FPS bajos estables ni quitar efectos a ciegas.
