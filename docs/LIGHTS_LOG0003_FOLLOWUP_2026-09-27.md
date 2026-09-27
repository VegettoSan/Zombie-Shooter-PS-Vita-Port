# log_0003: ruta de luces y comparación controlada — 2026-09-27

Resultado del A/B recibido: [log0004/0005](LIGHTS_LOG0004_0005_FOLLOWUP_2026-09-27.md).
ON sí se ejecutó; no se demuestra mejora de FPS, AS2/stencil no participaron.

## Prueba física recibida

Usuario probó Release `local-d8b8c5e-a4990ab788`, alternando la linterna varias
veces y jugando encendida/apagada. **No percibió mejora respecto a la anterior.**
No se declara mejora de FPS. No informó una regresión nueva de audio/logo.
Su confirmación positiva de audio/logo corresponde a log_0002; log_0003 sigue
sin errores de apertura/decoder/underruns de música ni output/Clear/Destroy.
Clocks siguen500/222/222/166, Width864, mismo backend originalM4A.

Log `C:/Users/Mortar/OneDrive/Desktop/log_0003.log`, SHA256
`9e3f17a812df11580f3b1db801713990341b0329a63d8ce2e4e5643ed124fc5e`.
82ventanas PERF, copia y análisis JSON conservados en el paquete.

## Hallazgo confirmado

`light_hooks installed_mask=3`, pero **light_rows calls=0 en las82ventanas**.
El kernel NEON de AsmDrawLightWithZ instalado en el pase anterior no participó
en este recorrido. Esto explica por qué esa optimización no podía aportar una
mejora aquí. Su prueba de equivalencia continúa válida; no confundir corrección
aritmética con relevancia en la ruta ejecutada.

VID_LIGHT::Draw sí recibió33735llamadas, con43.014698s inclusivos acumulados.
La capa11 coincide casi íntegramente con su coste en varias ventanas:

| Ventana | Presents FPS | VID_LIGHT calls | ms/llamada de luz | Capa11 ms/llamada | Software ms/render | SCRIPT ms/tick |
|---|---:|---:|---:|---:|---:|---:|
|19|32.0|260|0.292|0.496|15.976|15.221|
|22|21.0|430|1.824|7.442|22.668|15.122|
|42|24.3|738|1.137|6.874|17.502|11.452|
|45|21.0|660|2.861|17.868|17.918|12.81|
|65|31.4|158|1.51|1.544|20.972|5.986|
|73|36.6|184|1.471|1.503|20.831|5.052|
|76|32.9|147|0.011|0.039|22.104|3.292|
|79|24.1|617|0.521|2.695|16.644|18.975|
|80|17.7|534|2.423|14.589|20.716|19.049|
|81|16.1|486|2.526|15.21|21.361|19.029|

Ventana81:486luces/81ticks, VID_LIGHT1227925us/capa111232088us, alrededor de
15.2ms/tick en luces; SCRIPT19ms/tick y software21.36ms/render también pesan.
Ventana45: capa11~17.87ms/llamada. En76 la luz cuesta11us/llamada; otras escenas
cuestan1.47–2.86ms. La capa0 ronda15–20ms/render en varios tramos.
Timings inclusivos/anidados: no sumar VID_LIGHT a capa11, GRAPH o MAP.
FPS presenta ticks/presents; con reuse no equivale a imágenes nuevas.
La variación de escenas/enemigos impide atribuir diferencias globales de FPS
exclusivamente a linterna. El log no marca cada pulsación: no etiquetar ventanas
con un encendido concreto ni inferir causalidad sólo por coincidencia.

Audio:19late_wakeups,0deadline_misses,0enqueue/outputerrors,0Clear/Destroytimeouts;
música0open/decodeerrors y0underrun_chunks. No cambió su implementación.

## Cambio acotado y experimento

Nueva `source/utils/light_pipeline.c` mide exactamente GRAPH::DrawLightSource,
DrawLightSource_AS2 y DrawShadowToStencil; devuelve el resultado opaco original.
Las3entradas guardadas por símbolo, offsetThumb y prólogo8bytes son:
0x41299c `af03b5f0 0f00e92d`,0x4132d0 `af03b5f0 8d04f84d`,
0x4134c4 `af03b5f0 0f00e92d`. Trampolines PC-independientes, arena comprobada.
Argumentos32bits float/Color preservados como palabras por ABI SoftFP; referencias
y bool conservados. Nuevas mediciones `light_pipeline source/AS2/stencil` permiten
separar generación legacy del camino AS2/stencil. No afirmar aún cuál domina:
VID_LIGHT llama DrawLightSource, que selecciona una rama dentro del original.

El original DrawLightSource contiene dos divisiones repetidas en la generación
de máscara, llamadas Thumb BLX en0x41302a y0x413216 hacia el helper ARM0x8b7c68.
Análisis y ejecución original confirman **división entera con signo**, cociente r0.
R1 no es un resto fiable ni una salida usada en esas continuaciones. No interpretar
las operaciones como división unsigned por apariencia del compare del denominador.
La ejecución de estos dos sitios en esta escena aún necesita el próximo log.

Experimento `ZOMBIE_LIGHT_DIVISION=ON`: cache fija512bytes/64entradas de
recíprocos, publicada una vez por entrada y nunca sobrescrita. Empty0/busy1/ready>=2,
CAS sólo inicialización, release publicación/acquire lectura. Si colisiona/está
ocupada o d0, se ejecuta el helper original; no mutex, spins ni expulsiones.
Se trabaja con magnitudes unsigned para evitar UB en INT_MIN. El cociente
`umulhi(abs(n),floor(2^32/abs(d)))` se corrige si resto>=abs(d); después se aplica
signo con negación unsigned. Resultado exacto, sin float ni aproximación visual.

Se modifican **sólo4bytes por sitio**: BL Thumb hacia veneer cercano que salta al
wrapper con interworking. Guardas incluyen símbolo/prólogo del contenedor,12/20
bytes alrededor de los sitios y32bytes del helper original; branch encoding y
rango se comprueban antes de cualquiera de las escrituras. El primer call conserva
el ITT CS original. Helper original y sus demás callers quedan sin parchear.
Se respeta r4–r11/VFP/SP; las siguientes instrucciones no consumen flags salientes.
Sólo RAM de SO cargada se modifica y se limpian caches; SO en disco intacta.

Release O3: wrapper completo warm, incluyendo cache/fallback,51→35,60→35 y96→34
instrucciones ARM para tres casos comprobados; no son ciclos ni FPS de Vita.
Cold entries añaden una división64bits una vez; colisiones añaden overhead de
fallback. No prometer una ganancia neta sin A/B. No hay counters/log por pixel;
`light_division cache_populated_entries_lifetime` demuestra uso del cache y evita
que contadores calientes oculten una ganancia pequeña. Si permanece0, el
experimento no participa y se sigue la rama AS2/stencil medida.

**Default CMake OFF hasta evidencia física**. Se entregan dos Release que sólo
difieren en ese experimento; ambas incluyen las mismas sondas. Control no modifica
los calls de división. Audio, logo, resolución, shaders baseline, script/IA,
cantidad de luces/zombies y calidad visual conservan la base existente.
Esto aborda una operación de una rama de luces; no cierra el cuello de software
capa0 oSCRIPT ni equivale a acelerar todo el sistema de luces/sombras.

## Verificación

- PASS O0/O3:1520pares contra el helper ARM real, signos/extremos/INT_MIN,
  cold/warm/colisiones, cociente idéntico y registros preservados.
- PASS ambos sitios reales: veneer/BL hacia delante/atrás, rechazo fuera de rango,
  ARM/Thumb y continuación original; primer call condicional ejecutado/omitido.
- PASS800kintentos concurrentes/8threads por nivel: publicación de cache y fallback.
- PASS wrappers reales contra mocks con float nativo SoftFP:7/10/4argumentos,
  bits float, Color, bool/stack, retorno original y registro de timing.
- PASS23hooks canonical+2helpers, engine trampolines; suites estabilidad O0/O3:
  AAsset/cache, OpenSL/Clear/Destroy, AACworker/5M4A/EOF/loop/pause/volume/cancel,
  y política de reuse del logo. Logs de rechazo de M4A inválido son fixtures.
- Release-divon,Release-divoff y Debug-divon BUILD/VPK VERIFIED. VitaGL clean por
  variante, flags baseline; ReleaseO3/DNDEBUG y DebugO0/g3, SDK SoftFP original.
  SímbolosMOV/AAC/profiling presentes en ELF, flags del experimento contrastados,
  ZIP y hashes verificados. Host no reproduce audio/scheduler/GPU de Vita.
- APK/splits/SO/3642Datafiles/4FFmpegSDKarchives intactos. HEADmasterd8b8c5e;
  cambios locales previos preservados. Sin commit/push/fetch/publicación.

## Paquete y prueba A/B

Carpeta `C:/Users/Mortar/Downloads/Zombie-Shooter-Log3-Lights-2026-09-27`.
Source identity `local-d8b8c5e-4b6548a578`; Release/Debug del experimento terminan
`-divon` y Control `-divoff`. Header distingue Debug/Release; no comparar sus FPS.

| Archivo | Bytes | SHA256 |
|---|---:|---|
|Zombie-Shooter-Log3-release.vpk|2527496|`bc2558c400fb62e9e3730cd79416ea2e79ec8a280aabe5a0f804b7ea99db703e`|
|Zombie-Shooter-Log3-release.elf|12356316|`75385b69996142c9958237013e2b9a82b7baeb3f792433d4f41fb10416f820c9`|
|Zombie-Shooter-Log3-control.vpk|2526454|`0a3e95412fa902bc718e11eb3c0f8e94526220044348d619f37db4d323b160bb`|
|Zombie-Shooter-Log3-control.elf|12354840|`d0ad8cc7c911af699385c06f82e2ddddcb7939147d6047ff357294ad318a8b2d`|
|Zombie-Shooter-Log3-debug.vpk|2600833|`473701f725e1670811aa412a1df589ddb921ad55def2da405f21a12919370005`|
|Zombie-Shooter-Log3-debug.elf|14525572|`6f234733fdb69f78de9615fd32428980fcba9ba6917477c7cf1aaede1ae5b8b8`|

1. Instalar Release (`Zombie-Shooter-Log3-release.vpk`) sobre ZOMB00001. Conservar
   saves/Data/SO/5M4A/config y clocks500/222/222/166. Reiniciar aplicación.
2. En misma zona tranquila/posición/cámara: linterna apagada20s → encendida20s →
   apagada20s; comprobar que sombras/luces siguen iguales y audio/logo correctos.
3. Instalar Control (`Zombie-Shooter-Log3-control.vpk`), reiniciar y repetir esa
   misma escena/config/orden. La aplicación sólo tiene un VPK instalado cada vez.
4. Devolver **máximo dos logs completos**, uno Release y otro Control, indicando
   cuál corresponde a cada uno. Si sólo hay tiempo para uno, usar Release primero.
   Horda/explosiones opcionales después del bloque tranquilo, no mezclarlas con
   la comparación directa de linterna.

Esperado: pipeline_mask7 ambos; divisionmask3/caches pobladas en Release si rama
legacy ejecuta sus divisiones, divisionmask0 en Control. Source vsAS2/stencil
permitirá decidir si la optimización es relevante o hay que centrarse en otra
rama. Si hay crash, Debug+ELF correspondiente y dump, sin usar Debug para FPS.
Nueva ganancia de FPS y aspecto visual en Vita **PENDIENTES**. La prueba anterior
queda registrada como **sin mejora visible**, con kernel de fila inactivo.
