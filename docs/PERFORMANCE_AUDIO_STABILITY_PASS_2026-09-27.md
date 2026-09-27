# Performance y audio stability — 2026-09-27

## Estado de la entrega

BUILD VERIFIED y HOST/ARM TEST VERIFIED. Prueba física de esta entrega PENDIENTE.
No se afirma 30 FPS, eliminación del stutter, música audible o SFX sin cortes en Vita.
Esta build permite contrastar la caché, el tamaño de output y la música original y
obtener la subfase de MAP necesaria para la siguiente optimización equivalente.

Baseline obligatorio: `4771a9cdfcd714bdb15adb622fcaf2ebacf72ad2`.
HEAD y master conservan exactamente ese commit; cambios locales sin commit/push/publicación.
Respaldo del estado previo: `/home/vegettosandev/zombie-stability-backup-20260927`.
Se preservaron los cambios previos, incluidos finales de línea CRLF.

Log examinado personalmente: `C:\Users\Mortar\OneDrive\Desktop\log_0004.log`,
SHA256 `2a8b60c40d428a426446924a52e1734d8ff145b0087b73f2656edafbf05cb479`.
99 ventanas. ARM500/BUS222/GPU222/XBAR166, software864x489, display960x544,
MSAA NONE, RGBA final, `framebuffer_565=0`, `software_frameskip=1`, `music_mode=0`.

## Reconstrucción del trabajo anterior

| Pase | Evidencia / resultado conservado |
|---|---|
| 1 | Loader funcional, diagnóstico del coste Android/GL/I/O; baseline físico ~6–7 FPS tutorial. |
| 2 | Eliminación de preservación completa en reemplazo total de texturas. Prueba siguiente: logos~28, tutorial9–11 FPS; no atribuir todo el salto a una función. |
| 3 | MSAA NONE y análisis de formatos; siguiente prueba tutorial10–12, nivel2~8–9; superficie todavía grande. |
| 4 | Índice de ausencias e I/O. Loading~2min; FPS prácticamente iguales, +1 ocasional. |
| 5 | Sondas GRAPH/software/MAP/pre/post/collector, gameplay completado; coste CPU confirmado. |
| 6 | Familia palette/raster ARM; pase siguiente no mostró mejora sostenida ni llamadas largas a la familia instrumentada. |
| 7 | Ruta alpha equivalente; tests ARM preservados. No presentar compilación como mejora física. |
| 8 | Desglose VID_SOFTWARE y resolución864; preparePalette~4.8–9.4ms, draw_impl~1.5–3.5ms. |
| 9 | Transformación gamma palette NEON guardada; se conserva. El perfil nuevo ya exige mirar MAP/I/O. |
| 10 | Conversión final CPU RGB565~12–13ms frente RGBA~6ms: descartada como default, queda diagnóstico explícito2. |
| 11 / 4771a9c | Reuse software2:1. Picos~40, escenas ligeras~30, horda21–24 y spikes~9; SFX cortados y música0. |
| PCM/OGG previos | WAV float incompatible, posterior PCM16 seguía silencioso/crash OpenSL; OGG previo cargaba PCM completo y tenía hooks/layout/volumen incompatibles. No reutilizar esa arquitectura. |

Los informes existentes contienen las atribuciones y límites de cada prueba física.
No se cambiaron resolución, gamma, timestep, IA, población, efectos, clocks ni flags VitaGL.

## APK/Data/SO originales

AAPT y tablas reales DEX: paquete `com.sigmateam.zombieshooter.free`, versión3.5.3,
versionCode1153, minSDK24/target35. Base APK sin SO; split `config.armeabi_v7a.apk`
contiene el motor y nueve bibliotecas auxiliares de SDKs. SO ARMv7/Thumb2/SoftFP/NEON,
NativeActivity, dependencias log/android/EGL/GLESv2/OpenSLES/z/m/dl/c.
`GameActivity`/`TVGameActivity` definen onStart/onStop; `CommonActivity` contiene
onCreate/onDestroy/onPause/onResume/onStart/onStop y nativeOnActivityResult.
No aparece clase propia AudioTrack/MediaPlayer entre las clases SIGE del juego;
el audio del motor se confirma por sus símbolos MusicPlayer/SfxBuffer/opensles.
La inspección DEX es de tablas de clases/métodos; no se afirma haber decompilado
íntegramente todos los SDKs de anuncios. Se conserva el lifecycle de BaseStream.

Inventario estricto: 338 archivos `.vid`, 36,642,593 bytes; mediana25,497,
p90197,872, máximo1,651,859. `vid/2045.vid` mide1,274,486.
El directorio vid completo tiene348 archivos incluyendo imágenes auxiliares.
109 efectos originales: WAV PCM16 mono22050Hz, sin compresión; todos admiten
conversión entera2x a output44100. No se necesita resampler fraccional para este Data.
La preferencia del usuario de evitar WAV se aplica a música: se mantienen los SFX
originales del juego y no se generan sidecars musicales WAV/OGG.

| Música original | Bytes | Duración contenedor | Formato |
|---|---:|---:|---|
| amb01.m4a | 950784 | 58.759s | AAC44100 estéreo |
| menu_mus01.m4a | 1221933 | 75.434s | AAC44100 estéreo |
| mus01.m4a | 1563423 | 96.003s | AAC44100 estéreo |
| mus02.m4a | 2596021 | 159.605s | AAC44100 estéreo |
| rain.m4a | 1072440 | 66.223s | AAC44100 estéreo |

Cinco comprimidos suman7,404,601 bytes (~7.06MiB). PCM completo sería~76.7MiB;
no se asigna. Duración AAC decodificada puede incluir padding de paquetes;
el test compara todos los bytes PCM con FFmpeg, no sólo el metadato de duración.

Hashes del APK base, splits y SO, y de los3642 archivos de Data: iguales al respaldo.
SO canónica SHA256 `5e5e2e1bbe86e126c3e2dce5ad97c2e9dc6282305ea7d3e24b49ee4769c5b5b7`.
No se reemplazaron originales; VPK contiene loader/UI, no Data/SO/música.

## Comparación externa

Clones y resultados en `/tmp/zombie-vita-research`, fuera del repositorio.
Se inspeccionaron README, fuentes relevantes, flags y arquitectura de los17 repos.
API pública GitLab enumeró los proyectos del autor; cuatro ports Android relevantes.
Historial: nova3 contiene release `bb9796f`, cambios posteriores de enlaces;
Zenonia4: `4453fd9` Turbo, `9ddbee7` investigación GPU crash/CPU,
`9f4f68c` arco GPU, y documentación `5552cfd`. Los resultados de otros autores
son evidencia de sus juegos, no pruebas físicas de Zombie Shooter.
NOVA cita TheFloW/Andy Nguyen, Rinnegatamante, Volodymyr Atamanenko,
soloader-boilerplate/so_util/VitaGL/FalsoJNI/FalsoNDK/OpenSL; se siguieron esas
referencias en las capas ya presentes de Zombie y la guía local.

| Repositorio estudiado | Commit inspeccionado |
|---|---|
| [Advena-Vita](https://github.com/MetalSyntax/Advena-Vita) | `13ef21be5104d220196820ba48eb1f832dbc3048` |
| [Asphalt-5-Vita](https://github.com/MetalSyntax/Asphalt-5-Vita) | `a0b079ca8c4103d0a114b316a48dd52df88e9c8b` |
| [Asphalt-6-Vita](https://github.com/MetalSyntax/Asphalt-6-Vita) | `4159617d9c64813eae7dd8d5649b84f959867e5a` |
| [Dungeon-Hunter-2-vita](https://github.com/MetalSyntax/Dungeon-Hunter-2-vita) | `8417da666b67d9c70a91c2bff90bdac44ba17a40` |
| [Gangstar-Miami-Vindication-Vita](https://github.com/MetalSyntax/Gangstar-Miami-Vindication-Vita) | `92f889e8e3c4d32fc5ac3c1da521df3c004eb729` |
| [Inotia3-vita](https://github.com/MetalSyntax/Inotia3-vita) | `b8a9a7f2e8c19adff2eaea7634646750727eeedc` |
| [Sacred-Odyssey-vita](https://github.com/MetalSyntax/Sacred-Odyssey-vita) | `06a1b65c3baccaf71766a02faedb6d90f2fc97d5` |
| [Shadow-Guardian-vita](https://github.com/MetalSyntax/Shadow-Guardian-vita) | `043e95be90358dfb57061bbc9412f23f6d8af0ad` |
| [Zenonia2-psvita-port](https://github.com/MetalSyntax/Zenonia2-psvita-port) | `071f6ef78a318322ca9ac13f7d3d121789e3c358` |
| [Zenonia4-psvita-port](https://github.com/MetalSyntax/Zenonia4-psvita-port) | `5fa5e0d126140e3d2bb75170b1d0ea89bae7d6a5` |
| [asphalt8-vita](https://gitlab.com/sexcurrybeats/asphalt8-vita) | `840e51d4348fc07e5b3369e8ba2d3f7daf9cd1a1` |
| [bombsquad-vita](https://gitlab.com/sexcurrybeats/bombsquad-vita) | `347300dc67ebb37265f74f8714e3575215981bb3` |
| [nova3-vita](https://gitlab.com/sexcurrybeats/nova3-vita) | `1c533c374458a156bd83628e6cc9e436925ffdde` |
| [powerpuff-mojo-madness-vita](https://gitlab.com/sexcurrybeats/powerpuff-mojo-madness-vita) | `0aaa00938bec5c70f1422fa3a6380afa991be9c0` |
| [prince-of-persia-classic-psvita-port](https://github.com/MetalSyntax/prince-of-persia-classic-psvita-port) | `b74922bed98998072fc077d25bc6aeec020d83b3` |
| [psvita-port-toolkit-cli](https://github.com/MetalSyntax/psvita-port-toolkit-cli) | `516e612b7470496a878cebb130da80361326bc44` |
| [zenonia3-psvita-port](https://github.com/MetalSyntax/zenonia3-psvita-port) | `3d68cd36b574801af7695104987fc4a4c4b63398` |

| Técnica | Port / código | Problema | Aplicabilidad / decisión | Riesgo | Prueba |
|---|---|---|---|---|---|
| Software raster + separación lógica/render | Zenonia4 loader/main, historial Turbo | CPU RLE/blit/upload | Similitud raster alta; motor Nexus/Clet diferente a SIGE. Conservar reuse existente; medir imágenes nuevas. | Omitir UI o alterar lógica al copiar hooks | Comparar input/UI/animación y ticks/newframes |
| RAM audio/cache | Zenonia2/3/4, Inotia3 | Lecturas recurrentes | Bytes inmutables compartidos con cursores propios en FalsoNDK activo | Memoria/pinning | Hits, bytes, eviction, prueba concurrente |
| Grains explícitos | Zenonia2/3/4 e Inotia512; Advena1024; ShadowGuardian2048 | Output insuficiente | Unidades frames separadas del decode;1024 primera A/B | Latencia y Clear más lento |128/1024/2048 mismo recorrido |
| Core audio separado | Zenonia4/Advena/DungeonHunter2 | Contención con raster | Afinidad existente mixer core1 conservada; worker no fija prioridades nuevas | Contención CPU decoder | Decode/fill/gaps con música0/2 |
| Mix128/output512 y ring productor | NOVA3 third_party OpenSL/Vita.c | Jitter hardware/callback | Se adopta separación de unidades, no su ring/ownership ni offsets | Cambiar Clear/callback lifecycle | Clear/Destroy y fills bajo horda |
| Wrappers audio/JNI/FD/OBB | NOVA3/Asphalt8/BombSquad/Powerpuff | Compatibilidad Android específica | Estudiados; layouts/OBB no equivalentes a Data SIGE | Parches de otro motor | No portar offsets ni flags |
| Mix/AudioTrack propios y vídeo | Asphalt5/6, Gangstar, SacredOdyssey, PrinceOfPersia, DungeonHunter2 | APIs Gameloft/Java y cinemáticas | Similar finalidad, API diferente; no abrir segundo puerto musical | Ownership/latencia/hilos | Mantener único OpenSL |
| Toolkit / guía loader | psvita-port-toolkit-cli | Reproducibilidad y hooks | Aplicar guards y SDK fijado | ABI/fingerprint | SOhash/símbolo/Thumb/prologue |
| GPU quads / decoded sprite cache | Zenonia4 variantes descartadas | CPU raster | Descartado: GPU arc tuvo corrupción/crashes; decoded-cache misses; no equivalente automático | Fidelidad/VRAM/contexto | No introducir aquí |
| NEON / fastclear | Zenonia4 CPU arc | Loops CPU | Gamma existente equivalente permanece; no nuevo bypass sin hotspot MAP | Semántica/memoria | Esperar perfil real |

Fuentes primarias adicionales: [FFmpeg send/receive](https://ffmpeg.org/doxygen/trunk/group__lavc__encdec.html),
[VitaSDK audioout](https://github.com/vitasdk/vita-headers/blob/master/include/psp2/audioout.h).
Ningún código externo fue vendorizado y no se tomaron offsets de otro juego.

## Evidencia del log y cambios

Ventana85 (índice humano1):46frames/5033ms=9.1FPS,165opens/139ok,
3,156,413us abriendo; MAP4,990,055us inclusive; frame máximo3,745,364us.
Carga inicial también tiene ventanas2–10FPS con cientos deopens, distintas del gameplay.
Ventana92:115ticks/5022ms=22.8FPS, MAP4,819,126us=41.91ms/tick,
12opens65,090us,86Clears130,977us,1216outputs sin errores.
Ventana91:114ticks, MAP4,877,049us=42.78ms/tick,17opens138,853us,
896outputs. No se suman tiempos inclusivos MAP/GRAPH/I/O como costes independientes.

128frames a44100 son2.902ms; ideal~1723outputs por5s. Contadores896–1216
son compatibles con alimentación insuficiente bajo carga, pero no identifican
por sí solos una causa única. output_errors0 nunca prueba ausencia de starvation.

### Caché efectiva

CMake `NDK_PORT=ON`: compila `lib/falso_ndk/android/AAssetManager.cpp`;
`source/reimpl/asset_manager.cpp` está inactivo. Baseline hacía fopen/malloc/fread
por cada nuevo handle, aunque existiera otro handle con esos mismos bytes.
`source/utils/asset_cache.c`:512 entradas, key completa256, LRU, refcount,
bytes compartidos inmutables; cursor/seek propio por AAsset; eviction sólo sin refs.
Mutex no rodea I/O. Cargas concurrentes deduplican backing al admitir.
Mantiene thresholds previos <=256KiB o VID<=2MiB; no precarga ni whitelist inventada.
8MiB inicial dado totalVID~35MiB y working set aún desconocido;16 opcional,0 A/B.
Si pinned ocupa presupuesto, conserva fallback privado/FILE baseline.
No cabe garantizar presupuesto de todos los handles privados del motor: el límite
es de caché, incluidos pinned, no de todo heap. Metadata fija~180KiB adicional.
El log mostraba poolVitaGL RAM34253/71680KiB, no heap libre del loader;
systemfree2048KiB no demuestra OOM. Se añade mallinfo para medir el heap real.

Normaliza slash/backslash sin inventar aliases case-sensitive. Índice incluye music
para que ausencias `.ogg` repetidas sean rechazadas sin fopen, incluso música0.
Case ambiguo conserva fallback del índice. Telemetría por ventana hits/misses,
evictions/bytes/peak/avoidedopens/savedreadbytes y top5path; tabla128 bounded,
se informa overflow. Reads al cerrar pueden cruzar ventana. Log viejo no contiene
un recuento exhaustivo de reaperturas porpath: éste requiere el nuevo perfil.

### MAP y pacing

Se derivaron10símbolos reales de SO y callgraph en `/tmp/zombie-vita-research`:
StartTact, MapLoader::tact, SCRIPT::mainLoop, SPRITE_COLLECTOR::squeeze,
processDamagedTact, clearDeleteSpritesList, AutoAim::exec, MAP::groupsTact,
SfxBuffer::play(int,int), MAP::execPostponedScripts. Guard símbolo+offset+Thumb+
8bytes prologue y arena; trampoline16bytes sólo prólogos PC-independientes.
Expected mask0x3FF. Se muestran calls/samples/total/avg/max lifetime y p95bucket
aproximado; SFX muestrea1/8, sus totales estimados no son exactos.
Subfases inclusivas: no sumar si anidadas; sondas auxiliares pueden tener llamadas
fuera de MAP. El callgraph no demuestra que AI/pathfinding sean el hotspot.
No se optimiza ni paraleliza simulación sin ese timing físico.

Rates separados ticks/presents/newsoftware/reused, frameavg/max y >33/50/100ms.
No limiter ni nuevo adaptiveframeskip: MAP~42ms ya excede33.33ms; un sleep no
crea headroom. Reuse2:1 conservado:30presents puede significar15imágenes nuevas.
Siguiente política adaptativa sólo tras perfil/headroom, con histéresis y tests
antes de cambiar render; meta30imágenes sigue PENDIENTE.

### OpenSL y música original

Antes: buffer512bytes, stereoPCM16=128frames. Ahora:2buffers hardware,
128frames mixer porchunk, output configurable128/1024/2048 independiente de
SndFile. Port único BGM44100;1024=23.22ms/2048=46.44ms. Afinidad y prioridad
existentes conservadas. Puede aumentar esperaClear hasta un periodo output;
por eso la A/B debe mirar MAP y Clear simultáneamente.
Clear preserva timeout100ms, pendingflag y ownership hasta acknowledgement;
CAudioPlayer Destroy y fix previos intactos. No nuevo pool ni lockfree speculative.
Enqueue sigue allocando para convertir efectos: se mide coste/bytes/depth/allocs.
PCM8 conversión multiplicativa evita shift negativo UB; valida ratio entero,
canales/bits/alineación/overflow.32k/48k se rechazan, no se reproducen a velocidad
incorrecta; no están presentes entre los109SFX reales.

Música modo2: los5M4A originales AAC, FFmpeg ya instalado en SDKSoftFP.
Worker carga comprimido<=3MiB/archivo y decodifica en chunks1024 a ring8192frames
porvoice (4voices~128KiB total), prebuffer2048 (~46ms). Presupuesto contabilizado
8MiB (comprimido+estructura+scratch+AVIO), heap interno codec no incluido: mirar
mallinfo; apertura puede tener transitorio adicional <=3MiB+codec antes admisión.
No PCM largo, no conversión/package y no disco/codec en threadoutput.
Los4archivos estáticosSDK están fijados por SHA256 sin modificar SDK.

Se reemplazan sólo6métodos MusicPlayer backend con guards de esta SO; onUpdate/
updateVolume incluyen PCrelative pero son reemplazos completos, no trampolines.
Helpers STRING::c_str y BaseStream::stop tienen offset/Thumb/fingerprint.
BaseStream original conserva nombres, play/stop/tact, fades, cambio de pista y loop.
Layout derivado:volume+32,loop+40,shouldStop+68; volumen entero0–100.
Generation counters cancelan resultados antiguos tras stop/reopen; pause/resume,
EOF/error, clipping y shutdown/join verificados host. Worker arranca enSDL_open
después deSDL_close inicial. Mix trylock evita espera realtime; misses se cuentan.
Loop vuelve al principio AAC mediante seek/flush; padding exacto/gap perceptivo
requiere escucha física. El decoder nuevo no usa AndroidFD ni segundo puerto.
Modo1WAV queda legado diagnóstico ya existente, no requerido ni activado.

Diagnóstico5s bounded: expectedus, outputcalls/errors, filltotal/max, maxgap,
late (>period+2ms), deadline miss (>2period), silence,enqueue/depth/allocs,
Clear/Destroy waits, musicactive/queued/bytes/underrun/lockmiss/decodecost.
Silencio puede ser legítimo; no se etiqueta como underrun. Max campos son lifetime;
contadores/tiempos acumulados reportan delta de ventana; p95 es bucket superior.

## Validación

Todos los runners existentes ejecutados: assetindex, engineprobe, framebuffer565,
gamepad/DisplayMetrics/InputDevice, GLbuffers, JNIfields, logger, rasteralphaARM,
rasterpaletteARM, shadercache, textureupdate, gamma250k y renderScale/settings.
Inicialmente framebuffer runner CRLF y faltaban Unicorn/pyelftools: normalizado
runner e instaladas dependencias host; reejecutados PASS. No se ocultaron fallos.
Host paquetes FFmpeg/AAPT/Unicorn/pyelftools instalados, SDK intacto.

Nuevos O0/O3: caché con implementación activa real, handles/cursor/seek/EOF/refcount,
eviction/pinned/private/FILE/null/slashes/missing/empty/concurrent8threads;
Enqueue/Clear/Destroy reales con stubs host de plataforma,PCM8/16 mono/stereo2x,
rechazo fractional/alignment, timeout nofree y acknowledgement/free;
workerloop/pause/resume/volume/clamp/cancel40reopen/error/shutdown/restart;
AAC5archivos fullEOF+loop+invalid, PCM byte porbyte idéntico a FFmpeg host.
`run_stability_hook_regression.py`: SOhash,16símbolos/Thumb/offset/prologue +2helpers.
Trampoline mismatch fallback cubierto por engineprobe existente.
No son pruebas de scheduler/audio hardware ni benchmark Vita.

Builds Release y Debug SoftFP con VitaGL baseline reconstruido clean por variante.
Release flags efectivos O3/g1/DNDEBUG; Debug O0/g3. ELF atributos ARMv7/NEON sin
VFPregisterarguments hardfp. VPK ZIPintegrity PASS; ELF retenido por variante.
Warnings heredados: make jobserver y flagC no aplicable aC++; sin error debuild.

## Artefactos y hashes

Build ID `local-4771a9c-cc95d44a36`.
Carpeta Windows: `C:\Users\Mortar\Downloads\Zombie-Shooter-Stability-2026-09-27`.

| Artefacto | Bytes | SHA256 |
|---|---:|---|
| Zombie-Shooter-Stability-release.vpk | 3181383 | `4526ef38fdc16c9522ddf5307053fa183b64a00f5fc5e5dffb4e24edc1c05d45` |
| Zombie-Shooter-Stability-release.elf | 13521400 | `4c92ea430b9c809a9afd35a4e5cd84c102c08abcd59429b53b8a40a8ace2048a` |
| Zombie-Shooter-Stability-debug.vpk | 3253500 | `37282de80ea4de7e1d314bdcc42eb8e2fa088bf3e8ee94930f1ebc2f97e291a1` |
| Zombie-Shooter-Stability-debug.elf | 15669892 | `a4ef51f986e7deccb9fa64387245a69d41dc7fe385edc104b8f39adb991a4583` |

## Prueba física y métricas a devolver

1. Instalar primero Release en VitaShell, mismo titleID ZOMB00001. No borrar saves,
   Data ni SO. Los M4A originales deben existir en `ux0:data/zombieshooter/assets/music/`.
2. Editar `ux0:data/zombieshooter/config.txt` conservando las otras opciones:

```text
asset_cache_mib 8
audio_frames 1024
software_width 864
framebuffer_565 0
software_frameskip 1
music_mode 2
```

Un config existente con music0 sigue respetado: cambiarlo explícitamente a2 para
esta prueba. Reiniciar proceso después de cada config. No ejecutar prepare_music
ni prepare_music_ogg. No necesita paquete Data nuevo.

3. Misma secuencia: menú30s → tutorialligero30s → zombies30s → horda/disparos/
   explosiones60s → cambiozona30s → volvermenú. Escuchar música de menú/gameplay,
   loops, fade/volumen y variosSFX. Marcar hora aproximada de cortes/crashes/spikes.
4. Comparaciones separadas, una opción porvez: música2→0, audio1024→128;
   si1024 falla,2048 aparte. Caché8→0 para atribuir hits/spikes;16 sólo si heap
   permite y8muestra evictions frecuentes. Mantener ruta/armas/escena comparable.
5. Devolver log completo de arranque hasta final, config exacto y observación
   audible/visual. Confirmar build ID/Release, mapmask0x3FF, musicinstalled1.
   Si no instala hook, conservar log y usar0 como fallback mientras se analiza.
6. Incluir rates/pacing, cada map_subphase/enginephase, assetcache/path/io/slowopen,
   audiotiming/Clear/Destroy/music, heap y poolsVitaGL. Si hay crash: dump y ELF
   de esta variante. Debug sólo para crash/profiling adicional, no compararFPS conRelease.

La caché no evita primeras aperturas y no puede eliminar sola todo spike9FPS.
Headroom/hotspot MAP, costo AAC, eficacia1024, RAM real, fidelidad/loops/SFX y
estabilidad a largo plazo quedan pendientes de la misma Vita del usuario.
Si MAP sigue>33.33ms, el siguiente cambio debe partir de la subfase medida;
no reducir contenido ni declarar el objetivo25/30FPS logrado.
