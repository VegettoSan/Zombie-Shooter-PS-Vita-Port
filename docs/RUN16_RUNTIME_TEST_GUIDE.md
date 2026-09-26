# Run #16/#17 — guía de prueba runtime

Rama: `fix/run16-disable-render-scale`

Esta rama prueba resolución, emulación de mando Xbox y backend de música sin modificar `master` runtime.

## 1. Resolución

Archivo:

`ux0:data/zombieshooter/config.txt`

Probar en este orden, reiniciando completamente el juego entre cambios:

```text
software_width 0
```

Baseline seguro. No instala el hook de render-scale.

Luego, sólo si el juego inicia de forma estable:

```text
software_width 960
```

Y sólo si 960 funciona:

```text
software_width 864
```

No hace falta reinstalar el VPK entre valores.

## 2. Música: A/B original M4A vs PCM16

El mismo `config.txt` acepta:

```text
music_mode 0
```

Default seguro de Run17. **No instala** el hook de `createMusicPlayer()` de `audio_stream.c`; los WAV PCM16 no se buscan ni se abren por ese hook y el motor conserva su ruta original hacia los M4A.

Este modo sirve primero para aislar el crash. No implica que la implementación OpenSL actual vaya a reproducir audio M4A correctamente: el backend AndroidFD/M4A sigue siendo una tarea independiente.

Para reactivar el comportamiento anterior de sidecars PCM16:

```text
music_mode 1
```

En este modo se instala el hook y las cinco pistas conocidas se redirigen a los WAV PCM16 si son válidos.

Los archivos M4A originales deben conservarse. No es necesario borrar los WAV para probar `music_mode 0`; simplemente no serán solicitados por `audio_stream`.

### Interpretación

- `music_mode 0` también crashea en el mismo punto: el hook de música y los WAV quedan fuertemente descartados como causa de este startup crash.
- `music_mode 0` inicia pero `music_mode 1` crashea: investigar el hook/trampoline de `createMusicPlayer`, OpenSL y/o la ruta PCM.
- Si el log no contiene `audio_stream requested=...`, ningún WAV fue solicitado por el hook antes del crash.

## 3. Controles Xbox

Archivo:

`ux0:data/zombieshooter/controls.txt`

Se crea automáticamente la primera vez con el perfil estándar:

```text
cross A
circle B
square X
triangle Y

l LB
r RB
rear_left LT
rear_right RT

start START
select BACK

dpad_up DPAD_UP
dpad_down DPAD_DOWN
dpad_left DPAD_LEFT
dpad_right DPAD_RIGHT

l3 LS
r3 RS
```

Valores Xbox válidos:

`A B X Y LB RB LT RT LS RS START BACK DPAD_UP DPAD_DOWN DPAD_LEFT DPAD_RIGHT NONE`

El archivo es case-insensitive. Una entrada ausente conserva el default. `NONE` deshabilita esa entrada física.

`controls.txt` tiene prioridad sobre el viejo `vita_shooter`. El valor de `vita_shooter` queda sólo como fallback de compatibilidad y no debe usarse para estas pruebas.

DS3/DS4 no pasan por este remapeo.

## 4. Prueba diagnóstica recomendada de botones

Primero usar el perfil estándar sin modificarlo y anotar qué hace físicamente cada botón.

Si `Cross -> A` sigue sin funcionar, cambiar **sólo** una línea para determinar si falla el botón físico o la acción Xbox A. Por ejemplo:

```text
cross LB
```

Reiniciar y probar Cross. Después se puede probar `cross RB`, `cross X`, etc. Volver a `cross A` al terminar.

Para investigar el problema observado de L activando linterna y granada a la vez, comenzar obligatoriamente con:

```text
l LB
rear_left LT
```

No empezar con `l LT`, porque el objetivo inicial es separar hombro y trigger y comprobar si la duplicación desaparece.

## 5. Primera prueba después de Run17

Usar Release y dejar:

```text
software_width 0
music_mode 0
```

Mantener `controls.txt` generado sin modificar. Reiniciar completamente la app.

El log correcto debe mostrar aproximadamente:

```text
[PERF] audio_stream installed=0 music_mode=0 backend=original_m4a pcm_hook_skipped=1
[PERF] work_resolution installed=0 requested_width=0 mode=original hook_skipped=1
```

Si vuelve a crashear, enviar:

1. log completo de esa ejecución;
2. `.psp2dmp` completo, no un `.tmp` incompleto si es posible;
3. `Zombie-Shooter-Vita-Release.elf` de **ese mismo workflow**.

El workflow de esta rama ahora empaqueta los ELF Debug/Release junto a los dos VPK para poder resolver PC/LR con exactitud.

## 6. Logs de controles

Release registra el mapping Xbox efectivo una vez al iniciar con una línea `[PERF] [INPUT] xbox_map ...`.

Debug además registra cambios de máscara física Vita -> máscara Xbox y FalsoNDK registra transiciones de KeyEvent. Esto permite correlacionar una pulsación física con el control lógico y el evento enviado.

## 7. Qué devolver después de la prueba

Para cada resolución: indicar si inicia, carga menú/tutorial/gameplay y FPS aproximados.

Para controles: indicar la acción real observada para Cross, Circle, Square, Triangle, L, R, rear-left, rear-right, Start, Select y D-pad.

Para música: indicar `music_mode`, si se oye algo y si aparecen líneas `audio_stream requested`/`audio_uri`.

Si hay comportamiento duplicado o un botón que no responde, adjuntar el log Debug de esa misma configuración. Si hay crash, adjuntar también el `.psp2dmp` y el ELF correspondiente.
