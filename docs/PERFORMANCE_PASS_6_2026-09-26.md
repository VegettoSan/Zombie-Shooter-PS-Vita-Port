# Performance pass 6: acelerar el rasterizador por software

## Evidencia de Vita real, pase 5

Release local-35ea6e0-9c21ebbd26, log_0009 SHA-256 `573ea605459a626a472907f409095c2a3e4cceb24177b598754693e1b84c7b8f`.
El usuario completó tutorial y nivel2 disparando, matando zombies y usando
explosiones. Las cinco sondas de fase tienen llamadas/timing en gameplay:
REAL VITA VERIFIED / GAMEPLAY VERIFIED para ese recorrido, no estabilidad ilimitada.
960x544, MSAA NONE, ARM500/BUS222/GPU222/XBAR166. 66 ventanas, 338,809 s,
2912 frames. Índice6 carpetas/1786 entradas/21 KiB/0 fallos; 449,241ms de
construcción, 947 ausencias confirmadas descartadas. El usuario no indicó
nuevos FPS ni tiempo de Loading: no inventar una mejora del pase5.

Ventanas20–48: 1473 frames/146,582s =10,049FPS; software49,30ms/frame,
graph26,96ms, TexSub18,58ms y map97,26ms. Ventanas61–66:263frames/30,404s
=8,650FPS; software60,89ms, graph20,17ms, TexSub17,04ms y map114,37ms.
MAP::tact llama softwareTact y GRAPH::Tact (SO+0x437580 /0x43758c), y éstos
incluyen operaciones GL: tiempos inclusivos, NO sumarlos como fases disjuntas.
La fase software es costosa; aún no se midió qué porcentaje corresponde a
cada rutina privada. El nuevo contador verificará uso/cobertura de la familia
optimizada y el tiempo de software antes/después en el próximo log.

## Cambio funcional de gameplay

Nuevo source/utils/raster_palette.c/.h: adaptación de ocho métodos de la
familia VID_SURFACE::pallete_*SOFT_DRAW*. Sólo cálculo de píxeles y Z;
no recorta objetos, sombras, explosiones, lógica, mapas ni resolución.
Métodos normales/FLAT, blended/OPAQUE y con/sin escritura Z se mantienen distintos.
Offsets canónicos: 0x51c3f2,0x51c334,0x51c588,0x51c4d4,0x51c6ac,0x51c64e,
0x51c76c,0x51c716. ABI: this, referencias dst/palette/Z/sourceZ/source,
step y count; count octavo argumento en sp+12. FLAT lee depth signed en this+1188.
Se comprobó el ABI ejecutando las rutinas ARM originales con los mismos argumentos.

Antes: cada píxel carga/escribe repetidamente las cuatro referencias a punteros.
Ahora, sólo tras probar ausencia de alias, se trabaja con punteros locales y se
actualizan referencias al final. Fuente/Z/colores/punteros conservan el mismo estado.
NEON procesa grupos de ocho totalmente visibles; completamente ocultos avanzan
sin acceder a paleta/RGBA. Grupos mixtos y cola usan cálculo escalar exacto.
No redondear distinto ni usar división por255: blended usa alpha y (256-alpha),
división entera por256, alpha final255. OPAQUE conserva los32bits de la paleta.
Comparación Z normal unsigned16, FLAT signed32 con rango seguro0..65535;
igualdad visible. Escritura Z y avance de sourceZ conservan cada variante.

Guardas: step1, alineación, count<=16384, referencias distintas/alineadas/compactas,
intervalos sin overflow ni solapamiento con escrituras, paleta/fuentes intactas.
Aliasing de buffers de sólo lectura puede ser admisible (Z compartido sin write),
comprobado contra ARM original. Strides/alias/rangos no admitidos usan el original.
Referencias demasiado separadas (>256bytes) usan el original conservadoramente.
No hay cachés de payloads, nuevos pools de GPU, cambios de prioridad o threads.

Dispatcher ARM de24bytes lee count sin alterar argumentos/SP/LR; filas<32 van
DIRECTAS al original, sin guardas/counters/clock por fila. Filas>=32 prueban el
camino nuevo; si no cumple contrato, llaman su trampolín original. Cada stub y
trampolín usa la arena ejecutable existente. Prólogose8/12bytes PC independientes,
con instrucción completa, comprobados byte a byte antes de instalar; offsets,
símbolos o espacio incorrectos deshabilitan el hook. Se manejan explícitamente
entradas Thumb a media palabra, conservando los bytes que hook_thumb sustituye.
Sólo flush/escritura de instrucciones al boot. Disco demo/.so inmutable.

[PERF] palette mode0..7: calls (sólo filas>=32), fast_rows/fast_pixels,
vector_pixels, rejected_block_pixels, sampled_calls/us, max_us_lifetime.
Muestreo1/256 por modo; acumuladores atómicos, reporte cada~5s. No tiempos de
GPU ni extrapolación exacta de CPU total. Software/graph/map siguen medidos.

## Verificación significativa

Se ejecutan las funciones ARM reales de la SO inmutable en Unicorn para
compararlas con los kernels SoftFP compilados, O0/O3, escalar y NEON.
792 casos por optimización (8modos, longitudes8..256/colas, visibilidad total,
oculta/mixta, alpha0/1/127/128/254/255, igualdad y límites Z). Resultado:
bytes RGB/Z/paleta/fuentes/guards y referencias finales idénticos; SP y registros
callee-saved r4–r11/d8–d15 intactos. Guardas rechazan stride, alias, longitudes
negativas/cortas/grandes, mala alineación y FLAT fuera de rango sin mutar datos.
Se ejecutan los8 trampolines originales y el dispatcher REAL emitido por C:
72casos cortos/largos, ARM/Thumb y forwarding stack pasan en O0/O3.
Esto es prueba de funciones ARM aisladas, NO ejecutar el juego ni validar Vita.

Instrucciones del kernel O3, ejemplo modo0/128píxeles: totalmente visibles
7568→2338; ocultos2576→690; mixtos5345→4396. Otros patrones mixtos pueden costar
más instrucciones (p.ej. algunos modos opaque). El contador no mide ciclos
Vita, caches, wrapper/counters o FPS; no convertirlo en una promesa de mejora.
Debug O0 es diagnóstico; usar Release para comparar rendimiento.

- STATICALLY VERIFIED: equivalencia ARM O0/O3, prologues/trampolines/dispatch,
  RGBA/RGB565, índices, sondas, JNI/GL/logger O0/O3, git diff --check.
- BUILD VERIFIED: Debug/Release limpias, GCC10.3.0 SoftFP, lock SDK29k y hashes
  de submódulos verificados. Código Release contiene vld4/vst4/vmul/vmla NEON.
  HardFP no modificado; sin nuevos warnings del cambio.
- VPK VERIFIED: ambos ZIP/eboot/copias SHA idénticos.
- REAL VITA / GAMEPLAY VERIFIED: baseline pase5; cambio nuevo pendiente prueba.
- SO SHA:5e5e2e1bbe86e126c3e2dce5ad97c2e9dc6282305ea7d3e24b49ee4769c5b5b7.
- Tests nuevos reproducibles en tests/raster_palette_arm.c,
  raster_palette_arm_regression.py y run_raster_palette_regression.sh.
  Dependencias de pruebas Unicorn2.1.4/pyelftools0.32 se usaron sólo en carpeta
  temporal Windows; workflow local incorpora el test futuro. No se ejecutó Actions.
- CMake/patch/perf registran módulo/instalación/reporte; notas actualizadas.
- Se mantiene NativeActivity/JNI/audio/input y la metodología de
  docs/METALSYNTAX_PORTING_GUIDE.md; no referencia externa nueva ni offsets ajenos.
- Trabajo LOCAL: sin commit/push/fetch/pull ni operaciones remotas.

Build nueva `local-35ea6e0-99f6b9bfcf`. VPKs, ELF sin strip, flags, logs de build y comparación
ARM incluidos en Downloads/Zombie-Shooter-Performance-Pass-6.

## Test físico siguiente

Instalar Release, arrancar de cero y repetir tutorial+nivel2. Permanecer
60–90s en zonas comparables y30s disparando/explosiones. Anotar FPS habituales
mínimos y Loading; comprobar color/transparencia/objetos superpuestos. Devolver
el nuevo log. Si hay crash nuevo, también .psp2dmp (ELF exactos incluidos).
El siguiente log debe mostrar cobertura palette fast/vector y comparar software;
no se anuncia como alcanzados20–30FPS antes del test.
