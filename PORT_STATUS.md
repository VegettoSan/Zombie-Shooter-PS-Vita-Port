# Estado del port de Zombie Shooter para PS Vita

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


Actualizado: 2026-09-25. Baseline físico actual: `8282fe9`; pase 2 local compilado,
pendiente de prueba en hardware. Historial del baseline anterior `bd2dc1a` abajo. Receta SoftFP: `docs/REPRODUCIBLE_BUILD.md`.

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
El usuario autorizó explícitamente commit, push y workflow manual sólo para el pase 1.
El pase 2 permanece local, según AGENTS.md.
Los resultados finales de build y workflow se registran en el informe del pase.
