# Android Fidelity, audio y rendimiento — 2026-09-26

Build local-e149e0e-f32ca72dbe. Análisis estático, tests y builds verificados; nueva Vita real pendiente.
Informe de los 33 puntos del documento del usuario. Compilación no demuestra FPS,
cámara, sonido audible ni estabilidad física. No se ejecutó el juego en un emulador.

## 1. Commit y estado
Repositorio autorizado /home/vegettosandev/Zombie-Shooter-PS-Vita-Port, master,
HEAD e149e0ea477377022d7581d81574dad618012332. Estado local previo con cambios de
controles Xbox preservado. G:/Games/Zombie Shooter era el juego PC, no este repo.
No hubo commit/push/fetch/pull, Actions, publicación ni actualización de revisiones.
SDK /usr/local/vitasdk, GCC10.3.0 SoftFP; HardFP intacto. vitaGL pin:
9c23758ff17893db63887f95e9a4d9350c986d88.
SO original SHA256:
5e5e2e1bbe86e126c3e2dce5ad97c2e9dc6282305ea7d3e24b49ee4769c5b5b7.

## 2. Archivos modificados
Este pase: source/java.c, source/patch.c, source/utils/glutil.c,
source/utils/raster_palette.c, CMakeLists.txt, .gitignore; nuevos
source/utils/audio_stream.c, scripts/prepare_music.py, tests/shader_cache_regression.c,
tests/run_shader_cache_regression.sh; ampliación tests/input_device_regression.c.
FalsoJNI.c/.h; vitaGL custom_shaders.c/shared.h y zombie_shader_cache.h/.inc.
Parches falso_jni.patch/vitagl.patch y submodules.lock.json. El parche vitaGL
conserva zombie_texture_update.h preexistente. Documentación: este informe,
PORT_STATUS.md, port_progress.md, HARDWARE_TEST_LOG.md.
Otros cambios del estado final (mapping, FalsoNDK, workflow, ajustes, tests)
ya estaban presentes: no atribuir todo el diff a este pase.

## 3. Fuente exacta de 1480×838
Cadena original SO: screenSizeInInches0x3ca1e4 → createGraphicContext0x3ca4d0 →
scale_config::calculateFactor0x481db0 → singleton ESContext →
OpenGLES::init0x4677e8 → OpenGLES::UnLock0x46bec4.
Offsets relativos a load base canónica. createGraphicContext llama setScreenConfig
en0x3ca9b4 y calculateFactor en0x3ca9ba. factor() produce dimensiones de trabajo
ESContext+8/+12. Sus campos físicos+0/+4 vienen de EGL960×544.
OpenGLES::init copia trabajo a this+536/+540 (0x218/0x21c);
reserva width×height×4 en0x467bfc..0x467c36; buffer this+528 (0x210).
UnLock sube GL_RGBA/GL_UNSIGNED_BYTE desde ese buffer con esas dimensiones
en0x46bf54: retorno Thumb load_base+0x46bf59, históricamente0x9846bf59.

data/assets/game.cfg tiene MaxWidth1480, MinWidth720. Selección por diagonal
habilitada por defecto. Diagonal>=8 pulgadas elige MaxWidth:
factor1480/960, trunc(544×factor)=838. Es superficie software RGBA, no pantalla
física ni evidencia de FBO1480×838. Layout continuo: stride5920 y4,960,960bytes.

## 4. ¿Resolución correcta o bug?
Es una salida válida de la política del engine para una pantalla grande.
La entrada del port era incorrecta: xdpi/ydpi ausentes → FalsoJNI float default1.
sqrt(960²+544²)/1≈1103 pulgadas elige la rama>=8 y explica1480×838.
Se corrige JNI; no game.cfg, resolución forzada ni parche manual de zoom.

## 5. Resolución después
Con220DPI, diagonal≈5.016 pulgadas. Tabla original4.8..5.8 elige1024;
factor1024/960 y altura truncada580. Predicción estática1024×580,
stride4096,593,920píxeles,2,375,680bytes,52.11% menos trabajo en píxeles/bytes.
No es una medición física ni garantiza un ahorro proporcional de tiempo.
Nuevo PERF software_surface informa width/height/stride_bytes/bytes/caller
cuando cambia una subida RGBA grande: confirmar ahí el tamaño real.
La superficie física sigue960×544/MSAA none.

## 6. Campos JNI
Se implementan los campos leídos por la función nativa:
android/util/DisplayMetrics.xdpi:F y ydpi:F, IDs1100/1101.
Constructor <init>()V; cadena heredada Activity/NativeActivity.getWindowManager()
Landroid/view/WindowManager;, WindowManager.getDefaultDisplay()Landroid/view/Display;,
Display.getMetrics(Landroid/util/DisplayMetrics;)V. Lookup por clase/nombre/firma,
rechazo de firmas incorrectas. WindowManager/Display prestados estables; metrics
con vida propia. Valores físicos constantes a través de FieldsFloat.
No es una VM Android completa ni un DisplayMetrics mutable general.

widthPixels/heightPixels/density/densityDpi/scaledDensity no aparecen como accesos
JNI en esa ruta; no se inventan. Las dimensiones físicas ya las entrega EGL.

## 7. Valores y razón
xdpi=ydpi=220.0f, coherente con EGL del port y diagonal~5 pulgadas.
No se copió el200 de Fahrenheit ni se eligió DPI para forzar una resolución.
PERF display_metrics hace visibles DPI/superficie física también en Release.

## 8. Cámara antes/después
Antes se seleccionaba pantalla>=8 pulgadas; ahora la entrada física permite la
política original de escalas. Corrige la causa estática de superficie excesiva.
Encuadre, tamaño de personaje, HUD y zoom requieren comparar Android/Vita en
misma zona y orientación. Sin nueva ejecución/capturas no declarar cámara restaurada.

## 9. Música presente
Cinco originales reales M4A/AAC: menu_mus01, mus01, mus02, amb01 y rain,
en data/assets/music. Se añaden WAV float32, frecuencia/canales originales;
no reemplazar ni renombrar M4A. ZIP sólo cinco WAV y manifiesto.

| Pista | Hz | Canales |
|---|---:|---:|
| menu_mus01 | 44100 | 2 |
| mus01 | 44100 | 2 |
| mus02 | 44100 | 2 |
| amb01 | 44100 | 2 |
| rain | 44100 | 2 |

## 10. Causa de audio
MusicPlayer::onOpen0x4d4174 ya aplica STRING::Replace(".ogg",".m4a") en0x4d419a:
aliasar extensiones no aporta decoder. Engine::createMusicPlayer0x4d20f8 intenta
AAsset/ANDROIDFD locator0x800007bc o URI. El backend local realiza URI mediante
libsndfile, no el decoder M4A/AAC Android ni su locator ANDROIDFD.
FalsoNDK rechaza FD.m4a para evitar fugas: protección conservada.
La incompatibilidad de formato/ruta está demostrada estáticamente; no excluye
otro problema de mixer/lifecycle que sólo se pueda ver en consola.

## 11. Fix de audio
Hook verificado createMusicPlayer: shared_ptr result oculto r0, Engine*this r1,
STRING const& r2. Verificación de símbolo/offset/Thumb/prólogo
af03b5f0 0700e92d y arena; trampolín8bytes originales + salto de retorno.
STRING original12bytes usa constructor/c_str/destructor originales.

Whitelist de cinco music/<nombre>.m4a, con prefijo de resourcePath admitido,
a ux0:data/zombieshooter/assets/music/<nombre>.wav. Se valida WAV legible,
frames>0 y1..2canales. Ruta absoluta lleva al fallback URI original, usando
el mismo player y manejo de volumen/pausa/loop/destrucción/mixer.
Sin PCM válido o hook exacto, llamada original sin desvío.
PERF audio_stream: installed, requested/path/rate/channels/frames o missing_pcm.

prepare_music.py usa PCM float32 sin resampling. Re-decodifica original y WAV:
cada muestra debe ser idéntica. Cinco pistas PASS, hashes originales intactos.
No afirmar audio audible/loops/mezcla restaurados antes de escucharlos en Vita.
WAV~154MiB instalados; ZIP148,889,624bytes. Guardar como assets locales del juego.

## 12. Causa de la mira
vid/touch_aim.png existe. drawTouchAim0x465254 requiere Map, INPUT_Control
y ControlStates: retorna si no hay estados distintos de4 ni Type4 activo.
Usa screenMouse y transformación de mapa, AutoAim/manual y aimPosition;
y<0 causa retorno. Luego TextureDescription::checkAndLoad, scale_config::factor,
escala de menú/getTouchAimYShift y drawTextureCentered.
Estado/gate/coordenadas/carga/dibujo siguen causas runtime posibles.
No hay causa runtime concluyente ni asset faltante demostrado.

## 13. Fix de la mira
No se fuerza gate ni se crea una mira falsa. DPI afecta política de escala,
pero no demuestra restauración. Comprobar mira durante apuntado manual/disparo,
AutoAim y Android con gamepad. Si persiste, instrumentar gates originales con
la nueva evidencia. Objetivo pendiente, sin declarar restauración ficticia.

## 14. UI táctil y controller
ControlsPainter constructor0x464f44 usa assets originales. draw0x4654c0 busca
JoystickControl/TouchControl typeIDs699ba90f/29aa38bb, enabled+12.
0x4655b2 invierte Touch.enabled;0x4655be lee Joystick.enabled.
Si !Touch.enabled || Joystick.enabled salta sticks/botón táctil a0x465608.
drawTouchAim se llama después de ese gate.
Ocultar controles táctiles con gamepad habilitado es comportamiento original,
también Android con esta SO; no prueba que la mira deba ocultarse.
Mapping Vita→Xbox intacto.

## 15. Assets faltantes confirmados
Ninguno de los ocho del painter: attack_frame.png, touch_aim.png, move_joy.png,
move_back.png, shoot_joy.png, shoot_back.png, shoot_but.png, hover_cursor.png.
Todos existen en data/assets/vid, y los cinco M4A en music.
Presencia local no demuestra copia completa en tarjeta ni apertura runtime.
Sin reemplazos artificiales.

## 16. Vertex shaders únicos
Pendiente log de Vita; no inventar conteo.
## 17. Fragment shaders únicos
Pendiente log de Vita; no inventar conteo.
## 18. Combinaciones de programa
Pendiente log de Vita. Instrumentación en vitaGL observa XXH3 fuente/tipo/longitud
al ShaderSource y parejas al LinkProgram. Hasta256entradas por inventario,
usos y overflow; reporta cada~5s y una línea shader_unique por fuente/tipo.
Los usos de programa son enlaces, no frecuencia de draw ni uso visual.
No se vuelcan fuentes propietarias.

## 19. Arquitectura de caché
Propietario único vitaGL, HAVE_SHADER_CACHE=1 Debug/Release; GLSL/POSTPONED
preservados. Eliminado wrapper paralelo inactivo: skip_next_compile y nombre
global, buffer32KiB, guardado antes de compilación real y dummy fallbacks.
Native ShaderSource conserva concatenación y longitudes negativas.
Hash antes de traducción; clave fuente/pareja/tipo/bindings/esquema,
nombre v2-9c23758-GXP-...gxp. Sólo guardar programas compilados y semánticas
de pareja/matrices. Estado por shader, sin omitir compilación mediante flagglobal.

Ruta ux0:data/zombieshooter/cache/shaders. Envelope32bytes: magic/version2/
type/bindings/length/checksumFNV1a32/key64. Payload<=1MiB, lectura exacta.
Valida matrices/bindings/índices/nombres terminados/bools y GXP header mínimo160,
tabla de parámetros/count/records dentro del tamaño declarado y padding<=15,
magic/tamaño/
sceGxmProgramCheck. Corrupción → elimina archivo → compila GLSL retenido.
Allocaciones y registro comprobados antes de modificar shader; fallo de recursos
conserva archivo válido y usa compilación normal.
Temporal por hilo+rename; errores de IO y programa vacío no crean entradas.
Sin directorio se omite cache IO, no intenta raíz. Sin GXP/fuentes en repo/VPK.

## 20. Hit/miss/fallback
Host O0/O3 acepta envelope válido y rechaza wrong type/bindings/key/version/magic,
truncación y flip de cada byte del payload. También el GXP blit original de vitaGL:
declared282/stored284 aceptado, truncación/padding excesivo/tabla inválida rechazados.
Prueba validador compartido,
no SceGxm ni cache en disco Vita. Primer arranque, segundo hit y archivo corrupto
con recompilación/imagen idéntica siguen pendientes físicos.

## 21. Compile/link
POSTPONED retorna glCompileShader sin compilar; pareja se traduce/compila al link.
Guardado allí, nunca antes. Requests y actual_compile_calls separados.
Contadores de vida: hits/misses/invalid/bytes_loaded/compile_us/load_us/
writes_failed/actual_compile_calls/writes. Link timing conservado.
Cache caliente aún registra/enlaza; no prometer link_us cero ni más FPS estables.

## 22–23. Flags y resultados A/B
| Variante | Referencia/cambio | Comprobación local | Vita | Final |
|---|---|---|---|---|
| baseline | SoftFP+cache, checks conservados | Debug/Release PASS | Pendiente | Entregada |
| nodebug | baseline + NO_DEBUG=1 | vitaGL PASS | Pendiente | Desactivada |
| textures | nodebug + TEXTURES_SPEEDHACK=1 | vitaGL PASS | Pendiente | Desactivada |
| gc | baseline + GC127/0x20000 | ELF PASS | Pendiente | Desactivada |
| draw2 | baseline + DRAW_SPEEDHACK=2 | vitaGL PASS | Pendiente | Desactivada |
| draw1 | DRAW_SPEEDHACK=1 | No ejecutada | Pendiente evidencia draw2 | Desactivada |
| samplers | SAMPLERS_SPEEDHACK=1 | No ejecutada | Pendiente perfil | Desactivada |
| nodmac | NO_DMAC=1 | No ejecutada | Pendiente perfil copia | Desactivada |

Esto es build, NO A/B medido. Selector ZOMBIE_VITAGL_EXPERIMENT sólo Release,
defaultbaseline. Targets vitaGL_build/so_loader para experimentos, sin tercer VPK.
Esos compile checks preceden el ajuste final portable de bounds/padding GXP;
ambas builds finales se recompilaron con ese ajuste y su regresión nueva.
Final fresh vuelve a baseline y limpia objetos vitaGL entre variantes.
NO_DEBUG quita error handling; DRAW2=SAFER_DRAW_SPEEDHACK en el pin.
TEXTURES altera preservación/update: validar transparencias/regiones.
GC: API(priority,affinity), segundo argumento NO es tamaño del stack. Default
0x10000100/0; experimento127/0x20000 antes de Init. Cambia dos atributos del GC:
separarlos si hay diferencia para atribuir causa.
Sin HAVE_TEXTURE_CACHE, SHARED_RENDERTARGETS, readbacks/buffers/index/circular
hacks nuevos ni migración PVR. FBO count runtime no medido, no justifica shared RT.

## 24. Cobertura palette
PERF palette_install installed_mask/rejected_mask ahora visible siempre.
Esperado0xFF/0x00 si ocho hooks coinciden; no observado aún en nueva Vita.
Offsets/kernels/umbral32 conservados. MáscaraFF con calls0 sólo significa no
filas largas instrumentadas; no excluye cortas ni confirma toda cobertura.
El log previo sin líneas palette no atribuía costo a esa familia.
Contadores mode0..7/fast/vector/skipped, alpha/collector y sondas existentes intactos.

## 25. softwareTact
Documento del usuario reporta baseline~50–63ms/frame, MAP~90+ms/frame y8–11FPS.
Sin medición nueva. Sondas collector/alpha/palette conservadas; evaluar cobertura
antes de reimplementar otras rutinas por nombre. Map/software/graph inclusivos.

## 26. TexSub
Baseline del documento~17–18ms/frame y4,960,960bytes de superficie.
Nueva dimensión/tiempo requieren log. Menos bytes no garantizan ahorro lineal.

## 27. Bulk fill
Baseline~7–8ms/frame,4,960,960bytes/frame; swap~0.2ms.
Predicción2,375,680bytes con1024×580, no tiempo medido.
No sumar fases inclusivas. Como archivo histórico identificable, informe Pass7
describe log_0010:10.536FPS/software47.736/TexSub18.051ms en ventanas19–48,
8.467FPS/software59.919/TexSub17.658ms en79–96. No identificarlo como el último
log del usuario ni extrapolar30FPS. Reperfilar misma zona tras corregir metrics.

## 28. Debug
BUILD VERIFIED, fresh CMake, GCC10.3.0 SoftFP. ELF sin strip.
## 29. Release
BUILD VERIFIED, fresh baseline. ZIP/eboot/param.sfo íntegros.
Warnings preexistentes Cflags en C++/jobserver; sin error final.
Ni SO/assets/GXP empaquetados ni actualización SDK. Ambos tienen misma identidad.

## 30. Tests
PASS O0/O3: métricas JNI/cadena exacta/firmas incorrectas, InputDevice/gamepad,
campos JNI históricos, GL buffers/IDs, logger, texturas RGBA/RGB565+regiones/guards,
asset index fallos/OOM/concurrencia, engine probe emitter, cache envelope.
ARM original vs kernels palette792+72dispatcher por optimización;
alpha432+54entradas parcheadas por optimización: bytes/registros idénticos.
Unicorn ejecuta funciones ARM aisladas, no gameplay Vita.
Cinco conversiones musicales: muestras idénticas, hashes originales intactos.
Parches aplicados desde snapshots HEAD limpios FalsoJNI/FalsoNDK/vitaGL;
TODOS los hashes locked coinciden, incluido helper de texturas preexistente.
git diff --check código/docs excluye datos patches/*.patch con contexto/CRLF;
aplicación desde fuente limpia y hashes verifican los parches.

## 31. VPK y entrega
/home/vegettosandev/Zombie-Shooter-PS-Vita-Port/build-session-debug/zombie_shooter.vpk
/home/vegettosandev/Zombie-Shooter-PS-Vita-Port/build-session-release/zombie_shooter.vpk

Entrega C:/Users/Mortar/Downloads/Zombie-Shooter-Android-Fidelity-2026-09-26:
sólo dos VPK nuevos, ELF Debug/Release, music-pcm.zip, informe, logs y SHA256 manifest.
Extraer ZIP a ux0:data/ (lleva zombieshooter/assets/music/*.wav), mantener originales.
Usar Release para FPS; Debug para diagnóstico, no comparar sus FPS con Release.

## 32. git status --short
Captura completa al final; sin commit, cambios previos preservados.

## 33. Checklist exacto de Vita
- Startup: identidad de esta entrega, baseline,960×544/MSAA none,
  display_metrics220/220 y software_surface para tamaño interno REAL.
- Escuchar menú/niveles1 y2/ambient/lluvia, pausa/reanudar/reiniciar nivel,
  SFX simultáneos/loops/mezcla. Revisar audio_stream installed1 y rutas/rate/frames.
- Tutorial y nivel2: misma zona/orientación que Android; cámara/personaje/HUD/
  reticle/UI, movimiento/apuntado/disparo, AutoAim y controller/touch.
- Enemigos/explosiones/sangre/partículas/objetos superpuestos/transparencias;
  FPS habitual/mínimo, Loading, visual glitches, crash y stutter.
-60–90s PERF por zona: software/map/graph, TexSub, bulk bytes/us, memoria/audio.
- Caché vacía: miss→compile→write; segundo arranque misma zona: hits y menos
  actual_compile_calls, imagen idéntica. Con backup, truncar UN GXP del cache
  y verificar invalid→miss→recompilación sin crash ni cambio visual.
- Palette máscaraFF/rejected0 si coincide; calls mode0..7, alpha/collector;
  shader_inventory vertex/fragment/combinations/overflow incluso si0.
- A/B posterior: baseline/nodebug, nodebug/textures, baseline/gc,
  baseline/draw2. Misma zona/clocks/cache/60–90s; comparar tiempos y visuals.
  No conservar un flag porque sólo compila.

Enviar logs COMPLETOS de ambos arranques nuevos:
ux0:data/zombieshooter/logs/log_XXXX.log; no asumir número fijo ni sólo screenshots.
Indicar build/recorrido/audio/cámara/mira/FPS/Loading. Si logger falla, log_fallback.txt.
Si crash, .psp2dmp y ELF de la variante exacta de esta entrega (Debug/Release incluidos). Capturas Android/Vita equivalentes
son necesarias para concluir encuadre. No exigir pruebas largas sin leer estos logs.

## Investigación externa aplicada
Fuentes primarias revisadas2026-09-26. SHAs/snapshots externos en reference-review.txt.
El pin local no se actualizó. Un éxito en otro juego no prueba rendimiento aquí.

| Fuente | Hallazgo y decisión para Zombie |
|---|---|
| [GTA SA](https://github.com/TheOfficialFloW/gtasa_vita/blob/96941714673c56b689d51ce6f79df68bbd0bebd5/README.md) | Cache y varios flags en README; separar experimentos, no copiar lote |
| [BC2](https://github.com/TheOfficialFloW/bc2_vita/blob/8b2b3d7d9f0d9e76024017702760ea37da22880c/README.md) | DRAW1 documentado; investigar DRAW2 primero |
| [Bully](https://github.com/TheOfficialFloW/bully_vita/blob/f0b069f6552fc4974f56afe2d612a70fa9d54b08/loader/main.c) | Hash y GXP en ShaderSource; no asumir count1 ni cache antes de compilar |
| [Fahrenheit](https://github.com/Rinnegatamante/fahrenheit-vita/blob/6e6b3610269dcfa68407ce6d6961bbe4bedd66c2/loader/main.c) | DPI200 y adaptación shaders; usar220 y descriptors propios; README NO_DEBUG |
| [Anomaly2](https://github.com/Rinnegatamante/anomaly2_vita/blob/6882db50b35aa016fdde0f0d9132d2c17a630eef/loader/main.c) | GC127/0x20000 antes de Init; sólo experimento pendiente |
| [MC3](https://github.com/v-atamanenko/mc3-vita/blob/6fd1808dbaaa82f97fc653d1616efdcb7b856ea5/source/utils/glutil.c) | skip_next_compile/nombre global; eliminar patrón heredado |
| [Mass Effect](https://github.com/v-atamanenko/masseffect-vita/blob/fdd0641b568a12712e82df2102d8235b27a4cfb2/loader/utils/glutil.c) | Hash/GXP/fallback de otro shader específico; no reutilizar dummy |
| [Dead Space](https://github.com/v-atamanenko/deadspace-vita/blob/3eaa3c4b8a0201eec746f7676e0bddcffc5d5638/loader/utils/glutil.c) | Binarios específicos/PVR; preservar GLSL, PVR no elimina CPU raster |
| [Backstab](https://github.com/v-atamanenko/backstab-vita/blob/290130a8d6cb4b882ad445da4b4d9ac9f8377436/loader/utils/glutil.c) | Estado global de cache; preferir por shader |
| [GOF2](https://github.com/v-atamanenko/gof2-vita/blob/4d09f22c3930bd14c0d212b4f6e86c68e942698f/loader/utils/glutil.c) | Fallback shader específico; recompilar fuente original si falta/corrupto |
| [LSWTCS](https://github.com/gm666q/lswtcs-vita/blob/467f19055c9add1225e897d4d18d923ed7d15d09/source/utils/glutil.c) | Estado thread_local; evitar dependencia skip-next por hilo |
| [vitaGL pin](https://github.com/Rinnegatamante/vitaGL/tree/9c23758ff17893db63887f95e9a4d9350c986d88) | Paired GLSL/cache/Makefile exacto: dueño nativo único y validación |

Métricas de inventario, hit real, cámara, mira, audio audible, perf posterior y A/B
siguen PENDIENTES de Vita; no convertir predicciones o tests host en esos hechos.

### Captura final de estado

```text
 M .github/workflows/manual-prerelease.yml
 M .gitignore
 M CMakeLists.txt
 M PORT_STATUS.md
 M docs/HARDWARE_TEST_LOG.md
 m lib/falso_jni
 m lib/falso_ndk
 m lib/vitagl
 M patches/falso_jni.patch
 M patches/falso_ndk.patch
 M patches/submodules.lock.json
 M patches/vitagl.patch
 M port_progress.md
 M source/java.c
 M source/patch.c
 M source/utils/glutil.c
 M source/utils/raster_palette.c
 M source/utils/settings.c
 M source/utils/settings.h
 M tests/run_jni_field_regression.sh
?? docs/ANDROID_FIDELITY_PASS_2026-09-26.md
?? docs/CONTROLLER_IMPLEMENTATION_2026-09-26.md
?? scripts/prepare_music.py
?? source/utils/audio_stream.c
?? source/utils/gamepad.c
?? tests/gamepad_regression.cpp
?? tests/input_device_regression.c
?? tests/input_host/
?? tests/run_gamepad_regression.sh
?? tests/run_shader_cache_regression.sh
?? tests/shader_cache_regression.c
```
