# Estado del port de Zombie Shooter para PS Vita

Actualizado: 2026-09-25. Master restaura el local que el usuario identifica como
el build que arranca. Base original: `23d92dc1d60286aaa402c7f544cafdcb4a06ac04`,
con todos sus cambios semánticos sin commit de CMake/FalsoJNI/FalsoNDK/OpenSLES.
Receta exacta e instrucciones: `docs/REPRODUCIBLE_BUILD.md`.

## HECHOS confirmados

- El usuario confirma que esta copia local ejecuta el juego en una PS Vita real.
  Las notas locales alcanzan tutorial y movimiento/disparos (`log_0011`–`log_0014`),
  a 4–7 FPS, sin sonido y con congelación posterior. No se declara gameplay estable.
- Se preservan loader SO en `0x98000000`, relocations/imports, fixes Protobuf/kuser,
  las 41 entradas init_array y `ANativeActivity_onCreate`.
- APK/JADX confirman GameActivity → CommonActivity → NativeActivity; el SO no
  exporta JNI_OnLoad, android_main ni Java_* (`PORTING_PLAN.md`).
- GL conserva VBO/EBO guest IDs y traducción, inicialización única, shader bridge,
  allocator de 6 MiB y MSAA 4x. Clocks locales: ARM 444/BUS 222/GPU 222/XBAR 166.
- Assets conserva AAssetManager, buffers .vid y assets pequeños, streams grandes,
  rutas game.res y protección de FD ante música .m4a no soportada.
- Audio conserva libOpenSLES_zombie.a e IBufferQueue_Clear limitado a 100 ms.
  El local contiene también CAudioPlayer_PreDestroy acotado y Vita.c diagnóstico;
  sus fuentes estaban sin commit y se incluyen completas, sin rediseñarlas.
  La eficacia de esas dos últimas modificaciones sigue pendiente de hardware.
- Input conserva el comportamiento local completo; no se reasignan controles.
- `demo/libzombie_shooter.so` se recuperó del remoto sin alterar sus bytes:
  9,773,412 bytes; SHA-256
  `5e5e2e1bbe86e126c3e2dce5ad97c2e9dc6282305ea7d3e24b49ee4769c5b5b7`.
- Build Debug y Release locales limpios: BUILD VERIFIED y VPK VERIFIED, ZIP válido.
  Flags/defines/link coinciden exactamente con los directorios originales.
- Ambos usan /usr/local/vitasdk, GCC 10.3.0 SoftFP. El HardFP no fue modificado.
- Los parches aplicados a las revisiones limpias reconstruyen exactamente el
  contenido local de las dependencias. CMake los prepara automáticamente.

## HIPÓTESIS y pendientes

El último crash limpio aportado por el usuario tiene PC `0x8100AFB8`, LR
`0x983CBB31`, DFAR `0x0000776F`. La ruta GetFieldID → GetObjectField →
GetArrayLength → GetObjectArrayElement y el fieldID 0 de WINDOW_SERVICE pueden
confundir un field desconocido (NULL=0) con `"window"`. Sus bytes en +4 coinciden
con DFAR. Las tablas locales efectivamente usan ID 0 para WINDOW_SERVICE.
Es una hipótesis fuerte, no una corrección verificada. Se conserva el puente JNI
local sin cambiar IDs/arrays antes de probar el baseline en hardware.

Continúan abiertos estabilidad al reentrar/cambiar zona, audio, FPS, controles
físicos y la posible colisión JNI. Siguiente iteración: instalar este Debug en
Vita real, reproducir el recorrido que arrancaba y después la transición problemática;
recoger sólo el último log y un dump nuevo, comparándolos con el ELF de este build.

## CAMBIOS descartados del master anterior

Remoto anterior: `2809a6951cec83a460be5777f388edf04a76f259`, preservado completo
en `backup/before-local-restore-20260925`. No se hizo pull/merge/rebase.

| Clase | Tratamiento |
| --- | --- |
| A: necesario y existente localmente | Loader, GL bridge, buffering/FD, input y fixes JNI/NDK locales conservados. |
| B: mejora segura de reproducción | Sólo dos build types, verificación de SDK/parches y limpieza de objetos VitaGL entre variantes. No se reaplica código de runtime remoto. |
| C: experimental o distinto al baseline | Descartados quiet Release y sus defines/identidad de build, bridge fndk_log vacío, telemetría de memoria VitaGL, MSAA desactivado y política de preservar clocks externos. |
| C: diferencias CI | Descartados imagen nightly móvil/GCC15, include ASensor exclusivo de CI, shim getentropy y overrides de link. Se usa copia exacta del SDK local. |
| D: documentación | Se conservan reportes de crash/hardware, diagnóstico Release y plan de rendimiento como historia explícitamente marcada; no describen el runtime actual. |
| Excepción explícita | demo/libzombie_shooter.so y su documentación se conservan del remoto íntegros. |

Las afirmaciones antiguas de Release silencioso, MSAA off, clocks externos o
telemetría activa sólo corresponden a builds históricos del backup.
La validación de GitHub Actions se realiza después del push: no confundir sus
resultados de compilación con una prueba nueva en PS Vita.
