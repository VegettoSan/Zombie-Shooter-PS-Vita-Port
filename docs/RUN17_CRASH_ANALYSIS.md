# Run17 — crash con resolución original y controles Xbox runtime

Fecha: 2026-09-26
Rama: `fix/run16-disable-render-scale`
Build físico: `fe1fbbabc28941022bd6d0743b14c79c1a300126`

## Evidencia recibida

- `log_0017.log`
- `psp2core-1790461014-0x00001d2969-eboot.bin.psp2dmp.tmp`

El core recibido termina en `.tmp` y está truncado/incompleto como archivo ELF de dump, aunque conserva suficientes notas para recuperar el hilo que falló y sus registros. El workflow se actualizó posteriormente para incluir los ELF Debug/Release exactos en cada entrega.

## Estado confirmado por el log

El arranque usó correctamente el mapping Xbox generado:

```text
[PERF] [INPUT] xbox_map source=generated_controls.txt emulation=xbox cross=A circle=B square=X triangle=Y l=LB r=RB rear_left=LT rear_right=RT ...
```

La prueba de resolución estaba realmente en baseline:

```text
[PERF] work_resolution installed=0 requested_width=0 mode=original hook_skipped=1 expected_surface=1024x580
```

Por tanto el hook de `render_scale` NO estaba instalado.

El motor llegó a crear la superficie esperada original:

```text
[PERF] software_surface width=1024 height=580 stride_bytes=4096 bytes=2375680 caller=0x9846BF59
```

Ésta es la última línea del log antes del crash.

## Música

El build antiguo registró al inicio:

```text
[PERF] audio_stream installed=1 original_extension_mapping=ogg_to_m4a pcm_sidecars=5
```

Esto sólo confirma que el hook de `createMusicPlayer` fue instalado. En `log_0017` NO aparecen antes del crash:

- `audio_stream requested=...`
- `audio_uri ...`
- apertura de un WAV por el hook
- una petición de pista M4A/WAV registrada por esta ruta

Por tanto no existe evidencia de que los WAV PCM16 grandes hayan sido abiertos, decodificados o cargados en memoria antes de este crash. Su mero tamaño en almacenamiento no explica el fallo observado.

Aun así, una corrupción indirecta causada por instalar el hook/trampoline de música debe descartarse mediante A/B. Por eso el siguiente build añade `music_mode`:

```text
music_mode 0
```

- default seguro;
- `audio_stream_install()` retorna antes de resolver/parchear `createMusicPlayer`;
- no se reserva trampoline para ese hook;
- no se buscan WAV;
- el motor conserva su ruta original M4A.

```text
music_mode 1
```

- reinstala el comportamiento PCM16 anterior.

`music_mode 0` es primero una prueba de aislamiento. El backend OpenSL actual no implementa por sí solo reproducción AndroidFD/M4A, de modo que silencio en modo 0 no invalida la prueba.

## Core

Hilo que falló: `pthread`.

```text
stop_reason = 0x30004  (Data abort)
PC          = 0x8101B9EA = so_loader + 0x1B9EA
LR          = 0x81024081 = so_loader + 0x24081
DFSR        = 0x000000C7
DFAR        = 0x00000024
R0          = 0x00000024
R1          = 0x00000000
R2          = 0x00000000
R11         = 0x00000000
```

`DFAR=0x24` es consistente con un acceso tipo `NULL + 0x24`, pero no se debe afirmar la instrucción/objeto exactos sin simbolizar el ELF correspondiente.

## Correlación con Run16

Run16 anterior:

```text
PC = so_loader + 0x19DFA
LR = so_loader + 0x22491
DFAR = 0x24
```

Run17:

```text
PC = so_loader + 0x1B9EA
LR = so_loader + 0x24081
DFAR = 0x24
```

Ambos PC/LR se desplazaron exactamente:

```text
0x1B9EA - 0x19DFA = 0x1BF0
0x24081 - 0x22491 = 0x1BF0
```

Como el ejecutable recibió código nuevo entre ambos builds, el desplazamiento idéntico de PC y LR junto con el mismo `DFAR=0x24` es evidencia fuerte de que se trata del mismo sitio lógico de crash relocalizado dentro del nuevo `so_loader`, no de un fallo completamente nuevo introducido por el tamaño de los WAV o por `software_width 0`.

## Otras líneas del log

Antes del crash siguen apareciendo advertencias conocidas de compatibilidad JNI, AES, VID, grid y KeyBinder. No asignarles causalidad sólo por proximidad. La superficie de software se crea después de esas líneas y el dump apunta al ejecutable `so_loader`, por lo que el siguiente paso es simbolizar el PC exacto del nuevo build y hacer A/B de hooks, no adivinar por el último mensaje de error del motor.

## Próxima prueba obligatoria

Release del nuevo workflow, con:

```text
software_width 0
music_mode 0
```

`controls.txt` generado, sin modificar.

Resultado esperado en el log:

```text
[PERF] audio_stream installed=0 music_mode=0 backend=original_m4a pcm_hook_skipped=1
[PERF] work_resolution installed=0 requested_width=0 mode=original hook_skipped=1
```

Si vuelve a crashear en el mismo punto, el hook PCM/WAV y render-scale quedan fuertemente descartados y debe continuar el aislamiento sobre los demás hooks comunes (engine probes, palette/alpha u otra ruta de loader) usando el ELF Release exacto.

Si inicia con `music_mode 0` y falla con `music_mode 1`, concentrar la investigación en `audio_stream_install`, su trampoline, `createMusicPlayer` y OpenSL/PCM.

## Reproducción M4A nativa futura

Conservar siempre los `.m4a` originales. VitaSDK expone decodificación AAC mediante `SceAudiodec` y un reproductor multimedia `SceAvPlayer`. Una implementación nativa podría evitar los ~77 MiB de WAV PCM16, pero debe desarrollarse como tarea separada: `SceAudiodec` proporciona AAC y no debe asumirse que demultiplexa por sí mismo el contenedor M4A; `SceAvPlayer` es candidato para manejar una fuente de archivo completa. No introducir esta reescritura mientras se aísla el crash de startup.
