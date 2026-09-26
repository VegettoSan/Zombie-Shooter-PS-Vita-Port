# Estado del port de Zombie Shooter para PS Vita

Actualizado: 2026-09-25. Baseline funcional: `bd2dc1a8bd6067d0508381a5f4a8e1d8748b9969`,
publicado en master antes de este pase. Receta SoftFP: `docs/REPRODUCIBLE_BUILD.md`.

## HECHOS confirmados en Vita real

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
El usuario autoriza explícitamente commit, push y workflow manual en este pase.
Los resultados finales de build y workflow se registran en el informe del pase.
