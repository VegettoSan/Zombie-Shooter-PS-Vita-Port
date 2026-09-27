> Actualización posterior: el usuario probó esta build en Vita física y confirmó
> logo sin parpadeo, música M4A y SFX sin cortes. FPS todavía fluctúan con hordas,
> explosiones y luces. Los estados pendientes de audio/logo del texto histórico
> siguiente quedan actualizados en [seguimiento log_0002](LIGHTS_LOG0002_FOLLOWUP_2026-09-27.md).

# Corrección de audio y logo tras log_0001 — 2026-09-27

## Evidencia física recibida

Usuario: parpadeo nuevo en logo Sigma Team, música ausente, SFX tartamudeando,
menú/tutorial alrededor de30 presents/s y caídas según escena. No inferir éxito
por picos ni declarar una regresión de FPS desde recorridos distintos.

Log `C:\Users\Mortar\OneDrive\Desktop\log_0001.log`, SHA256
`68d0d5b12afb8456428cbf3806b93e52604d52b546e5e51542e11e50fa70657d`.
Build física: Release `local-4771a9c-cc95d44a36`,104ventanas/7790líneas.
Los guards previos: MAPmask0x3FF, música installed1, gamma1 y resol864 funcionaron.
Hay música_mode2 y worker1: no es un config0 ni hook no instalado.
Suma del log:262errores abriendo música,0frames decodificados,0errores decode.
No existe ni una apertura exitosa contabilizada de decoder musical.

Audio1024/44100=23.219ms.1927latewakeups y1260deadline_misses por la definición
period+2ms/2period. output_errors0 y enqueue_errors0 no contradicen cortes.
Maxfill llega257687us y gap2088355us lifetime (incluye carga inicial).
Clearmax23205us; ventana91:44Clears/423425us,154outputs/5s.
El buffer mayor solo no resolvió el audio y aumentó el máximo de espera Clear
frente al baseline128. Se conserva la opción128 para la comparación física.

Ventana19:33.5presents/s, MAP28.75ms/tick, SCRIPT::mainLoop13.56ms/tick.
Ventana31:28.7presents/s, MAP33.27ms/tick, SCRIPT13.57ms/tick.
Ventana81:20.1presents/s, MAP44.29ms/tick, SCRIPT18.47ms/tick.
MAP es inclusivo y contiene también GRAPH y softwareTact: no sumar sus costes
como subsistemas independientes. Subfases squeeze/damaged/groups suelen ser
microsegundos; no justifican reducir zombies/IA. No se identificó todavía el
coste exclusivo por entidad dentro de cada ejecución de script.

Cache296hits/296opens evitados (~15,033,379bytes de bulkread evitados),7409misses,
3136fallbacks,112evictions en todo el recorrido. Durante buena parte del gameplay
la caché ocupa8,388,601bytes/112entradas y todas están pinned. Por seguridad no
se desalojan esos bytes. Heap pasó aproximadamente16MiB al arranque a120MiB
asignados; poolsVitaGL y heap son cifras distintas. No se aumenta cache a16 a ciegas.

Rates viejas presentan exactamente dos ticks por imagen nueva en muchos tramos;
30presents no equivale automáticamente a30imágenes nuevas. En las dos primeras
ventanas, software por render~3.43/2.92ms mientras fase software seguía siendo
omitida en la mitad de los ticks. Esa omisión en una etapa barata es innecesaria
para el logo y es una hipótesis concreta para su defecto visual.

## Correcciones implementadas

### M4A original: capability ausente en el SDK

La comprobación `arm-vita-eabi-nm --defined-only` muestra `ff_aac_decoder`, pero
**ningún `ff_mov_demuxer`** en libavformat.a del SDK. Su configure tiene
`--disable-everything --enable-demuxer=...,mp4,m4a,...`: el demuxer se llama
`mov`, por lo que esos nombres no habilitaron la lectura del contenedor M4A.
La prueba host anterior tenía MOV y no detectaba esa diferencia con la Vita.
Esta es una carencia estática confirmada coherente con262opens fallidos; aún
no se excluyen errores adicionales de acceso a los M4A en la tarjeta del usuario.

Se construyó FFmpeg8.0.1 mínimo: MOVdemux+AACdecode+swresample+avutil, SoftFP,
O3, sin red/filtros/codec workers extra. Cuatro archivos estáticos privados en
`build-music-ffmpeg/install/`, fuera del SDK, ignorados por Git.
CMake ejecuta `scripts/build_music_ffmpeg.py`, con URL oficial, SHA256 fuente
`05ee0b03119b45c0bdb4df654b96802e909e0a752f72e4fe3794f487229e5a41`,
manifest de hashes y validación de componentes. Una configuración nueva descarga
la fuente oficial y compila; siguientes reutilizan archivos hash-verificados.
No se modifica `/usr/local/vitasdk` ni HardFP. No se convierten/generan WAV/OGG.
Los5M4A originales siguen siendo la entrada; worker/ring/mixer único/lifecycle
BaseStream y presupuesto anterior permanecen. No pasa I/O/codec al threadoutput.

Ahora el arranque informa `music_capabilities mov_demuxer=1 aac_decoder=1`;
fallos de apertura tienen stage/path/error, máximo16mensajes por proceso.
Esto distingue fopen/mov/openinput/streaminfo/rate/codec/conversión.
El hook instalado no prueba que el decoder haya abierto: mirar decodedframes.

### Mixer: prioridad explícita y verificable

Se conserva core1 y puerto único. El hilo audio solicita prioridad96 antes de
abrir el puerto y registra resultados/priorityactual/affinityactual en Release.
El render reporta prioridad159/affinity0 en este log; la ruta nativecreatethread
anterior pedía0x10000100 y la variante pthread dependía de su default. No había
telemetría que confirmase la prioridad efectiva del mixer.96 está por encima del
render159 en la escala de Vita y es una hipótesis medible contra starvation.
No se cambian prioridades del juego o decoder; el mixer bloquea en audiooutput
y su fillhabitual es corto, sin busywait nuevo. CPU/locks largos pueden seguir
provocando cortes: se compararán late/fill/gaps/Clear, no sólo output_errors.
Clear100ms/pending/acknowledgement y Destroyownership no cambiaron.
No se copió el ring productor NOVA3 porque introduce otra latencia/ownership:
primero verificar la carencia de prioridad y el codec de esta build.

### Logo y render: reuse sólo para software sostenidamente costoso

`software_frameskip 1` ahora conserva imágenes nuevas en frames baratos.
Cuatro renders consecutivos >=8ms habilitan reuse2:1; cuatro <=4ms lo deshabilitan;
el rango intermedio reinicia rachas. No cambia simulación/timestep ni salta MAP.
Un pico aislado de carga no habilita reuse. Owner/argument nuevos fuerzan render
real; el argumento viene de un flag del MAP en la SO, no de un offset prestado.
La política se prueba con secuencias baratas, picos, histéresis y cambios de owner.

`software_frameskip 0`: render siempre, para aislar parpadeo.
`software_frameskip 2`: reuse2:1 anterior forzado, para A/B.
La corrección visual del logo es candidata, no está confirmada físicamente.
No se introduce limiter30: aún hay MAP44ms en escenas pesadas, sin headroom.

### Medir el cuello de script

Dos sondas derivadas de SO: SCRIPT::startup0x48ba58 y
SCRIPT::callFunction0x48bb84, prólogos8bytes PCindependientes, símbolo+offset+
Thumb+arena comprobados. callFunction preserva sus6argumentos (this,index,
3referencias y pointerresultado); muestreo1/64 y timing inclusivo.
Nuevo mask esperado0xFFF. Los totales muestreados son estimaciones; no sumarlos
con mainLoop ni cambiar frecuencia/lógica de scripts. Pueden incluir llamadas
fuera de MAP y llamadas anidadas. Permiten elegir la próxima optimización específica.

## Referencias revisadas

HEAD público comprobado con ls-remote, sin modificar el repositorio del usuario:

- [Zenonia4 loader](https://github.com/MetalSyntax/Zenonia4-psvita-port/blob/5fa5e0d126140e3d2bb75170b1d0ea89bae7d6a5/loader/main.c): separación lógica/render y tratamiento específico de UI/offscreen; motorClet noSIGE. No copiar hooks ni asumir que sus estados de UI existen aquí.
- [Advena audio](https://github.com/MetalSyntax/Advena-Vita/blob/13ef21be5104d220196820ba48eb1f832dbc3048/source/audio.c): frames1024, coreaudio1, mixerconclipping; su prioridad/engine no prueba eficacia aquí.
- [Inotia3 audio](https://github.com/MetalSyntax/Inotia3-vita/blob/b8a9a7f2e8c19adff2eaea7634646750727eeedc/source/audio.c): grain512, decoder/mixer y lifetime; no copiar sus formatos ni offsets.
- [FFmpeg configure oficial](https://ffmpeg.org/releases/ffmpeg-8.0.1.tar.xz): demuxerMOV separado de AACdecoder; codec aislado conservando originales.

Clones/exámenes permanecen en `/tmp/zombie-vita-research`; no se vendorizan ports.
La comparación de los17repos del pase previo sigue en su informe.

## Pruebas, builds e integridad

PASS O0/O3: caché/AAsset reales, queuePCM8/16/conversiones/Clear timeout y
acknowledgement/Destroy, música worker/EOF/loop/pause/volumen/cancel/shutdown,
los5M4A comparados byte porbyte con FFmpeg host; nueva política barata/costosa/
histéresis/owner/argument. PASS renderScale/settings, framebuffer565 y engineprobe.
18prologues+2helpers contra SO canónica PASS. Checks de ELFRelease/Debug:
MOVdemux+AACdecoder presentes y ARMSoftFP sin VFPregisterarguments hardfp.
Los tests de host no simulan scheduler ni salida física de Vita.
Release O3/DNDEBUG, Debug; VitaGL clean porvariante con flagsbaseline.
VPK ZIPintegrity PASS y ELF correspondiente retenido. Source/head actual
`d8b8c5e418191465879bcd1d6fbe20a19a1c43f4` ya contiene el pase anterior como
commit del usuario; no se resetearon commits ni se hizo commit/push/publicación.
Se conserva referencia física4771a9c, SO y todos los originales.

Build `local-d8b8c5e-f41edd8b67`.

| Archivo | Bytes | SHA256 |
|---|---:|---|
| Zombie-Shooter-Audio-Logo-release.vpk | 2523234 | `8e759fd95c9e161898b2b626d4c73e0457eb6f47c10a41bcd0475141d523b7e2` |
| Zombie-Shooter-Audio-Logo-release.elf | 12345788 | `d1ed97d913cba1577e75bcd9bfbbbe2274b8bfdf003a622b502f784a5c9f499e` |
| Zombie-Shooter-Audio-Logo-debug.vpk | 2595664 | `f8f9381bfe9af05a23cf45a67f0448136d7532c91ca8628fa96f234aedc13e6f` |
| Zombie-Shooter-Audio-Logo-debug.elf | 14494860 | `c5cef938e05a4a7e9b57535c7890b20737dacaeab8e39d7f0fd9712d37bdb8d3` |

## Prueba física necesaria

Instalar Release en VitaShell sobre ZOMB00001, sin borrar saves/Data/SO.
Conservar M4A en `ux0:data/zombieshooter/assets/music/`. En config.txt:

```text
music_mode 2
audio_frames 1024
asset_cache_mib 8
software_width 864
framebuffer_565 0
software_frameskip 1
```

Reiniciar. Observar logo y menú, escuchar música de menú y gameplay, hacer el
mismo recorrido tutorial→horda→disparos/explosiones→zona→menú. Probar pausa,
volumen y cambio de música. Devolver log completo y config; indicar si el logo
sigue parpadeando y en qué escena se corta audio.

Esperado en log: buildID nuevo, capabilities1/1, schedulerpriority96 resultado0,
mapmask0xFFF, decodedframes>0 y bytesdecoder>0 durante música.
Si música sigue muda, devolver music_open_failed completo y confirmar paths/
existencia de los5M4A; no generar sidecars. Si hay crash, dump y ELF de esta build.

Si logo persiste, comparación separada software_frameskip0 (reiniciar), sin tocar
las otras opciones. Si SFX aún cortan, comparación separada audio_frames128,
reiniciar y repetir escena:1024 puede añadir hasta23ms deClear aunque no haya
underruns.2048 no se recomienda automáticamente: duplica latencia/esperaClear.
Como tercera prueba opcional música0 aísla el costo nuevo AAC.
Se necesita como máximo unlogprincipal y unlogA/B del problema que continúe.
30FPS/imágenes reales, loops audibles, ausencia de cortes y parpadeo, memoria
estable y eficacia de prioridad/reuse permanecen pendientes de esta prueba.
