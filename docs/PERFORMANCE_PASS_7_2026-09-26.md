# Performance pass 7: ruta alpha de VID_SOFTWARE

## Resultado físico del pase6 y evidencia

El usuario confirma FPS similares al pase5 y Loading inicial de casi2min.
log_0010: Release local-35ea6e0-99f6b9bfcf, SHA256
757ac572712e940ec37f150f9100d2a79ff502107f74969f010aeef8cc27bad7.
960x544, MSAA NONE, ARM500/BUS222/GPU222/XBAR166. No afirmar mejora del pase6.
114 ventanas de5s aproximadamente; no hay líneas palette mode en todo el log:
no se observaron llamadas instrumentadas largas (>=32) a esa familia. Esto NO
prueba que filas cortas no se ejecuten ni confirma instalación, porque el estado
se ocultaba en Release. El pase7 hace visible la máscara de instalación.

Ventanas19–48:1598frames/151,673s=10,536FPS; software47,7358ms/frame,
graph23,7043ms, TexSub18,0505ms, map93,5044ms. Ventanas79–96:776frames/
91,647s=8,467FPS; software59,9194ms, graph23,3568ms, TexSub17,6580ms y
map115,785ms. Escenarios inferidos del patrón de fase: las ventanas NO tienen
un marcador de nivel; los recorridos distintos no son benchmark idéntico.
Los tiempos map/software/graph son inclusivos; NO sumarlos como disjuntos.

Primeras17 ventanas (87,722s, incluyen logos y carga, no todo el arranque):
asset_open_ok54,597649s, fail2,494124s; shader_link5,905452s. open incluye
fopen/medir archivo/cargar su buffer/cerrar según AAssetManager, NO es sólo
latencia de llamada fopen. No demostrar que todo ese tiempo sea disco. Los
shaders medidos no explican por sí solos Loading~2min. No cambiar política de
buffers sin distinguir estas etapas. La carga inicial sigue pendiente mejorar.

## Cambio funcional acotado

Se revisó el ARM canónico: VID_SOFTWARE::draw_impl llama
AsmDrawWithAlpha32 por PLT desde0x513ebc y0x513fb8. Es una ruta diferente a
VID_SURFACE::pallete_* del pase6. La fase software sí es costosa en Vita;
la contribución exacta de estas dos rutinas sigue pendiente medición física.
Nuevo source/utils/raster_alpha.c/.h reemplaza sólo los scanlines alpha32:
- drawLineWithAlpha32 (RGBA directo), so+0x470642;
- AsmDrawWithAlpha32 (fuente indexed8/paletaRGBA), so+0x4707b8.

ABI fuente r0, Z readonly r1, destinoRGBA r2, count r3, depth ushort sp0,
paleta sp4 sólo indexed. Misma condición unsigned depth>=Z (igualdad visible),
sin escritura Z. RGB exacto: (source*a+destination*(255-a))>>8; alpha final
=destination_alpha+((255-destination_alpha)*a>>8). Esta fórmula difiere de
la familia palette previa: NO usar su256-a ni alpha255. Bloques8 visibles
NEON vld4/vmul/vmla/vshrn/vst4; ocultos evitan acceso source/palette/RGBA;
mixtos y cola escalar. Mezcla escalar calcula R/B en dos lanes16bits separados
con pesos que suman255 (sumas<=65025, sin carry/overflow), y G/alpha exactos.

Filas<32 van al original mediante dispatcher ARM20bytes, sin Cguards/counters.
Count<1 conserva el retorno original temprano. Rango seguro32..16384,
alineación, noalias con buffers escritos ni overflow: si falla, trampoline
original. Fuentes de sólo lectura pueden solaparse entre sí. No hay nuevas
reservas/cache de texturas, culling/resolución ni pérdida de objetos/efectos.
La SO en disco y la arquitectura NativeActivity/JNI/input/audio siguen intactas.

Hooks después de primer6bytes (cmp/IT/bx), primero push:0x470648/0x4707be.
Dirección/símbolo/prologue/arena comprobados antes de escribir. Original6bytes
se preserva; copia8/12bytes completa PC independiente y resume entry+8/12.
Segunda entrada Thumb a media palabra manejada con NOP alineador de so_util.
Sólo instalación al boot y flush inicial, sin modificar instrucciones por fila.
Máscara alpha_hooks expected3; palette_hooks expected255; reporte Release visible.
Counters alpha mode0direct/1indexed con llamadas largas, fast/vector/skipped,
y muestreo1/256 us. Se imprimen incluso cero para distinguir falta de uso.

Sonda de baja frecuencia SPRITE_COLLECTOR::DrawLayer so+0x4f4b20, prólogo8bytes
verificado; this/int/referenciasVECTOR2/bool/bool, dos bool en stack. Reporte
engine phase=collector total_us inclusivo para acotar el coste de sprites.
No timers/logging por píxel. Esta sonda conserva llamada, argumentos y resultado.

## Verificación

- STATICALLY VERIFIED:432casos por O0/O3 ejecutan ARM ORIGINAL inmutable y
  comparan kernels escalar/NEON: longitudes32..1024/colas, alpha0/1/127/128/
  254/255, Z0/1/32767/32768/65534/65535, igualdad/visible/oculto/mixto. Todos
  los bytesRGBA/Z/source/paleta/guard idénticos, SP/r4–r11/d8–d15 intactos.
-54casos por O0/O3 ejecutan ENTRADA SO parcheada en RAM, emitterC real,
  count negativo/cero/corto/largo, retorno temprano, alias y fallback original,
  stack forwarding/ARM-Thumb/entrada a media palabra. Pasan.
-Guardas rechazan null/alineación/alias/longitudes sin mutar RAM. Bloques
  ocultos pasan con fuente/paleta SIN MAPEAR: no leen píxeles rechazados.
-Unit tests en tests/raster_alpha_arm.c/.py y run_raster_alpha_regression.sh.
  Unicorn2.1.4/pyelftools0.32 sólo carpeta temporal Windows, no emulador Vita.
-O3/128píxeles instrucciones kernel directo visibles5131→936, mixtos3563→2650,
  ocultos1035→344; indexed5387→1505,3313→3097,1035→353. No incluye todo
  wrapper/counters ni mide ciclos/caches/hardware: NO convertir en FPS prometidos.
-DebugO0 es más lento y diagnóstico; usar Release para medir FPS.
-Regresiones existentes JNI/GL/logger, RGBA/RGB565, índiceassets, engineprobe
  O0/O3 pasan; git diff --check pasa. Build conserva hashes/pins de submódulos.
-BUILD VERIFIED Debug/Release limpias GCC10.3.0 SoftFP /usr/local/vitasdk.
  Código Release contiene instruccionesNEON. HardFP no tocado, sin nuevos
  warnings; warnings preexistentes de flagsC++/jobserver siguen.
-VPK VERIFIED: ambos ZIP/eboot y SHA de copias verificados.
-REAL VITA VERIFIED: baseline pase6 del log0010. Nuevos FPS/gameplay pase7
  PENDIENTES hardware; no reclamar20–30FPS ni menorLoading todavía.
-SO SHA256 intacto:5e5e2e1bbe86e126c3e2dce5ad97c2e9dc6282305ea7d3e24b49ee4769c5b5b7.
-Referencia aplicada: docs/METALSYNTAX_PORTING_GUIDE.md; análisis localSO exacta,
  sin referencia externa nueva/offsets de otro juego. Sin commit/push/fetch/pull
  ni Actions. HEAD usuario b827208 preservado. Nuevos archivos CMake/patch/perf,
  notas y regresiones; workflow sólo incorpora test futuro, no se ejecutó remoto.

## Entrega y prueba física

Build local-b827208-4403ffbcce, VPK Debug/Release, ELF sinstrip, flags y logs en
C:/Users/Mortar/Downloads/Zombie-Shooter-Performance-Pass-7.
Instalar Release, reiniciar juego y repetir tutorial+nivel2; observar60–90s
mismas zonas, luego disparos/zombies/explosiones. Comprobar transparencia/colores,
objetos superpuestos y HUD. Anotar FPS habitual/mínimo y tiempoLoading.
Devolver nuevo log (log_0011 o siguiente). Sólo si crash nuevo, también .psp2dmp.
El log debe mostrar alpha_hooks installed_mask3, palette_hooks255, collector y
alpha llamadas/fast/vector: si aparecen ceros, esa ruta tampoco cubre el trabajo
lento y no se debe seguir optimizándola por su nombre. No requerir al usuario
hacer múltiples pruebas adicionales antes de tener evidencia nueva.
