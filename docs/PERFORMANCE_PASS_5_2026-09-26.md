# Performance pass 5: localizar el coste del motor

## Resultado real del pase 4

Release `local-8282fe9-64dee46461`, log_0008 SHA-256 `c8d4cd08e5744c57706c1e9f4de029551da9919156d8479547225d6168d80b10`.
Usuario: Loading inicial aproximadamente 2 minutos; FPS prácticamente iguales,
sólo +1 ocasional. No presentar como una mejora sostenida de gameplay.
MSAA NONE, 960x544 y clocks ARM500/BUS222/GPU222/XBAR166 confirmados.
85 ventanas/439,313 segundos/3491 frames. El índice se construyó sin fallos:
1271 entradas, 16 KiB residentes, 290,056 ms de enumeración; 800 ausencias
confirmadas descartadas en la sesión. Los opens fallidos incluyen esos descartes.

Primeras 16 ventanas (tramo de carga inferido, incluye logos): 86,659 s,
2375 aperturas exitosas/54,218 s y 4403 fallidas/5,241 s. El pase 3 tenía
53,699 s exitosas y 33,470 s fallidas en su tramo inferido 2–24 (118,345 s).
Son recorridos/ventanas diferentes, no un benchmark A/B ni el tiempo completo
del Loading. Concordancia con el reporte de usuario: mejoró carga, FPS no.

Ventanas 39–46: 342 frames/40,363 s = 8,473 FPS; tex_sub 7,381 s =21,583 ms/frame.
Aperturas agregadas 395,039 ms en ese tramo; las lecturas no explican por sí
solas los FPS sostenidos. 856 draws muestreados/11,179 ms de CPU, periodo16.
La muestra no mide duración GPU ni todos los cambios de estado GLES.
No atribuir el tiempo restante sólo a software rendering sin medir las fases.
La instrumentación bulk_fill muestra que el clear grande no se llama todos los
frames de gameplay; cambiar memset no está justificado. runClocks sigue crudo,
sin inferir porcentaje ni unidades. No sumar tiempos anidados o de otros hilos.

## Cambio de carga basado en fallos observados

Se amplía el mismo índice conservador a menus/, menus/rpg/ y
menus/img/supply_boxes/: el log tiene aperturas fallidas de 18–57 ms en ellas
(p.ej. frame_black_market_slot.bmp, skill_tree_preview_07.png,
cap_open_01_10.png). El camino de archivos presentes permanece idéntico.
Una enumeración completa permite descartar sólo ausencias confirmadas;
error/Unicode/nombres ambiguos/OOM/capacidad -> implementación original.
Se conservan fallbacks de backslashes: no normalizar una ruta sin comprobar
su semántica real. Carpetas padres no ocultan el match de una carpeta hija.
Ahora seis carpetas, tope total384 KiB; catálogo local1786 entradas/asignación
21 KiB. Sólo hashes de nombres, sin payloads, caché de texturas o reservas GPU.
Vita enumera su catálogo real; reiniciar si se añaden assets durante una sesión.

## Pregunta concreta de esta build: ¿qué fase tarda dentro del motor?

Mediciones [PERF] engine phase=graph/software/map/pre/post, calls/total_us/
max_us_lifetime e inclusive=1 cada ~5 s. Interpretar como tiempos completos de
llamadas, potencialmente anidados; no sumarlos como una partición del frame.
Post incluye swap: la llamada aún abierta aparece en el reporte siguiente.
Sólo cinco límites de fase; no logs por sprite ni cambio de prioridades.

Símbolos/offsets comprobados en la SO canónica:
- GRAPH::Tact(int) 0x410a30
- GRAPH::softwareTact(int) 0x410458
- MAP::tact() 0x4372c4
- OpenGLES::preTact() 0x46aa2c
- OpenGLES::PostTact(int) 0x46af1c

Trampolines de 16 bytes en la arena RX ya reservada por el loader. Se copian
únicamente los 8 bytes exactos de prólogos verificados, PC independientes y
con límite de instrucción completo; salto Thumb a entrada+8. No usar esto
como relocator ARM general. Símbolo/dirección/alineación/espacio/prólogo deben
coincidir o se deshabilita la sonda. Código canónico de disco no se modifica.
Sólo se instalan al arranque, antes de constructores/hilos del juego; no usar
SO_CONTINUE ni escribir instrucciones/flush de caché por frame. Se conserva
r0 de retorno y argumentos this/+int; ABI sin floats/retornos ocultos/stackargs.
La build Release desensamblada confirma forwarding de argumentos y retorno.
Los nuevos hooks aún requieren prueba física: compilación no prueba su ejecución.

NativeActivity/JNI/audio/input, resolución, MSAA NONE, conservación COW y bytes
originales de assets permanecen como el baseline. No bajar detalle ni omitir
objetos, explosiones o lógica. El cambio funcional apunta a búsquedas de carga;
la instrumentación no se anuncia como una mejora de FPS.

## Verificación y archivos

- STATICALLY VERIFIED: 5 prólogos contra bytes reales de la SO; emisión de
  trampolines O0/O3 y rechazo sin mutación ante mismatch. No emulación ARM.
  Índice O0/O3: seis carpetas, padres/hijas, presencia/ausencia, colisiones,
  fallos I/O/OOM/capacidad y lectores concurrentes. RGBA/RGB565, JNI, GL y logger
  O0/O3 pasan. git diff --check pasa.
- BUILD VERIFIED: Debug y Release limpias, GCC10.3.0 SoftFP, preparación SDK29k,
  revisiones/hashes de submódulos y flags Release verificados; HardFP intacto.
- VPK VERIFIED: ZIP, eboot y copias Windows SHA-256 idénticos.
- REAL VITA VERIFIED: pase4 y reporte de gameplay del usuario; nueva build pendiente.
- SO SHA-256: 5e5e2e1bbe86e126c3e2dce5ad97c2e9dc6282305ea7d3e24b49ee4769c5b5b7.
- Trabajo local sobre master35ea6e0 existente; sin commit/push/operaciones remotas.

Archivos: source/patch.c, utils/engine_probe.h, utils/perf.c/h,
utils/asset_index.c, tests de índices/probes, workflow y notas de progreso.
Nueva build Debug/Release `local-35ea6e0-9c21ebbd26`. VPK, ELF sin strip, flags y logs incluidos.

## Prueba en Vita

Instalar Release y arrancar de cero. Anotar tiempo hasta tutorial; jugar el
mismo recorrido 60–90 s y 30 s con disparos/explosiones, abrir menú y nivel2.
Anotar FPS habituales/mínimos; devolver el nuevo log. Los tiempos de las fases
orientarán el siguiente cambio de gameplay. Si aparece un crash nuevo, devolver
ese log y el .psp2dmp; los ELF de ambas variantes están junto al VPK.
No se prometen 20–30 FPS ni menor carga antes de ese test real.
