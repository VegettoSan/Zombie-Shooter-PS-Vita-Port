# Performance pass 3: MSAA NONE y texturas RGB565

## Evidencia del baseline físico

Build probada: `local-8282fe9-cb0ab2d552`, log_0006.log.
SHA-256 del log: `706e785e65957cccfd13229009e9dac1d72e1f3be6ea61927ff743d8edc6437e`.
El usuario confirma logos ~28 FPS; tutorial 9–11 FPS completado, menú ~11,
nivel 2 ~8. Loading inicial largo (~4 FPS); segunda carga más corta.
Clocks 500/222/222/166. PSVshell mantiene MEM360/365, VMEM112/112, PHY26/26 MiB.

103 ventanas PERF. La subida grande desde OpenGLES::UnLock, caller 0x9846BF59,
es RGBA/UBYTE, 1480x838: ~19 ms/frame, COW sin preservación de bytes anteriores.
Es un buffer interno del juego; la salida física sigue 960x544, sin downsampling.
En las últimas dos ventanas, 79 frames/10,092 s = 7,828 FPS, TexSub 28,12 ms/frame.
La otra subida desde TEXTURE::update, caller 0x98472BB1, hace cinco updates/frame
RGB/UNSIGNED_SHORT_5_6_5 de 256x256; copia anterior de 128 KiB por update.
Esas dos ventanas registran 611.906 us de preservación, ~7,75 ms/frame, y
51.773.440 bytes preservados. No confundir todo el TexSub con esa copia.

Shaders: primer reporte acumula 36 compile/18 link, 5.863.071 us en link.
Incluye trabajo anterior al primer present, así que NO compararlo como porcentaje
de la primera ventana de 5 s. Todas las ventanas posteriores tienen cero compile/link.
Puede explicar segundos de arranque, pero no la demora larga de Loading registrada.
No se añade caché de shaders basada en la hipótesis de minutos de compilación.

Ventanas 2–24 (intervalo inicial de carga inferido; sin marcadores exactos de fase):
119,470 s; 6.728 asset opens, 4.379 fallidos, 87,677 s acumulados en open.
Son agregados de hilos; no sumarlos a GL ni afirmar que toda esa duración es CPU
bloqueando render. AAssetManager_open incluye fopen, seeks/tamaño y buffering.
El log no permite separar cuál domina ni identificar rutas repetidas: se instrumenta.
No se aplica caché negativa permanente, ni se ocultan errores/archivos faltantes.

Pools libres mínimos registrados: RAM3130 KiB, VRAM8598 KiB, PHY7125 KiB.
Reservas PSvshell completas no equivalen a pools internos agotados.
No fallos de nueva asignación COW, no glFinish/Flush, ni errores de salida de audio.

## Cambios de este pase

1. `source/utils/glutil.c`: vglInitExtended en 960x544, SCE_GXM_MULTISAMPLE_NONE.
   Nuevo [PERF] render confirma msaa=none. Es la reducción de calidad autorizada;
   bordes pueden mostrar más aliasing. Heap/pools/clocks permanecen iguales.
2. `patches/vitagl.patch` y lock: misma preservación segura del pase 2 ampliada a
   GL_RGB/GL_UNSIGNED_SHORT_5_6_5 → SCE_GXM_TEXTURE_FORMAT_U5U6U5_RGB, mip0 único,
   rectángulo válido, unpack_row_len cero o width. Helper usa bpp2 o bpp4.
   Conserva top/bottom/left/right/padding. Actualización parcial preserva sólo el
   complemento; completa alineada no copia backing antiguo. Payload upload y
   propiedad/GC de GPU siguen intactos; demás formatos/mips mantienen fallback.
   `tex_cow rgb565` permite confirmar elegibilidad real sin asumir destino por bytes.
3. `source/utils/perf.c`: asset_open_ok/asset_open_fail, sumas separadas. Son
   subdivisiones de asset_open, NO costes adicionales que deban sumarse con él.
   Máximo éxito y fallo >=10 ms por ventana, con caller/mode/path (191 bytes,
   truncated explícito). Lock breve sólo para snapshots, nunca alrededor de I/O/log.
   Dos líneas como máximo cada ~5 s; ningún log por intento. Return/errno preservados.
4. `tests/texture_update_regression.c`: equivalencia byte por byte para RGBA y
   RGB565, tamaños pequeños con bordes/NPOT, 256x256, 960x544, parciales y guard bytes.
   Notas de progreso/baseline actualizadas. JNI/NativeActivity/audio/input/FD/.m4a/
   assets/game.res/buffering/resolución interna no se modifican.

## Verificación local

- STATICALLY VERIFIED: pruebas reales del helper O0/O3 pasan para bpp2/bpp4;
  JNI/GL/logger O0/O3 pasan. Parche en archive VitaGL limpio reproduce hashes exactos.
- BUILD VERIFIED: Debug y Release limpios, SDK GCC10.3.0 SoftFP; Release -O3,
  sin trazas pesadas. SDK HardFP intacto. prepare_build verifica pin/hashes/ABI.
- VPK VERIFIED: ambos ZIP completos; copias Windows con SHA-256 idéntico.
- SO inmutable SHA-256: 5e5e2e1bbe86e126c3e2dce5ad97c2e9dc6282305ea7d3e24b49ee4769c5b5b7.
- REAL VITA VERIFIED / GAMEPLAY VERIFIED: baseline pase 2, no esta build nueva.
  Sin cifra prometida de FPS o tiempo de carga. Dos cambios funcionales: MSAA
  explícitamente pedido y extensión de la misma copia evitable medida. El próximo
  log separa rgb565/bytes/coste y registra la configuración de MSAA.

ID nuevo Debug/Release: `local-8282fe9-b3b1dfa9b9`.
Salidas: build-session-debug/zombie_shooter.vpk y build-session-release/zombie_shooter.vpk.
Copias: C:\Users\Mortar\Downloads\Zombie-Shooter-Performance-Pass-3.
Trabajo local, sin commit/push/workflow remoto, según AGENTS.md.

## Prueba física siguiente

Instalar Release sin borrar datos ni saves. Mismos clocks y recorrido: logos,
Loading inicial cronometrado, tutorial con movimiento/disparos/explosiones, menú,
nivel 2 y segunda carga cronometrada. Comprobar colores/texturas y ausencia de
corrupción, audio e input. Devolver log completo del ID nuevo, FPS de tutorial/nivel 2
sin pausas y tiempos de ambas cargas. El header [PERF] render debe indicar msaa=none.

Comparar tex_cow rgb565/preserved_bytes/preserve_us, tex_upload por caller y
asset_open_ok/fail con rutas lentas. Si CPU restante sigue dominando, medirlo antes
de parches en funciones internas del SO; no sustituir la lógica funcional por speedhacks.

## Prueba física posterior log_0007

MSAA NONE/RGB565 probados en hardware: tutorial10–12 FPS, menú12–15 y nivel2
8–9 FPS, carga inicial ~3min, carga nivel2 <=2s. Preservación RGB565 sin copias
antiguas confirmada. Gameplay/carga siguen por mejorar; informe del pase4.
