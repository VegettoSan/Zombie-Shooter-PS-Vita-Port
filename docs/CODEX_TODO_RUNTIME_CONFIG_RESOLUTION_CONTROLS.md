# Codex TODO — configuración runtime de resolución y controles Vita

Fecha: 2026-09-26

Este documento contiene **instrucciones solamente**. No implementar automáticamente al leerlo y no cambiar comportamiento de `master` hasta que los resultados de hardware indiquen que corresponde integrar cada parte.

## Reglas generales

- PS Vita real es la autoridad de runtime. No declarar una integración como verificada sólo por host tests, compilación o Vita3K.
- Mantener exactamente dos VPKs: Debug y Release.
- No modificar ni reemplazar `demo/libzombie_shooter.so`; es la referencia canónica e inmutable.
- No mezclar estos cambios con optimizaciones nuevas de renderer/audio/input. Integrarlos de forma aislada y comprobable.
- No sobrescribir archivos de configuración existentes del usuario.
- Toda opción runtime nueva debe tener logging claro en Release para poder identificar qué configuración se usó en cada log.

---

# 1. Integrar configuración runtime de resolución

## Contexto

`master` actualmente contiene `software_width` en `source/utils/settings.c`, con valores aceptados 0/864/960, pero el default actual es 864 y el hook de `render_scale` se instala desde `so_patch()`.

El run físico que usó el límite 864 produjo un crash inmediatamente después de crear/subir la primera superficie de software. Para aislar la causa se creó la rama de prueba:

`fix/run16-disable-render-scale`

En esa rama se preparó una variante A/B/C con un único VPK y un único archivo:

`ux0:data/zombieshooter/config.txt`

Codex debe **revisar y portar la lógica**, no hacer cherry-pick ciego ni borrar otros cambios que `master` pueda haber recibido después.

## Semántica requerida

Usar:

```text
software_width 0
```

Valores soportados:

- `0` = política original Android/juego. El hook de `scale_config::calculateFactor()` NO debe instalarse. Este es el baseline seguro y debe ser el valor por defecto mientras se investiga el crash.
- `960` = instalar el hook verificado y limitar el ancho de trabajo a 960. En Vita 960x544 se espera aproximadamente 960x544 si el factor original es superior al límite.
- `864` = instalar el hook verificado y limitar el ancho de trabajo a 864. En Vita 960x544 se espera aproximadamente 864x489.

No aceptar valores arbitrarios todavía. Si el archivo contiene otro entero, hacer fallback a `0` y registrar una advertencia.

## Creación del config

Si `ux0:data/zombieshooter/config.txt` no existe:

1. Crear el archivo una sola vez.
2. Usar `software_width 0` como default.
3. Conservar también las demás opciones ya soportadas por el port.
4. Incluir comentarios cortos que expliquen 0/960/864 si el parser los ignora correctamente.
5. No volver a crear ni sobrescribir el archivo en siguientes arranques.

Si el archivo ya existe pero no contiene `software_width`, usar 0 en memoria sin borrar ni reescribir las demás opciones del usuario.

## Orden de inicialización

`settings_load()` debe ejecutarse antes de `so_patch()`. La decisión de instalar el hook debe tomarse después de haber leído `software_width`.

Para `software_width 0`:

- no reservar trampoline para este hook;
- no llamar `hook_addr()` para `scale_config::calculateFactor()`;
- no modificar factor;
- dejar toda la política original del motor intacta.

Para 960/864:

- conservar validación de símbolo;
- validar Thumb;
- validar offsets de función/getter/setter contra el SO canónico;
- validar prólogo;
- validar patch arena;
- instalar sólo si todo coincide;
- si falla cualquier comprobación, dejar la función original y loguear `installed=0`.

## Logging requerido

Modo 0:

```text
[PERF] work_resolution installed=0 requested_width=0 mode=original hook_skipped=1
```

Modo 960 o 864, tras instalar:

```text
[PERF] work_resolution installed=1 requested_width=<valor>
```

Durante las primeras llamadas, registrar:

```text
[PERF] work_resolution physical=960x544 factor_before=<...> factor_after=<...> predicted=<WxH>
```

El logging debe permitir identificar inequívocamente la configuración usada en un log físico.

## Matriz de prueba física

Usar el mismo Release VPK y reiniciar completamente la app después de editar el archivo.

1. `software_width 0`
   - debe omitir el hook;
   - se espera la política original, observada previamente alrededor de 1024x580;
   - si crashea aquí, el render-scale no es la causa y no deben probarse 960/864 hasta analizar el nuevo dump.

2. `software_width 960`
   - sólo probar si 0 es estable;
   - si 0 funciona y 960 falla, sospechar del hook/trampoline o de modificar `scale_config`, no específicamente del factor 0.9.

3. `software_width 864`
   - sólo probar si 960 es estable;
   - si 0 y 960 funcionan pero 864 falla, aislar el problema en el factor/superficie reducida o supuestos internos asociados a ese tamaño.

No convertir 864 en default hasta completar esta prueba física.

---

# 2. Crear configuración separada para remapear botones de PS Vita

## Problema actual

El mapping físico de Vita está parcialmente hardcodeado en `source/utils/gamepad.c`.

Actualmente:

- perfil estándar (`vita_shooter 0`): L->LB, R->RB, rear-left->LT, rear-right->RT;
- perfil Shooter (`vita_shooter 1`): L->LT, R->RT, rear-left->LB, rear-right->RB;
- Cross=A, Circle=B, Square=X, Triangle=Y permanecen fijos;
- DS3/DS4 deben conservar su perfil estándar independiente.

La prueba física mostró que todavía hay botones/actions mal acomodados. No seguir agregando perfiles hardcodeados. Hacer el mapping de la Vita portátil configurable por archivo.

## Archivo nuevo

Crear, separado de `config.txt`:

`ux0:data/zombieshooter/controls.txt`

Razón: resolución/opciones generales y controles no deben compartir un parser cada vez más complejo, y el usuario debe poder modificar controles sin tocar otros ajustes.

Si no existe, crear una plantilla una sola vez. No sobrescribirla posteriormente.

## Formato propuesto

Formato simple `boton_fisico accion_logica`, una asignación por línea.

Plantilla inicial:

```text
# Zombie Shooter Vita control mapping
# Physical Vita button -> Android/Xbox logical action

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
```

El archivo debe ser case-insensitive para nombres conocidos o normalizarlos internamente.

## Nombres físicos mínimos soportados

- `cross`
- `circle`
- `square`
- `triangle`
- `l`
- `r`
- `rear_left`
- `rear_right`
- `start`
- `select`
- `dpad_up`
- `dpad_down`
- `dpad_left`
- `dpad_right`

No mapear sticks analógicos mediante este archivo en la primera implementación. Mantener ejes/deadzone como están para no mezclar dos sistemas distintos.

## Acciones lógicas mínimas soportadas

- `A`
- `B`
- `X`
- `Y`
- `LB`
- `RB`
- `LT`
- `RT`
- `START`
- `BACK`
- `DPAD_UP`
- `DPAD_DOWN`
- `DPAD_LEFT`
- `DPAD_RIGHT`
- `NONE`

`NONE` permite deshabilitar un botón físico para pruebas.

Estas acciones deben terminar produciendo exactamente los keycodes/ejes Android que ya usa FalsoNDK; el archivo sólo debe modificar la traducción física Vita -> control lógico. No duplicar la capa Android ni reescribir el sistema de InputDevice.

## Ejemplo de perfil Shooter manual

El usuario debe poder conseguir el perfil Shooter sin recompilar escribiendo:

```text
l LT
r RT
rear_left LB
rear_right RB
```

Por lo tanto, cuando `controls.txt` exista y sea válido, debe tener prioridad sobre `vita_shooter`.

Compatibilidad:

- si no existe `controls.txt`, conservar temporalmente el comportamiento actual de `vita_shooter 0/1` para no romper configs viejos;
- cuando exista `controls.txt`, ignorar `vita_shooter` para la Vita portátil y registrar que se está usando mapping explícito;
- no aplicar este remapeo a DS3/DS4 u otros controladores externos salvo que en el futuro se diseñe un archivo/perfil separado para ellos.

## Parsing y seguridad

- Ignorar líneas vacías y comentarios `#`.
- Ignorar claves físicas desconocidas con warning, sin abortar.
- Si una acción lógica es desconocida, conservar el mapping default de ese botón y loguear warning.
- No hacer crash por archivo incompleto.
- Un archivo parcial debe sobrescribir sólo las entradas presentes; el resto conserva defaults.
- Permitir duplicar una acción lógica en varios botones físicos para facilitar pruebas, pero registrar el mapping final completo al inicio.
- No guardar/reordenar automáticamente el archivo del usuario durante el arranque.

## Logging requerido

Al cargar:

```text
[INPUT] vita_mapping source=defaults
```

o:

```text
[INPUT] vita_mapping source=controls.txt
```

Luego registrar una línea compacta con el mapping efectivo, por ejemplo:

```text
[INPUT] map cross=A circle=B square=X triangle=Y l=LT r=RT rear_left=LB rear_right=RB start=START select=BACK
```

En Debug se pueden mantener trazas de transición. En Release evitar spam por frame; el mapping efectivo sólo necesita imprimirse al inicio.

## Tests requeridos

Extender `tests/run_gamepad_regression.sh`/tests relacionados para cubrir:

1. archivo inexistente -> defaults actuales;
2. archivo parcial -> sólo cambia lo especificado;
3. perfil estándar explícito;
4. perfil Shooter explícito;
5. face buttons remapeados;
6. `NONE`;
7. nombre físico inválido;
8. acción lógica inválida;
9. duplicados lógicos permitidos;
10. DS3/DS4 no afectados;
11. release sin trazas por frame;
12. reinicio de la app aplica cambios de archivo sin recompilar.

No declarar el mapping final correcto hasta recorrer el tutorial en Vita real y anotar qué acción realiza realmente cada botón.

---

# 3. Orden recomendado para Codex cuando se retome

1. No tocar `master` runtime hasta tener el resultado del VPK de `fix/run16-disable-render-scale` con 0/960/864.
2. Analizar los logs físicos y decidir qué parte de `render_scale` es segura para integrar.
3. Integrar primero el sistema de configuración de resolución de forma aislada.
4. Build + host tests + Vita real.
5. Después implementar `controls.txt` sin cambiar renderer/audio.
6. Hacer tests host de input.
7. Probar en Vita real tutorial/menús/gameplay y corregir únicamente el mapping del archivo/defaults.
8. Sólo cuando ambas funciones estén verificadas, actualizar documentación de estado y considerar limpiar/deprecar `vita_shooter`.

## Criterio de éxito

La meta es que **un mismo Release VPK** pueda usarse para experimentar sin recompilar:

- cambiar resolución editando `config.txt` y reiniciando;
- cambiar botones Vita editando `controls.txt` y reiniciando;
- preservar Android InputDevice/FalsoNDK, mandos externos, audio, renderer y SO canónico;
- producir logs que indiquen exactamente qué configuración estuvo activa durante cada prueba física.
