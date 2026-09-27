> Actualización log_0003: usuario no observó mejora visible; el kernel instalado
> registró0calls en todo el recorrido. VID_LIGHT y capa11 sí tienen coste alto.
> Ver [seguimiento y siguiente comparación](LIGHTS_LOG0003_FOLLOWUP_2026-09-27.md).

# Seguimiento físico log_0002 y luces — 2026-09-27

## Resultado confirmado por el usuario

Vita física, Release `local-d8b8c5e-f41edd8b67`. Usuario con overclock al máximo;
header confirma clocks CPU/BUS/GPU/XBAR 500/222/222/166 MHz, software_width864,
framebuffer_5650, software_frameskip1, music_mode2, audio_frames1024 y cache8MiB.

- Logo Sigma Team: **sin parpadeo** en esta prueba.
- Música original M4A: **audible y sin cortes**, según el usuario.
- Efectos: **sin cortes ni tartamudeo**, según el usuario.
- FPS: usuario percibe mejora ligera; continúan bajones con zombies, explosiones,
  linterna y luces del escenario. **No hay FPS estables confirmados**.

Estos resultados sustituyen los estados pendientes de logo/audio en el informe
AUDIO_LOGO_FOLLOWUP. No prueban todas las transiciones, bucles, volumen o pausas.
Log origen `C:/Users/Mortar/OneDrive/Desktop/log_0002.log`, SHA256
`e635401368425c5599d0e30a458a12b0fbdd3687ab79a83cc1e72e2b62749184`.
Se conserva copia en el paquete nuevo. 95 ventanas PERF de aproximadamente5s.

## Lectura del log

MOV/AAC capabilities1/1, mixer prioridad96/affinity0x20000 confirmados, hooks
MAP0xFFF. En las ventanas:0music_open_errors,0decode_errors,0underrun_chunks,
0enqueue_errors,0output_errors,0Clear/Destroy timeouts,0deadline_misses.
Hay29late_wakeups menores que dos períodos; no describir scheduler perfecto.
Audio correcto confirmado por escucha del usuario, no únicamente por counters.

| Ventana PERF (1-based) | Presents FPS | Software ms/render nuevo | SCRIPT mainLoop ms/tick | GRAPH ms/tick |
|---|---:|---:|---:|---:|
|1|58.9|3.135|0.522|6.859|
|20|25.8|20.801|15.140|7.087|
|30|28.8|17.773|14.073|5.464|
|60|26.9|17.095|11.541|10.133|
|90|23.3|18.896|18.195|4.193|
|91|19.6|21.848|18.591|4.249|
|95|25.8|18.176|18.038|4.212|

La ventana91 presenta99ticks y49renders en5029ms:19.6presents/s pero9.7
imágenes software nuevas/s. La reutilización reduce carga, no duplica imágenes
reales. MAP es inclusivo y contiene GRAPH/software/script; no sumar sus totales
como si fueran etapas independientes. Los scopes que cruzan fronteras PERF
pueden tener desfase entre llamadas y tiempo de una ventana.

En91, preparePalette promedia7us por muestra y draw_impl49us: sus estimaciones
inclusivas17.2ms/117ms por ventana no explican los1070.6ms de software completo.
Esto justifica medir otras capas y luces; no demuestra que todo tiempo restante
pertenezca a iluminación. SCRIPT cuesta18.6ms/tick y callFunction muestreado1/64
sigue siendo demasiado agregado para atribuir costes por entidad. No se reduce
su frecuencia ni la cantidad de enemigos. Las ventanas no etiquetan cuándo el
usuario encendió la linterna: no inventar una correlación temporal exacta.
Hay caídas durante cargas; no presentar el mínimo de todas las ventanas como
mínimo FPS de combate ni comparar recorridos distintos como benchmark controlado.

## Cambio local de esta entrega

### Cálculo exacto del mask de profundidad de luces

Nueva `source/utils/raster_light.h/.c`: rutina NEON de8pixels para
`AsmDrawLightWithZ(unsigned char*, unsigned char*, unsigned short*, unsigned short*, int, unsigned short)`.
SO canónica, símbolo `_Z17AsmDrawLightWithZPhS_PtS0_it`, Thumb0x510b01,
entry0x510b00, prólogo `af03b5f0 0b00e92d`, verificado antes de modificar RAM.
Trampolín copia8bytes PC-independientes; no se modifica SO en disco.

Disassembly original trata profundidades como int16 **con suma y resta int32**:
`delta = signed(source_z) + signed(depth) - signed(scene_z)`.
Resultado0 si delta<0; `source | 15` si delta>127; en0..127,
`source | ((delta >> 3) << 12)`, reducido a16bits. NEON conserva precisamente
este comportamiento; no saturar delta a16bits ni cambiar el umbral127/128.
Filas16..16384 alineadas y sin solapamiento destino/entradas usan NEON; restantes
usan original, incluidas filas pequeñas, alineación rara y alias. Colas escalares
exactas. Bloques totalmente rechazados no leen source color.

O3: para128pixels, ejecución de instrucciones ARM original→NEON:
edge2384→816, random2016→816, totalmente rechazado1677→768.
Son instrucciones de kernels aislados, **no ciclos ni FPS medidos en Vita**.
O0 es más lento (3731–6291 instrucciones), por lo que Debug sirve para diagnóstico,
no para comparar rendimiento. Release real contiene instrucciones NEON verificadas.

La frecuencia/coste de esta ruta todavía no aparece en el log antiguo: podría
usarse poco o nada en la escena concreta. El código GRAPH::DrawLightSource también
contiene generación de textura/mask y otra ruta AS2/stencil; no están reemplazadas
por esta optimización. No afirmar que esta entrega haya acelerado todo el sistema
de iluminación o solucionado ya las hordas. `light_rows` del siguiente log mostrará
calls/fast_rows/pixels/sampled_us para comprobar uso real.

### Separación de capas y luces

El hook existente de SPRITE_COLLECTOR::DrawLayer, guarded Thumb0x4f4b21,
se reutiliza para medir por índice de capa; **no se parchea dos veces**. Tabla fija
17buckets (0..15 y otros), sin asignaciones, mutex o log por llamada.
Se añade hook guarded VID_LIGHT::Draw, Thumb0x511879, prólogo
`af03b5f0 0f00e92d`, conservando original y argumentos. Tiempo exacto inclusivo
por llamada. `scene_draw` informa calls/us/avg/max por capa y VID_LIGHT cada5s.
Costes anidados: no sumar VID_LIGHT a su DrawLayer ni a software.
Nuevos hooks `light_hooks installed_mask=3 expected_mask=3`.

Audio, FFmpeg privado MOV/AAC, prioridad del mixer y política del logo quedan en
la misma base funcional probada. No se generan sidecars musicales WAV/OGG ni se
cambia resolución/calidad/cantidad de luces o zombies. No se introduce limiter
conSCRIPT18.6ms y software21.8ms: falta margen para prometer30FPS estables.

## Validación y artefactos

PASS kernel ARM O0/O3:288casos por nivel con original canónico, scalar y NEON;
63casos adicionales de entrada parcheada por nivel verifican6argumentos,
ARM/Thumb, SP, registros preservados, guardas, alias, filas cortas y fallback.
PASS engine_probe y20hooks canonical+2helpers. PASS estabilidad O0/O3:
cache/AAsset concurrentes reales, OpenSL conversion/Clear/Destroy y worker/decoder
music/EOF/loop/pause/volume/cancel/shutdown,5M4A byte-exact frente a FFmpeg host,
y política de reuse del logo. Estas pruebas no simulan scheduler/audio físico.

Release O3/DNDEBUG y Debug O0/g3 efectivos compilados SoftFP SDK
`/usr/local/vitasdk`. VitaGL limpio por variante, baseline flags. Símbolos MOV/AAC
y light instalador/report presentes en ambosELF, sin ABI argumentos VFP hardfp;
VPK ZIPintegrity PASS. APK/splits/SO y3642Datafiles hashes intactos,4archivos
FFmpeg del SDK intactos. No commit/push/fetch/merge/publicación en repo usuario.

Build **`local-d8b8c5e-a4990ab788`**, carpeta Windows:
`C:/Users/Mortar/Downloads/Zombie-Shooter-Lights-2026-09-27`.

| Archivo | Bytes | SHA256 |
|---|---:|---|
|Zombie-Shooter-Lights-release.vpk|2525571|`ff364c1eaaf6585205e0fc6d8c820951214935c7dcf78bc851c75e7dacd2759d`|
|Zombie-Shooter-Lights-release.elf|12351680|`682483ee98e3a34c4f54d1ce3bb47d4392893b040ad9cd70ce723d87e9c03be2`|
|Zombie-Shooter-Lights-debug.vpk|2598646|`8b398983752f6b74eacbd9a06e55f0a324e3640d5c59f2d9215d1835cedbd6de`|
|Zombie-Shooter-Lights-debug.elf|14511764|`4a31de8ddeb2342ee7d913293fb83817386a223b9db57394755848bef85ae06e`|

## Próxima prueba física

1. Instalar `Zombie-Shooter-Lights-release.vpk` sobre la aplicación ZOMB00001
   usando VitaShell; conservar saves/Data/SO y los5M4A originales.
2. Mantener misma configuración y overclock que log_0002; reiniciar aplicación.
   music_mode2, audio_frames1024, asset_cache_mib8, software_width864,
   framebuffer_5650, software_frameskip1.
3. Comprobar logo/audio primero. En zona tranquila, mantener cámara/posición:
   linterna apagada20s → encendida20s → apagada20s. Anotar ese orden.
4. Repetir recorrido con horda/explosiones y luces del escenario. No comparar
   Debug contra Release ni cambiar resolución/overclock en mitad de la prueba.
5. Devolver log completo nuevo. Esperado buildID nuevo, lightmask3, scene_draw
   por capas y VID_LIGHT, y light_rows. Si calls0, la optimización de fila no
   participa en esa escena; se trabaja entonces sobre las rutas medidas reales.

Audio/logo permanecen **confirmados en la build anterior probada**; estabilidad
y mejora de FPS de esta build nueva, luces visualmente iguales y hooks activos
permanecen **pendientes de Vita física**. No prometer30FPS ni declarar cerrada
la optimización de zombies/scripts/iluminación.
