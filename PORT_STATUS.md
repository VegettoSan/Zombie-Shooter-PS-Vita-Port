# Estado del port de Zombie Shooter para PS Vita

## Pase9: acelerar Gamma/preparePalette (2026-09-26)

El Release Pass8 `1aebd541` ya fue probado en Vita real. Runtime confirma
`software_width=864`, superficie interna 864x489 y salida 960x544. El desglose
nuevo cambia el diagnóstico: en gameplay pesado `preparePalette` consume
~4,8–9,4 ms/frame, mientras `draw_impl` queda ~1,5–3,5 ms/frame; TexSub sigue
~5,6–7,1 ms/frame. Ejemplos físicos: ~12,5FPS con software35,95/prep6,22/
draw_impl3,43/TexSub6,04 ms por frame; ~15,2FPS con software23,41/prep4,76/
draw_impl1,51/TexSub6,88; ~12,4FPS con software30,26/prep8,70/draw_impl1,94/
TexSub7,15. Los tiempos GRAPH/MAP/software son inclusivos y no se suman.

Análisis de la SO canónica localiza el coste: `VID::SetGammaToPalette`
(so+0x506378) transforma 256 colores y llama 256 veces a
`Color::Color(Gamma,Color,int)`. La fórmula ARM exacta AARRGGBB fue reconstruida.
Pass9 reemplaza sólo esa función por una implementación bit-exacta NEON: 8 colores
por bloque/32 bloques por paleta, con Gamma default, saturación RGB y alpha signed
conservados. Hook guardado por símbolo+offset+Thumb+prólogo8bytes; SO inmutable.
Regresión independiente: casos límite +250.000 combinaciones color/Gamma.
Nueva telemetría `gamma_palette` permitirá medir beneficio real. **FPS NUEVOS AÚN
PENDIENTES VITA**: no afirmar 20–30 sostenidos. Si preparePalette cae como se
espera, TexSub (~6–7ms/frame) es el siguiente objetivo respaldado por el log.
Detalles: `docs/PERFORMANCE_PASS_9_2026-09-26.md`.

## Pase8: descomposición interna de VID_SOFTWARE (2026-09-26)

El perfil previo dejaba `GRAPH::softwareTact` en ~47,74–59,92 ms/frame frente a
~17,66–18,05 ms/frame de TexSub; los tiempos MAP/GRAPH/software son inclusivos y
no se suman. Pase8 no añadió otro speedhack: analizó la SO canónica y confirmó
`Draw/DrawToVid -> preparePalette -> draw_impl`. Se añadieron sondas muestreadas
de bajo coste para Draw(1/32), DrawToVid(1/32), preparePalette(1/64) y
draw_impl(1/32), con símbolo+offset+prólogo8bytes+arena validados. La prueba física
del Release `1aebd541` demostró que `draw_impl` NO era el hotspot dominante y
dirigió Pass9 hacia `preparePalette`. SHA de SO intacta. Detalles:
`docs/PERFORMANCE_PASS_8_2026-09-26.md`.

## Pase7 y resultado físico pase6 (2026-09-26)

Usuario/log0010: FPS parecidos, carga inicial casi2min; NO mejora confirmada
pase6. No hay contadores palette de filas>=32: cobertura desconocida; ahora
máscara instalación visible. Software47,74ms/frame ventanas19–48 (~10,54FPS),
59,92ms en79–96 (~8,47FPS); map incluye software/graph. Primeras17ventanas:
asset_open_ok54,60s (incluye carga buffer/cierre), shader_link5,91s; los
shaders medidos no explican toda la demora. Carga inicial sigue pendiente.
Pase7 adapta dos rutinas alpha32 diferentes usadas por VID_SOFTWARE::draw_impl,
NEON8píxeles y mezcla escalar exacta; Zreadonly/transparencia original, fallback
conservador. Nueva sonda collector y counters alpha incluso0. Equivalencia
contra ARM original432casos/O0/O3 +54entradas SO parcheadas/ABI/fallback pasan.
Debug/Release/VPK local-b827208-4403ffbcce verificados; FPS reales nuevos PENDIENTES Vita.
Detalles: docs/PERFORMANCE_PASS_7_2026-09-26.md. NativeActivity/SoftFP/SO intactos.


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


Actualizado: 2026-09-26. Baseline físico actual: Pass8 `1aebd541`; Pass9 compilación/validación en curso.
Receta SoftFP: `docs/REPRODUCIBLE_BUILD.md`.

## Historial: hechos del baseline bd2dc1a

- El usuario probó físicamente el nuevo Zombie-Shooter-Vita-Release.vpk:
  VitaGL, logos, LOADING y tutorial OK; movimiento, disparos, explosiones y sonidos OK.
  Caminó durante un buen rato y el juego siguió respondiendo. Estabilidad inicial OK.
- JNI fix: **REAL VITA VERIFIED**. IDs no nulos para WINDOW_SERVICE/SDK_INT;
  fields object desconocidos devuelven NULL. No reapareció el crash signatures/0x776F.
- Nuevo baseline con audio funcional: aproximadamente **5 FPS sostenidos**.
  Las observaciones antiguas sin audio y pendientes del fix JNI son historia,
  no el estado de este baseline. Estabilidad larga y cambios de zona siguen abiertos.
- APK/JADX: GameActivity → CommonActivity → NativeActivity. SO en 0x98000000,
  41/41 init_array, protobuf/kuser, asset buffering, FD fixes, protección .m4a,
  VBO/EBO guest IDs, input y lifecycle preservados.
- `demo/libzombie_shooter.so` permanece inmutable: SHA-256
  `5e5e2e1bbe86e126c3e2dce5ad97c2e9dc6282305ea7d3e24b49ee4769c5b5b7`.

## Pase de rendimiento 1

Detalles y límites: `docs/PERFORMANCE_PASS_1_2026-09-25.md`.
Release elimina logging normal, console spam de diagnósticos y sync por error;
conserva errores, fatal y PERF. Bindings GL se rastrean sin consulta por draw ni
búsqueda inversa de 65.535 slots. Se agregan contadores GL/audio/logger/I/O/waits.
Clocks son mínimos 444/222/222/166 y no bajan configuraciones externas superiores.
Se mantienen 960x544, MSAA 4x, memoria, audio y timeout de 100 ms, assets e input.
No se activan speedhacks ni se modifica el binario Android.

Objetivo inicial: >15 FPS sostenidos; posterior: 20–30 FPS. **No medidos aún**
para esta nueva build. Compilación/ZIP no prueban FPS ni estabilidad en hardware.
El siguiente paso es probar Release con el mismo tutorial y conservar el log
completo (header de build y varias ventanas PERF). Si queda tiempo sin explicar,
se necesita perfil de CPU dentro del SO antes de nuevas optimizaciones.

## Reproducción y publicación

Sólo Debug/Release, SDK `/usr/local/vitasdk`, GCC 10.3.0 SoftFP;
`/usr/local/vitasdk-hardfp` intacto. CMake aplica parches versionados con hashes.
Ambos parches se verificaron desde revisiones limpias; JNI/GL/logger pasan en O0/O3.
Respaldo de este baseline: `backup/before-fps-pass-1-20260925`.
`backup/before-local-restore-20260925` se conserva en origin.
Los resultados finales de build y workflow se registran en el informe del pase.

## Pase Android Fidelity — 2026-09-26

Build local-e149e0e-f32ca72dbe: Debug/Release baseline SoftFP BUILD VERIFIED.
xdpi/ydpi220 corrige entrada de diagonal;1480×838 viene de1DPI y MaxWidth1480.
1024×580 es predicción estática pendiente Vita. Cinco WAV conservan muestras
originales, con desvío URI OpenSL; sonido/loops pendientes. UI touch oculta por
lógica original gamepad; causa runtime de mira no demostrada.
Cache shader nativo validado, fallback e inventario. NO_DEBUG/textures/draw2
compilan y GC ELF compila, todos desactivados hasta A/B físico.
SO/mapping previos intactos, tests host/ARM y parches desde fuentes limpias PASS.
Sin commit/publicación. FPS/cámara/audio/cache hits nuevos PENDIENTES.
Informe33puntos y checklist: docs/ANDROID_FIDELITY_PASS_2026-09-26.md.

## Corrección FPS/PCM16 — 2026-09-26

log_0013 confirma en Vita el build previo: superficie 1024×580, cámara mejor según
usuario y gameplay ~15 FPS, picos20; música silenciosa pese a WAV presente.
Causa estática: paquete previo float32 incompatible con SndFile_Realize del SDK.
Esta entrega usa PCM16 validado contra AAC decodificado a PCM16 y logs Realize.
Resolución interna limitada a 864×489 prevista por defecto (864/960/0 en config);
pantalla física960×544/DPI220 conservados; 28.86% menos píxeles.
Build local-54fc2ef-0aeec2a806: Debug/Release/VPK y regresiones PASS; nueva música audible,
asignación real de superficie y 25 FPS estables PENDIENTES de Vita.
Esta nota actualiza los estados pendientes del informe Android Fidelity anterior.
Informe/instalación: docs/FPS_PCM16_PASS_2026-09-26.md. Sin publicación.


## Prueba física log_0002 y pase luces — 2026-09-27

Usuario confirma en Vita Release local-d8b8c5e-f41edd8b67: Sigma Team sin
parpadeo, música M4A original audible sin cortes, SFX sin tartamudeo. Overclock
máximo500/222/222/166; FPS mejora subjetiva pero bajones con hordas/explosiones/
linterna/luces. Log95ventanas:0musicopen/decode/underruns,0outputerrors,
0Clear/Destroytimeouts,0deadline_misses;29late_wakeups menores. No30FPS estables.
Software~17–22ms/render ySCRIPT~11–19ms/tick en varias escenas, timings inclusivos.
Nuevo kernel exacto NEON profundidad de luces + medidas por capa y VID_LIGHT,
guards2hooks nuevos y reutilización del collector ya existente. Kernel288casos
ARM+63entrada parcheada por O0/O3 PASS; estabilidad/audio/cache/reuse PASS.
Build local-d8b8c5e-a4990ab788 Release/Debug/VPK/ELF SoftFP BUILD VERIFIED.
Uso real del kernel y FPS/luces de la nueva build PENDIENTES Vita. Audio/Logo se
conservan sobre la base ya probada; no retocar música ni convertirla aWAV.
APK/SO/Data/SDK intactos. No commit/push/publicación.
Informe/hashes/prueba: docs/LIGHTS_LOG0002_FOLLOWUP_2026-09-27.md.


## Seguimiento log_0003: luces reales — 2026-09-27

Usuario no percibe mejora del pase local-d8b8c5e-a4990ab788 tras alternar linterna.
Log82ventanas confirma light_rows0: kernelNEON anterior nunca usado en recorrido.
VID_LIGHT33735calls, capa11~15–18ms/tick en tramos; capa0software ySCRIPT siguen
costosos. No atribuir diferencias de escenas sólo a linterna ni declarar30FPS.
Nueva medición source/AS2/stencil,3hooks guarded; experimento exacto de2divisiones
signed en DrawLightSource, sin tocar helper global, con cache64entradas inmutable
512bytes/fallback. DefaultOFF hasta A/B físico. Source local-d8b8c5e-4b6548a578,
Release/Debug-divon y Release-control-divoff BUILD/VPK VERIFIED. 1520casos ARM,
800kintentos concurrentes O0/O3, sitios reales IT/BL/veneer, ABI3wrappers,23hooks
canonical y estabilidad/audio/cache/reuse PASS. Originales/SDK intactos.
FPS y visual del experimento PENDIENTES Vita; si cache0/ramaAS2, cambio no participa.
Informe/prueba2logs: docs/LIGHTS_LOG0003_FOLLOWUP_2026-09-27.md. Sin publicación.


## Seguimiento log_0004 Release ON / log_0005 control OFF — 2026-09-27

Encabezados coinciden con A/B; ON pobló5entradas, source18208/23987calls,
AS2/stencil y light_rows cero. No hay mejora FPS atribuible: distinta carga.
Divisiones siguen defaultOFF. Capa0~17ms/render, luz~6,6ms/tick y SCRIPT12–13ms/tick
en ventanas activas. Clear espera~2,4–2,9ms normalizados/tick y hasta1s/5s por
ventana. Nueva exclusión de FillBuffer permite Clear inmediato sólo sin lector;
conserva ack/timeout/propiedad cuando hay lector. Música M4A original, bloque1024
y Destroy sin cambios. Cola/gate O0/O3 y 200handoffs concurrentes PASS; sanitizer
y layout de mixerSDK PASS; estabilidad/música/cache/reuse/guards/ABI PASS.
Build local-d8b8c5e-ee2bcbe20f-clearfast Release/Debug; hashes/verificación en paquete.
Uso real, FPS y audio de Clear nuevo PENDIENTES Vita. Pedir1log de juego con
disparos/explosiones/linterna y regreso al menú. Originales intactos, local-only.
Informe: docs/LIGHTS_LOG0004_0005_FOLLOWUP_2026-09-27.md.
