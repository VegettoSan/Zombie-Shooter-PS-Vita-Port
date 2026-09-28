# Pase de ingeniería local — 2026-09-28

El resultado de este pase es una corrección de durabilidad, una protección de
reutilización del framebuffer, una caché de misses medida por operaciones y
builds diagnósticas de los bloqueos restantes. **No se ha probado esta build en
una Vita física. La campaña, el mapping gameplay y el parpadeo siguen abiertos.**

Referencia de checkout: `5f67eec3a73952dae9c541a2253501804ebe03d7`.
Baseline físico: `5e246c97bbf1a8de4858abb4843757dd980ed271`.
La suciedad previa del árbol principal era de finales de línea; se preservó el
trabajo existente. No se hizo pull, fetch, commit, push, merge, rebase, reset ni
clean. No se publicaron resultados. La SO canónica conserva SHA-256
`5e5e2e1bbe86e126c3e2dce5ad97c2e9dc6282305ea7d3e24b49ee4769c5b5b7`.

## Evidencia original y alcance

Se leyeron los doce documentos exigidos, además del pipeline reproducible.
Se localizaron XAPK, APK base 3.5.3/versionCode 1153, split ARM y assets bajo
este checkout. Se desensamblaron los seis DEX con baksmali y se buscaron las APIs
de persistencia en todos ellos. `engineering-evidence-2026-09-28/dex-manifest.json`
vincula los DEX extraídos byte por byte al APK. Los matches amplios incluyen
ads/billing, metadatos y nombres comunes: su cantidad no demuestra uso de save.
La búsqueda focalizada SIGE está en `dex-sige-persistence.txt`.

Los 2.432 archivos `assets/` del APK coinciden byte por byte con `data/assets/`.
No se mezclaron los datos del juego PC de `G:\Games\Zombie Shooter`.
Se preservaron NativeActivity, JNI/NDK, assets, SFX y M4A AAC originales.

## Hipótesis y decisiones

| Problema | Evidencia / diferencia | Cambio y riesgo | Verificación pendiente |
|---|---|---|---|
| No aparece save | SO usa RegistryPrivate -> Activity.getPreferences(0); el reporte físico reciente no tiene traza de puts | Logs nativos/JNI/disk separados y acotados; permiten distinguir antes de cifrado, antes de Java y fallo de disco | Avanzar y relanzar completamente |
| Pérdida de un store previo | Se hacía remove(main) antes de comprobar rename(tmp,main) | Temporal sincronizado, rotación a backup, recuperación y sync de rename; mantiene el formato existente | Durabilidad de filesystem Vita y descifrado real |
| Digital incorrecto | Sticks funcionan; códigos actuales coinciden con constructores ARM y filtro nativo | Observación physical -> emitted -> KeyboardControl -> KeyBinder; no se cambia el mapping por una conjetura | Aceptación y acción real por botón |
| Supuesto scan/meta faltante | Camino digital nativo consume action/keycode; no importa scan/repeat/meta | Mantener semántica de eventos; registrar los campos emulados | No atribuir la causa a campos no consultados |
| Flicker y frame reuse | Gate anterior comprobaba caller/shape, no textura/productor ni redefinición | Gate por textura, buffer y shape; delete/redefine invalidan y fuerzan nuevo productor | Objetos concretos y patrón en hardware |
| Flicker COW/GPU | Sin identificación de objetos afectados; COW ya difiere del upstream | A/B full-copy y finish conservando allocation/deferred free; defaults off | A/B de un mismo objeto y escena |
| Luces costosas | Log4/5: source activa, AS2/stencil/scanline cero; no speedup probado con divisiones | Preservar optimización exacta opt-in OFF y profiler real; no otra sustitución sin medición pareada | Linterna OFF/ON y luces/explosiones comparables |
| Carga | Miles de fallos; repetición fuera de los siete índices, especialmente i18n// | Caché exacta de ENOENT, 256 entradas, por proceso; sin precargar assets | Cold-start A/B y coste de hits en Vita |

## Guardado: contrato demostrado y límite

`classes4/com/sigmateam/sige/RegistryEnumerator.smali` llama exactamente a
`Activity.getPreferences(0)`, obtiene `getAll().keySet()` y llama al native
`onKey(String)`. La SO registra `(Ljava/lang/String;)V` en su constructor
`RegistryPrivate` (`0x3f4a00`). `RegistryPrivate::getString` (`0x3f4b70`) resuelve
`getPreferences(I)`, `getString(String,String)`; `setString` (`0x3f4e2c`) usa
`edit()`, `putString(String,String)`, y los caminos `apply()V`/`commit()Z`.
`Registry::storeValue` (`0x3eb08c`) cifra el valor **antes** de llamar al backend;
si `crypto::encrypt` falla, no llega a putString. En lectura `loadValue`
(`0x3eb100`) descifra y valida el tag `0x47415443`; el fallback no prueba que
la campaña esté ausente del disco. Se identificaron AES-256/base64 para valores
y scramble de claves con Salsa. El port conserva esos valores opacos y deja
el cifrado/validación originales en la SO. No cambia identidad, package name,
Android ID, parámetros de cifrado ni el formato `ZSPREF1` previo del port.

El registry es un backend local real del motor, no una hipótesis basada sólo
en strings. **No se ha demostrado todavía qué conjunto concreto de claves
contiene toda la campaña ni que todos los scripts de campaña alcancen este
backend.** `game_session_state`, metadata de save slots y dumps del ScoreSystem
son pistas nativas; los snapshots GPG/backend son otra ruta y no deben
confundirse con el guardado local. `Profile` lee configuración como game.cfg;
eso tampoco demuestra que sea un save de campaña. No se encontró un backend
SQLite/FileOutputStream de campaña en las clases SIGE inspeccionadas.

Log4/5 tienen GetMethodID(getPreferences) no resuelto, pero anteceden al intento
reciente de SharedPreferences. Esos logs explican aquellas ejecuciones;
**no explican por sí solos** por qué no apareció el archivo en la prueba más
reciente. También hay ausencia de PackageManager/PackageInfo y certificado
inválido. No se parcheó esa validación ni se convirtió en la causa sin seguir
su efecto sobre el save. La traza nueva registra `Registry.loadValue`,
`Registry.storeValue`, resultado de cifrado y `RegistryPrivate.setString`, con
offset/símbolo/prologue guards y trampolines que no reescriben código por evento.

`preferences_store.inc` escribe sólo cuando el motor ha hecho cambios reales.
No crea un fichero vacío al boot ni una campaña artificial. Escribe tmp,
fflush, sync, close; rota el main validado a `.bak`; reemplaza y sincroniza el
rename. Si falla el reemplazo, restaura o retiene el backup legible. Un main
truncado recupera el backup al siguiente proceso. El backup puede ser el commit
anterior, no promete recuperar progreso que nunca se confirmó. Se rechazan
stores estructuralmente incompletos, claves duplicadas y NULs incrustados.
La integridad semántica/cifrada sigue correspondiendo al motor original.

La instrumentación Debug diferencia reads (64) de puts/commits y operaciones
disk (presupuestos independientes). Registra hashes de claves/longitudes, no
payloads de la campaña. Logs `[SAVE] open/create/write/rename/unlink/stat/fsync`,
preference get/put y commit/apply no se ejecutan por frame de forma ilimitada.
Los wrappers de imports disk sólo se seleccionan en Debug; conservan el backend
SceLibcBridge/FalsoNDK y errno. La API Activity no se amplió indiscriminadamente
con paths Android inventados.

## Input: contrato y diagnóstico entregado

La SO usa `InputHandlerNative::onInputEvent` (`0x3ce038`). KEY gamepad puede
pasar por JoystickControl, que rechaza tipo KEY; después se procesa por
`AndroidKeyboardControl::handleEvent` (`0x3d0774`). DOWN=0 produce estado 1,
UP=1 estado 0, MULTIPLE=2 se rechaza. Se consulta `AKeyEvent_getKeyCode` y se
filtra con `handleKey` (`0x3d0578`); se encola un byte de code y el estado.
`refresh` entrega ese code/state a `KeyBinder::setKey` (`0x424520`).
La prueba Unicorn ejecuta el filtro ARM original, no una réplica C.

Los constructores de `Keyboard::GAMEPAD_*` en init-array `0x3cff2d` usan:

| Control | Android code | Target mostrado por el juego |
|---|---:|---|
| A / Cross | 96 | Select/Continue |
| B / Circle | 97 | Back |
| X / Square | 99 | Sin acción gameplay declarada |
| Y / Triangle | 100 | Buy ammo |
| LB / L | 102 | Next weapon |
| RB / R | 103 | Previous weapon |
| LT / rear-left | 104 o axis 17 | Medkit |
| RT / rear-right | 105 o axis 18 | Grenade |
| D-pad up/down/left/right | 19/20/21/22 o HAT | Medkit/Grenade/Prev/Next |

Las teclas de la tabla actual coinciden con esos códigos. El native no consulta
scanCode/metaState/repeatCount en esta ruta; no se agregaron campos arbitrarios
para tratar síntomas. Device id 1, sources y onAxisInfo conservan el contrato
de InputDeviceHelper del DEX. BRAKE/GAS siguen como aliases de trigger de la
emulación existente; no se quitaron sin demostrar cómo los bindings del motor
los consumen. Strict/hybrid y controles externos mantienen comportamiento.

Debug registra transiciones físicas y lógicas, evento entregado con device,
source, type, action, code, scan, repeat, meta emulado=0; cambios HAT/triggers,
buttonState emulado=0; aceptación real del KeyboardControl y llegada a Binder.
**Esto es una build de diagnóstico del problema digital, no un mapping gameplay
corregido.** Si la aceptación/code/state son correctos y una pulsación sigue
activando dos acciones, el siguiente objetivo son las combinaciones/actions del
Binder y los scripts, no las teclas PC ni touch sintético.

## Renderer

Un upload del framebuffer puede omitirse sólo después de uno completo de la
misma textura/productor/dimensiones. `glTexImage2D` y `glDeleteTextures`
invalidan la reutilización; la siguiente softwareTact se produce normalmente.
Otro upload ajeno invalida conservadoramente la elegibilidad. Los logs informan
rechazos del gate, ID/productor y hasta 64 tuplas distintas de textura, caller,
shape y formato para separar framebuffer dinámico de sprites/paletas.
No se alteran alpha, paletas, gamma, calidad ni efectos.

**La causa visual del flicker aún no está identificada.** Este defecto estático
del gate no demuestra causalidad con los sprites descritos. Si los buffers
cambian sólo en un tick reutilizado, el gate hace el upload original y lo
registra; esa transición requiere observarse en hardware. La mejora puede
reducir reutilización durante cambios de textura; no se promete un speedup.

`render_diagnostics` es una máscara, default 0:

- 1: glFinish antes de updates (diagnóstico de ownership/GPU wait, lento).
- 2: COW con copia completa original (misma asignación/deferred free).
- 4: subir también frames software reutilizados, aislando el skip de upload.

Mantener un bit por prueba. `framebuffer_565 1` sigue migrándose a 0 por la
regresión física anterior. La variante corregida y segura para diagnóstico es
2; no se reinstala el RGB565 antiguo como si fuera un A/B válido.

## Perfil real de luces y FPS

Se recalcularon los logs físicos originales de Desktop, con hashes y contadores
en `hardware-log-summary.json`. DrawLightSource: 18.208 llamadas/25.132.972 us
en log4; 23.987/28.540.267 us en log5. AS2 y stencil: cero en ambos. Los
promedios son 1.380,3/1.189,8 us por llamada, inclusivos. El informe previo
normaliza luz a 6,623/6,614 ms por tick y capa0 a 17,474/17,577 ms por render.
SCRIPT: 13,224/12,401 ms por tick. No sumar tiempos inclusivos anidados.

Log4 era divisiones ON y log5 OFF, **no una escena pareada linterna ON/OFF**.
22,572/22,945 presents/s incluyen reuse; imágenes software nuevas 11,331/11,502.
No demuestran mejora por divisiones. No se volvió a optimizar la scanline
`AsmDrawLightWithZ` sin usos. La optimización exacta existente sigue opt-in OFF.
Se preservan el profiler source/AS2/stencil, MAP/GRAPH/SCRIPT/layers, gamma,
uploads, CPU thread y Clear/FillBuffer. Tampoco se reatribuyó como trabajo nuevo
el fix anterior del gate de Clear que evitaba esperar el siguiente audio block.
**No hay un nuevo incremento de FPS o una reducción física de luz demostrados
por este pase.** Para otra sustitución aritmética hace falta medir la subruta
activa en escenas marcadas y comparables.

## Carga: cambio sustentado por opens reales

Las ventanas reportadas suman 4.632/4.637 opens fallidos, 2,795/2,780 s de
tiempo medido de fallo en log4/5. Los opens exitosos suman 64,592/59,505 s,
pero los bloques abarcan cargas/transiciones y no son un cronómetro de startup.
La tabla limitada reporta `i18n//vid/empty.vid` 88/116 veces y
`i18n//wav/null.wav` 23/23; hubo overflow, por lo que no es inventario completo.

Se añadió caché de **paths exactos** que el backend realmente devolvió con
ENOENT: hasta 256 entradas, names <256 bytes, máximo de strings 65.536 bytes.
No memoriza EIO/EACCES/OOM, no oculta coincidencias de hash, no cambia case,
separadores ni paths; capacidad/OOM caen al open original. No carga bytes de
assets ni interfiere con archivos de save. Se vacía al iniciar/configurar el
proceso. Assets se consideran inmutables mientras el juego corre, como el
índice/LRU anteriores: si se cambia el paquete, hay que reiniciar.
`asset_negative_cache 0/1` permite A/B; default 1. PERF informa avoided_opens,
entries y bytes, además de los índices existentes.

La regresión demuestra que 10.000 consultas posteriores del mismo ENOENT
evitan el open sin llamadas de directorio, que errores inciertos no se cachean,
y que otras rutas siguen abiertas. **Esto verifica ahorro de operaciones, no
segundos de carga o FPS medidos en la Vita nueva.** Medir cold-start A/B antes
de afirmar una cifra. No se precargó todo ni se aumentó el LRU por defecto;
se entrega el preset del usuario con 16 MiB.

## Referencias externas

Clones de investigación exclusivamente en `/tmp/vita-reference-ports/`:

| Repositorio / revisión | Coincidencia relevante | Diferencia y decisión |
|---|---|---|
| [elliencode/FalsoNDK](https://github.com/elliencode/FalsoNDK), `6bf476b7d6f03abcbb8122f0b9646c48b426cb18` | NativeActivity, AInput, OpenSL, axes/KeyEvents; Apache-2.0 | Se comparó emulación; los contratos SIGE se derivan del APK/SO exactos |
| [v-atamanenko/FalsoNDK](https://github.com/v-atamanenko/FalsoNDK) | Diseño original NDK/controls; Apache-2.0 | API/layout upstream antiguo, no prueba semántica de acciones de este motor |
| [v-atamanenko/mc3-vita](https://github.com/v-atamanenko/mc3-vita), `6fd1808dbaaa82f97fc653d1616efdcb7b856ea5` | JNI, loader ARM, VitaGL, bridge de archivos; MIT | Lifecycle Java Gameloft diferente; putString no es un backend persistente transferible |
| [MetalSyntax/Zenonia4-psvita-port](https://github.com/MetalSyntax/Zenonia4-psvita-port), `5fa5e0d126140e3d2bb75170b1d0ea89bae7d6a5` | JNI/loader/software buffers, saves con byte arrays; MIT | NexusClet/saveFile, no RegistryEnumerator SIGE; no copiar offsets ni layout Dalvik supuesto |

No se copió implementación de save/input ni offsets de esos juegos. El nuevo
código es propio del port; se preservan los notices de las dependencias.

## Validación y builds

Los runners de `tests/run_*` incluyen los existentes más nuevos tests:

- Storage write/close/reopen entre procesos independientes O0/O3; pérdida de
  tmp, corrupción de main, fallo de rename y fallo de restore; ciphertext fixture
  opaco, nunca una partida real ni un archivo distribuido para aparentar save.
- JNI real: descriptors, refs, editor, enumeration; eventos pad/sticks/HAT,
  external controller, remapping y cola en O0/O3.
- Unicorn del filtro digital canónico y prologues de cinco hooks Debug.
- Gate de upload: IDs, productor, dimensiones, invalidación/recycled IDs y nuevo
  productor tras invalidación, O0/O3.
- Replay de parches FalsoJNI/FalsoNDK/VitaGL desde revisiones fijadas, fuera del
  checkout, con comparación byte-exacta de resultados normalizando CRLF.
- Existing alpha/palette/light/division/gamma/ABI ARM+NEON, raster scale, buffers,
  texture preservation, shader cache, settings, logger, audio queue/mixer gate,
  LRU concurrente, decoder/worker y PCM byte-exacto frente a FFmpeg para M4A.

`scripts/run_local_regressions.py` conserva los CRLF preexistentes de runners,
ejecutándolos mediante temporales eliminados al finalizar. Resultados/logs en
`/tmp/zombie-engineering-20260928/test-results/`, copiados a la entrega.
La verificación final y hashes exactos se adjuntan en `VALIDACION.json`.

Sólo `/usr/local/vitasdk`, default `-mfloat-abi=softfp`. HardFP no se tocó.
Builds Debug/Release secuenciales y limpias en directorios nuevos
`build-engineering-delivery-debug` y `build-engineering-delivery-release`;
VitaGL se recompila desde objetos limpios por variante. Debug conserva O1/g3
del proyecto; Release O3/g1. Los tests nuevos se ejecutan O0/O3. Se verifican
VPK/eboot no vacíos, ZIP CRC, eboot idéntico al del directorio y ELF matching.

Estado local: **STATICALLY VERIFIED**, **BUILD VERIFIED**, **VPK VERIFIED**
cuando corresponda a contratos, compilación y paquete comprobados.
Estado de esta nueva build: **REAL VITA VERIFIED** y **GAMEPLAY VERIFIED**
pendientes. El protocolo está en `ENGINEERING_VITA_TEST_2026-09-28.md`.


## Artefactos finales comprobados

Build ID de ambas variantes: `local-5f67eec-5e3402a4c9`.
Title ID verificado en el SFO de cada VPK: `ZOMB00001`.
Directorio de entrega Windows:
`C:\Users\Mortar\Downloads\Zombie-Shooter-Engineering-2026-09-28`.
Ruta equivalente WSL:
`/mnt/c/Users/Mortar/Downloads/Zombie-Shooter-Engineering-2026-09-28`.

| Variante | Archivo en la entrega | Bytes | SHA-256 |
|---|---|---:|---|
| Debug | Zombie-Shooter-Engineering-Debug.vpk | 2610933 | 76cf95cf7da029428216e1c110b06ecdb61564e3610bca6a1b8388dbc2f8c093 |
| Release | Zombie-Shooter-Engineering-Release.vpk | 2533656 | 43d191f4df1fce883966da9cb9f17ce6edee4ddf4ca51e96f8f7f484b72b7527 |

**BUILD VERIFIED**: ambos builds limpios terminaron con éxito; compiler default
SoftFP, ELF ARMv7 sin VFP-register argument ABI. Las cuatro advertencias por
variante son flags de C enviados a C++ y jobserver del submake; los logs completos
se adjuntan. Los ELF contienen el mismo build ID que el código final; los hooks
nativos de input/save están presentes únicamente en Debug.

**VPK VERIFIED**: ambos ZIP pasan CRC, sus eboot coinciden byte por byte con el
build y cada ELF/velf/eboot copiado conserva su hash. Los ELF sin strip están en
`Debug/so_loader.elf` y `Release/so_loader.elf`, junto al velf y eboot respectivos.
La SO canónica coincide también con la entrada ARM del split APK original.

Las **21 suites pasan**, incluidos los tests existentes de audio/renderer/JNI y
los nuevos de restart, ARM input, patches y guards. Los contratos reconstruidos
del APK/SO y las condiciones estáticas indicadas están **STATICALLY VERIFIED**.
Los tests no prueban progreso de campaña ni acciones gameplay.
**REAL VITA VERIFIED** y **GAMEPLAY VERIFIED** siguen pendientes para esta build.

`ARCHIVOS_MODIFICADOS.md` enumera los 53 archivos de código/parches/docs/evidencia
tocados o creados en este pase; omite diferencias previas de CRLF. La entrega
incluye `Cambios-locales.patch` con los archivos nuevos, snapshot completo de
fuentes modificadas, evidencia original dirigida, logs y `VALIDACION.json`.
Los presets completos conservan el baseline de usuario; no se entrega ningún
save artificial ni datos del juego alterados.
