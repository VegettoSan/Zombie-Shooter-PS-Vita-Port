# log_0004 / log_0005: A/B de luces y espera de Clear — 2026-09-27

## Evidencia física recibida

El usuario identifica **log_0004 como Release ON** y **log_0005 como control OFF**.
No aporta en este mensaje una valoración visual ni auditiva nueva.
Los encabezados verifican la misma base `local-d8b8c5e-4b6548a578`, sufijos
`-divon` / `-divoff`, ambos Release y clocks 500/222/222/166.

| Log | SHA256 | Ventanas PERF |
|---|---|---:|
|0004|6489b22c466d9016369dd7d5c1f7618b66a703e3797624e33bc7e5af777e5336|47|
|0005|315b96c33a5769b45510ac03a1fba495352507dcf46ec24055dbd493b447a84a|52|

Los archivos originales permanecen en `C:/Users/Mortar/OneDrive/Desktop/`.
Copias, JSON por ventana y hashes están en el paquete del seguimiento.

## Resultado del experimento de luces

- ON instaló las dos llamadas (`installed_mask=3`) y pobló cinco entradas de
  denominadores: **el cambio sí participó**. OFF mantuvo máscara/caché cero.
- `DrawLightSource`: 18.208 llamadas en ON y 23.987 en OFF. `AS2` y `stencil`:
  cero llamadas en ambos. La ruta usada aquí es la antigua; no corresponde
  perseguir las sombras de AS2 como explicación de estos recorridos.
- `light_rows` continuó en cero. La optimización NEON de la fila anterior sigue
  sin participar en estas escenas.
- La caché ocupada es evidencia de ejecución, no una medición de hit rate o de
  divisiones totales. No se añadieron contadores por píxel.

Resumen de ventanas con `source.calls > 0` (33 ON / 37 OFF), incluyendo sus
transiciones/cargas. Tasas ponderadas por duración; tiempos por total de llamadas:

| Métrica | ON log4 | OFF log5 |
|---|---:|---:|
|Presents por segundo|22,572|22,945|
|Imágenes software nuevas por segundo|11,331|11,502|
|DrawLightSource, us por llamada|1380,3|1189,8|
|Luz inclusiva, ms normalizados por tick|6,623|6,614|
|Capa0, ms por render|17,474|17,577|
|SCRIPT, ms por tick|13,224|12,401|
|Clear wait, ms normalizados por tick|2,439|2,925|
|Clear wait / duración de ventanas|5,51%|6,71%|
|Clear calls|1004|1516|

Como sensibilidad, excluyendo ventanas con `frame_max_us > 250000` quedan
27 ON / 31 OFF: presents 24,022 / 24,423; imágenes nuevas 12,014 / 12,211;
Clear wait 2,505 / 2,990 ms normalizados por tick. Este filtro excluye pausas
largas y también podría excluir carga real intensa; no es un benchmark pareado.

**No hay mejora de FPS demostrada por este A/B.** Distintas duraciones, luces,
SFX, enemigos y transiciones impiden atribuir la diferencia al cociente. La
optimización de divisiones continúa desactivada por defecto y en la build nueva.
No se identifica una ventana específica como linterna encendida sin marcadores.
Timings inclusivos: no sumar luz a capa11/GRAPH/MAP, ni SCRIPT a MAP. Clear puede
estar incluido en llamadas de juego; su normalización tampoco es tiempo exclusivo.
Presents incluye cuadros reutilizados, por lo que no equivale a imágenes nuevas.

## Cuello confirmado: Clear espera el siguiente bloque de audio

Todas las llamadas a Clear de los tramos medidos entran en espera aunque no haya
un lector ejecutándose en ese instante. El mixer permanece bloqueado en
`sceAudioOutOutput` durante parte del bloque de 1024 frames (23,219 ms a 44100 Hz).
El código anterior sólo reconocía Clear al volver a `FillBuffer`.

Ejemplos (índices de ventana desde cero):

|Log / ventana|Clear calls|Clear wait us|Ticks|
|---|---:|---:|---:|
|4 / 40|113|1026633|93|
|4 / 41|94|870430|95|
|5 / 41|102|1012914|92|
|5 / 42|114|977327|91|

Es una espera síncrona del llamador, no un error/underrun. Los dos logs mantienen
cero errores de apertura/decodificación/underrun M4A, output, Clear/Destroy timeout
y deadline misses. Existen late wakeups y mixer lock misses; cero errores no
sustituye una confirmación auditiva del usuario. Música sigue en M4A AAC original.

## Cambio mínimo y exclusión de lectores

`MixerGate.c/.h` añade un mutex que cubre **sólo** la llamada real a
`IOutputMixExt_FillBuffer`, incluidos sus callbacks. No cubre envío de audio,
mezcla de música, decodificación ni disco. No cambia tamaño, frecuencia, prioridad
ni afinidad de audio.

Clear conserva el mutex del player y **prueba** adquirir el gate sin esperar.
Si el gate está libre, la cola existe y el Track pertenece al player (o no hay
Track), ningún FillBuffer puede leer esos buffers: reinicia reader/avail, libera
los buffers de conversión, resetea front/rear/count/playIndex/pending y notifica
los waiters. Conserva `mFramesMixed` y el epílogo de interfaz. Devuelve éxito sólo
tras completar el vaciado. No invoca callbacks de buffers cancelados.

Si hay lector activo, Track discordante o array inexistente, conserva el Clear
anterior: petición, condvar, timeout de 100 ms y propiedad del buffer hasta ack.
Nunca espera el gate mientras retiene el mutex del player, evitando invertir el
orden gate→player que usa FillBuffer. Destroy no cambia.

El reconocimiento de Clear del **SDK instalado** se verificó en el desensamblado
de `IOutputMixExt.o`, `track_check+0x1d4..0x204`. Layout compilado:
queue356, count364, playIndex368, pending382, array384, front388, rear392,
playerTrack1076, trackReader8, trackAvail12. Coincide con los campos reiniciados.
No se reemplaza el mixer del SDK ni se copia código del port NOVA3; la copia local
de ese port sirvió para orientación, y el binario instalado es la verificación.

Se registra `clear_immediate_calls` junto a los contadores Clear existentes para
comprobar uso y esperas residuales en Vita. Son contadores por operación, no por
muestra/píxel. Se reemplaza el archivo OpenSL únicamente en el archive local de
cada build; el archive original del SDK permanece intacto.

## Verificación

- Cola real `IBufferQueue.c` + gate real: O0/O3 PASS. Conversiones PCM8/16,
  mono/stereo, liberación única inmediata, reset del Track sin perder framesMixed,
  200 handoffs con lector en otro hilo, timeout de propiedad, Track discordante,
  array ausente y player sin Track. El lector de estos tests modela el protocolo;
  no se presenta como ejecución completa del SDK en host.
- ASan/UBSan de cola + gate PASS; layout ABI del mixer instalado PASS.
- Música original: cinco M4A, EOF/loop, worker/cancelación/pausa/volumen/shutdown y
  salida PCM idéntica a FFmpeg host, O0/O3 PASS. No se crean archivos WAV de música.
- AAsset/cache concurrente y política reuse/logo O0/O3 PASS.
- 23 guards del SO + dos helpers, ABI de los tres wrappers de luz O0/O3 PASS.
- SO, tres APK/splits y 3642 archivos Data mantienen sus hashes de respaldo;
  cuatro bibliotecas FFmpeg originales del SDK mantienen sus hashes.
- Release/Debug SoftFP, VPK y ELF: resultados y hashes en `SHA256.json` del paquete.
- **FPS, ausencia de regresión auditiva y comportamiento de Clear en Vita: pendientes.**

Arquitectura NativeActivity/SIGE y música original sin cambios. Referencias previas
MetalSyntax Zenonia4 (separación lógica/render) y Advena/Inotia3 (audio) siguen
siendo contexto; no justifican offsets prestados. Trabajo local, sin publicación.

## Siguiente prueba física

Instalar la nueva Release de `Zombie-Shooter-Log45-Clear-2026-09-27`, manteniendo
Data/configuración/clocks. Jugar 2–3 minutos en el tutorial/escena usada para el
control: disparos sostenidos, explosiones, varios zombies y alternar la linterna.
Comprobar sonido completo, música continua y luces iguales. Volver al menú y jugar
otra vez para ejercitar stop/reinicio de SFX. Enviar **un log nuevo** y percepción
de bajones/audio; no repetir el A/B de divisiones por ahora. Debug queda para dump
si aparece un crash. El objetivo medible es Clear inmediato frecuente y menos
Clear wait sin timeout/errores; no prometer 30 FPS ni mejoras sólo por compilar.
