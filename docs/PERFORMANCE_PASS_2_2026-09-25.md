# Performance pass 2: preservación de texturas

## Baseline físico y objetivo

Release `8282fe987449b4d66a69161f7b575f12abc0fcdd`, log_0005.log del usuario.
Logo VitaGL 60 FPS; logos del juego ~11; LOADING 2–4 FPS, ~6 minutos; tutorial
6–7 FPS, ~4 con muchas explosiones. Movimiento, disparos y exploración de varias
zonas sin crash. Conservar lo funcional. Objetivo: 20–30 FPS y carga más corta;
**todavía no verificados** para esta nueva build.

Clocks realmente preservados: arm500/bus222/gpu222/xbar166. PSVshell informa
MEM360/365, VMEM112/112, PHY26/26 MiB; reserva del sistema no equivale a falta de
espacio dentro de los pools VitaGL. No se modifican tamaños ni thresholds.

## Evidencia y atribución

75 ventanas, 385,879 s de intervalos de present registrados. Ese sumatorio no
es una medición de toda la carga: omite el tiempo anterior al primer present y
no hay marcador explícito de fase. La duración de seis minutos viene del usuario.
Últimas 12 ventanas: 5,921 FPS; TexSubImage 83.465 us/frame, 49,42% del tiempo.
Hay una actualización costosa por frame incluso en logos/LOADING, con picos
recurrentes cercanos a 80 ms. No hay glFinish/glFlush, ni Clear/Destroy timeouts.
Audio Clear suma 668 ms y logger sync 701 ms en esas ~61 s; no dominan el frame.
Swap medio ~0,2 ms. El looper espera en otro hilo: no sumar sus ~5 s como stall
del render. I/O del pase 1 omite coste de AAssetManager_open/buffering y seeks.

El tiempo de TexSubImage incluye allocator, preservación del backing anterior
y escritura/conversión. El log previo no permite separar esas partes: no afirmar
que los 83 ms sean todos copia redundante. No todo el frame quedará explicado
al eliminarlos: aun ahorro ideal del coste completo daría aproximadamente 12 FPS
en esas ventanas. Para 20–30 habrá que medir y atacar también lo que quede.

SO intacto: glTexSubImage2D se llama desde OpenGLES::UnLock(bool), SO+0x46bf54,
con RGBA/UNSIGNED_BYTE, offset0/0 y dimensiones del buffer CPU. También se llama
desde TEXTURE::update en SO+0x472a90 y 0x472bac (actualizaciones parciales/completas).
Eso permite elegir una ruta de optimización concreta sin cambiar binarios ni API.

## Cambio funcional acotado

En VitaGL fijado 9c23758, _glTexSubImage2D hace copy-on-write si la textura fue
usada recientemente: asigna otro backing, copia TODO el anterior, retira el viejo
con su política de GC y finalmente sobrescribe el rectángulo solicitado.

El parche preserva todo ese ownership. Para GL_TEXTURE_2D, mip0, un solo mip,
RGBA/UNSIGNED_BYTE → U8U8U8U8_ABGR, pixels no NULL y rectángulo válido/no vacío:
unpack_row_len cero o igual a width (stride de entrada sin casos ambiguos):
se copia sólo el complemento del rectángulo que se va a sobrescribir. Se guardan
filas superiores/inferiores, columnas laterales y padding. Una sustitución completa
alineada necesita cero bytes del backing anterior. La copia/escritura del nuevo
payload sigue intacta. No se escribe en memoria que la GPU pueda estar leyendo.

Mips, formatos con conversión, cubemaps, vacíos y demás casos mantienen el camino
anterior. No se habilita TEXTURES_SPEEDHACK ni otros speedhacks. Fallo de nueva
asignación produce GL_OUT_OF_MEMORY sin liberar/modificar la textura anterior;
antes podía pasarse NULL a memcpy. No hay nueva sincronización GPU ni DMA.

Versionado: patches/vitagl.patch y hashes de textures.c/header nuevo en el lock;
no se cambia la revisión del submódulo. prepare_build.py aplica/verifica el parche.
License VitaGL LGPL-3.0 conservada; fuente modificada y helper se distribuyen como
patch en el repositorio principal, sin incorporar binarios de juego.

## Nuevos agregados (cada ~5 s)

- tex_cow: calls/optimized/full_replacements/alloc_failures, alloc_us, preserve_us,
  old_bytes/preserved_bytes y tamaño máximo de backing.
- tex_upload: grupos por caller, calls/total_us/max_us, shape del máximo y formato.
  Hasta siete callers; el octavo grupo recoge overflow. No ocultar calls extra.
- gl compile/link: calls/total_us/max_us para shaders durante carga.
- mem: memoria libre del sistema y libre/total de pools RAM/VRAM/PHY de VitaGL.
  VGL_MEM_SLOW es el enum PHYCONT en esta revisión. Lectura no cambia reservas.
- io asset_open/asset_seek: tiempos completos y errores, además de métricas previas.

No hay logs por draw/upload/asset. Reloj añadido sólo al allocator/copia de COW;
no al draw. Nuevos counters GL tienen el mismo único escritor (render) del pase 1.
Las demás limitaciones de overlap/sampling del pase 1 siguen vigentes.

## Verificación

Host test del helper real O0/O3 compara bytes finales con copy-all + overwrite:
rectángulos completos/parciales, todos los bordes pequeños, NPOT/padding, 960x544,
960x544 sobre 1024x1024 y guard bytes. Comprueba también cuántos bytes se copiaron.
Parche sobre checkout VitaGL limpio debe reproducir los hashes exactos. JNI/GL/
logger siguen pasando; SO hash canónico debe seguir intacto. Builds Debug/Release
SoftFP desde limpio, VPK ZIP y flags Release: resultados al pie una vez completados.

Conservados: JNI, audio y waits100ms, lifecycle/input, assets/game.res/buffering,
FD/.m4a, 960x544, MSAA4x, heap/pools/thresholds y clocks máximos. Los errores de
music\mus02.ogg son archivos no disponibles; no se cambia audio para ocultarlos.

## Prueba siguiente

Mismos datos, PSVshell máximo y mismo recorrido en Release. Cronometrar desde
entrada en LOADING hasta tutorial; verificar imágenes/animaciones, movimiento,
disparos, explosiones y cambios de zona. Devolver un log completo con ID local
nuevo, al menos tres ventanas estables, tex_cow/tex_upload y memoria.

Comparar tex_sub_us/frame, bytes preservados y alloc/preserve_us. Si optimized=0,
la ruta real no cumple las condiciones: los callers/formats/shapes explicarán cuál.
Si baja el coste pero no llega a 20 FPS, la siguiente medición debe atribuir CPU
restante/draw/decodificación; no asumir que GPU o falta de RAM lo explican.

## Resultado de verificación local

- STATICALLY VERIFIED: helper equivalente byte a byte en O0/O3; padding, bordes
  y guard bytes; JNI/GL/logger O0/O3 pasan. Parche sobre archive VitaGL limpio
  reproduce hashes exactos. prepare_build verifica SDK, ABI y fuentes.
- BUILD VERIFIED: Debug y Release completos desde directorios limpios, GCC 10.3.0
  SoftFP. Release loader/FalsoNDK -O3, sin diagnósticos pesados.
- VPK VERIFIED: ambos ZIP íntegros con eboot.bin y metadatos; copia Windows con
  SHA-256 idéntico. Binario Android canónico permanece inmutable.
- REAL VITA VERIFIED / GAMEPLAY VERIFIED: únicamente el baseline 8282fe9.
  Esta nueva build aún necesita hardware; no afirmar mejora de FPS/carga.

ID de ambas builds: `local-8282fe9-cb0ab2d552`.
Salidas locales: `build-session-debug/zombie_shooter.vpk` y
`build-session-release/zombie_shooter.vpk`. Copias para instalar en
`C:\Users\Mortar\Downloads\Zombie-Shooter-Performance-Pass-2`.

Sin commit, push ni ejecución remota en este pase; regla local-only de AGENTS.md.

## Validación física posterior: log_0006

El usuario completó el tutorial a 9–11 FPS, alcanzó el menú (~11 FPS) y abrió
el nivel 2 (~8 FPS); logos ~28 FPS. Loading inicial lento/~4 FPS y nivel 2
con carga más corta. Este pase ya es REAL VITA VERIFIED / GAMEPLAY VERIFIED
para ese recorrido. Evidencia detallada y siguiente cambio: informe del pase 3.
