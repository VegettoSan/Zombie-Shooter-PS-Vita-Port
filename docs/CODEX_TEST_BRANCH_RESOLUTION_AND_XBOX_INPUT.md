# Codex test branch — resolución + emulación de mando Xbox

Fecha: 2026-09-26
Rama de trabajo: `fix/run16-disable-render-scale`

Este documento contiene **instrucciones para Codex** para trabajar y probar ambas áreas en esta rama. No modificar `master` como parte de estas pruebas. PS Vita real es la autoridad de runtime; Vita3K no sustituye la validación física.

## Reglas de esta rama

- Mantener exactamente dos VPKs: Debug y Release.
- No modificar, reemplazar, parchear en disco ni regenerar `demo/libzombie_shooter.so`; es la referencia canónica e inmutable.
- No mezclar estas tareas con nuevas optimizaciones de renderer, audio o assets.
- Conservar los cambios y optimizaciones actuales de la rama salvo que una prueba A/B demuestre que deben retirarse.
- No tocar la ruta de sticks analógicos salvo que aparezca evidencia nueva: en Vita real los dos sticks funcionan correctamente actualmente.
- Los cambios de resolución y de input deben ser identificables por logs y reversibles mediante archivos de configuración, sin recompilar entre variantes cuando sea posible.
- No declarar un fix como verificado sólo por compilación, host tests o Vita3K.

---

# A. Resolución runtime: conservar el A/B/C actual

La rama ya contiene la prueba de resolución mediante:

`ux0:data/zombieshooter/config.txt`

Opción:

```text
software_width <valor>
```

Valores de prueba:

```text
software_width 0
software_width 960
software_width 864
```

Semántica obligatoria:

- `0`: política original del motor. **No instalar** el hook/trampoline de `scale_config::calculateFactor()`. Es el baseline seguro.
- `960`: instalar el hook verificado y limitar el ancho de trabajo a 960; se espera aproximadamente 960x544 cuando el factor original sea superior.
- `864`: instalar el hook verificado y limitar el ancho de trabajo a 864; se espera aproximadamente 864x489.
- Cualquier valor no reconocido debe caer a `0` de forma segura.

Si el config no existe, crear una plantilla una sola vez con `software_width 0`. No sobrescribir posteriormente el archivo del usuario.

Orden físico de prueba:

1. `software_width 0`
2. sólo si 0 inicia y funciona, `software_width 960`
3. sólo si 960 funciona, `software_width 864`

Interpretación:

- 0 falla: dejar de culpar al render-scale como causa principal y analizar el nuevo log/dump.
- 0 funciona y 960 falla: sospechar del hook/trampoline o de modificar `scale_config`.
- 0 y 960 funcionan pero 864 falla: sospechar de la superficie/factor 0.9 o de supuestos internos ligados a ese tamaño.
- los tres funcionan: comparar FPS, carga, estabilidad y logs antes de elegir default.

Logging mínimo esperado:

```text
[PERF] work_resolution installed=0 requested_width=0 mode=original hook_skipped=1
```

para 0, y para 960/864:

```text
[PERF] work_resolution installed=1 requested_width=<valor>
[PERF] work_resolution physical=960x544 factor_before=<...> factor_after=<...> predicted=<WxH>
```

No convertir 864 en default hasta validarlo en Vita real.

---

# B. Evidencia real actual de controles en Vita

Resultado físico confirmado por el usuario con el ajuste actual:

- ambos joysticks analógicos funcionan correctamente;
- el botón físico `L` enciende/apaga la linterna **y además lanza una granada al mismo tiempo**;
- los demás botones físicos probados no realizan sus acciones esperadas;
- en particular `Cross`, que debería representar Xbox `A`, no funciona como `A`;
- por tanto el estado actual **no es simplemente un layout mal acomodado**: la ruta de botones digitales del mando Xbox debe investigarse.

No inventar una tabla de acciones restante; sólo las observaciones anteriores están confirmadas físicamente.

Hipótesis de trabajo, no conclusión:

- sticks correctos sugieren que la ruta de ejes/motion events está viva;
- botones digitales ausentes/incorrectos apuntan a la ruta de botones/key events/aliases/metadatos del dispositivo lógico;
- `L` realizando dos acciones puede ser una duplicación de representaciones (por ejemplo shoulder + trigger/alias), un código/eje erróneo o un problema de cómo el juego interpreta esos eventos. Debe demostrarse con trazas.

**No intentar resolver esto sólo intercambiando botones en `gamepad.c`.**

---

# C. Objetivo de input: simular un mando Xbox

La PS Vita portátil debe presentarse al juego como **un único mando Xbox lógico coherente**.

Contrato conceptual:

```text
botón físico PS Vita
    -> exactamente un control Xbox lógico
    -> backend interno del port
    -> Zombie Shooter
```

La interfaz configurable debe hablar exclusivamente en términos Xbox:

- `A`, `B`, `X`, `Y`
- `LB`, `RB`
- `LT`, `RT`
- `LS`, `RS` (click de sticks)
- `START`, `BACK`
- `DPAD_UP`, `DPAD_DOWN`, `DPAD_LEFT`, `DPAD_RIGHT`
- `NONE`

No exponer `KEYCODE_*`, Android sources, IDs de ejes Android ni otros detalles de FalsoNDK en la configuración del usuario. Si internamente la implementación debe convertir esos controles Xbox a eventos Android para alimentar el `.so`, eso es únicamente backend.

## C1. Primero diagnosticar/reparar la ruta digital

Antes de considerar terminado `controls.txt`, Debug debe poder seguir una pulsación de extremo a extremo.

Para cada cambio de estado físico registrar de forma acotada:

- botón físico Vita detectado;
- máscara física antes y después de `fndk_translate_pad_buttons()` o la capa equivalente;
- control Xbox lógico pretendido;
- evento interno emitido;
- `deviceId`;
- `source`;
- DOWN/UP o valor de trigger;
- si se produjo KeyEvent, MotionEvent, trigger axis y/o alias de botón.

Ejemplo orientativo:

```text
[INPUT] physical=cross down=1 xbox=A
[INPUT] emit xbox=A key=<...> source=<...> device=<...> action=DOWN
[INPUT] physical=cross down=0 xbox=A
[INPUT] emit xbox=A key=<...> source=<...> device=<...> action=UP
```

Para `L`, la traza debe permitir demostrar si una sola pulsación genera dos controles/eventos distintos. No afirmar la causa hasta verlo.

Release no debe generar spam por frame; sólo mapping inicial y diagnósticos excepcionales.

## C2. Mapping temporal mínimo para validar la infraestructura

Mientras se depura, usar como intención lógica Xbox:

```text
cross -> A
circle -> B
square -> X
triangle -> Y
l -> LB
r -> RB
rear_left -> LT
rear_right -> RT
start -> START
select -> BACK
```

D-pad conserva D-pad Xbox. Sticks analógicos conservan la ruta actual.

Una pulsación física normal debe terminar en **un solo control Xbox lógico** salvo que la semántica Xbox requiera explícitamente más de una representación interna para compatibilidad; si se usan aliases internos, deben representar el mismo control lógico y no provocar dos acciones de gameplay.

`LT` y `RT` deben conservar semántica de trigger Xbox. En Vita, una fuente física digital puede representar 0.0/1.0, pero no debe producir accidentalmente otra acción de shoulder independiente.

DS3/DS4 y otros mandos externos no deben ser alterados por el mapping de Vita portátil.

---

# D. `controls.txt` para pruebas sin recompilar

Una vez estabilizada la ruta Xbox lógica, implementar en esta rama:

`ux0:data/zombieshooter/controls.txt`

Formato:

```text
# Physical Vita button -> Xbox logical control
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

Nombres físicos mínimos:

- `cross`, `circle`, `square`, `triangle`
- `l`, `r`
- `rear_left`, `rear_right`
- `start`, `select`
- `dpad_up`, `dpad_down`, `dpad_left`, `dpad_right`

Acciones Xbox soportadas:

- `A`, `B`, `X`, `Y`
- `LB`, `RB`, `LT`, `RT`
- `LS`, `RS`
- `START`, `BACK`
- `DPAD_UP`, `DPAD_DOWN`, `DPAD_LEFT`, `DPAD_RIGHT`
- `NONE`

Reglas del parser:

- ignorar comentarios `#` y líneas vacías;
- nombres case-insensitive o normalizados;
- archivo parcial: sólo sobrescribe entradas presentes, resto conserva defaults;
- entrada física desconocida: warning y continuar;
- acción Xbox desconocida: conservar default de esa entrada y warning;
- `NONE` deshabilita esa entrada física;
- se permiten dos botones físicos apuntando al mismo control Xbox para experimentación;
- no reordenar ni sobrescribir automáticamente un archivo existente;
- si no existe, crear plantilla una sola vez;
- cuando `controls.txt` exista y sea válido, tiene prioridad sobre `vita_shooter` para Vita portátil;
- si no existe, conservar temporalmente compatibilidad con `vita_shooter 0/1`;
- no aplicar el archivo a DS3/DS4.

Ejemplo para invertir shoulders/triggers sin recompilar:

```text
l LT
r RT
rear_left LB
rear_right RB
```

Logging de arranque:

```text
[INPUT] vita_mapping source=defaults emulation=xbox
```

o:

```text
[INPUT] vita_mapping source=controls.txt emulation=xbox
```

seguido por una línea compacta con el mapping efectivo, por ejemplo:

```text
[INPUT] xbox_map cross=A circle=B square=X triangle=Y l=LB r=RB rear_left=LT rear_right=RT start=START select=BACK
```

---

# E. Tests de input requeridos

Extender las regresiones para comprobar, como mínimo:

1. sticks existentes no cambian;
2. Cross produce la intención Xbox A y sólo una transición DOWN/UP;
3. Circle/B, Square/X y Triangle/Y;
4. L/R como LB/RB en default;
5. rear-left/rear-right como LT/RT en default;
6. una entrada física no provoca dos acciones Xbox lógicas accidentalmente;
7. trigger digital 0/1 no duplica shoulder;
8. archivo inexistente -> defaults;
9. archivo parcial -> sólo cambia lo indicado;
10. perfil Shooter explícito vía archivo;
11. face buttons remapeados;
12. `LS`/`RS` asignables;
13. `NONE`;
14. claves/acciones inválidas seguras;
15. duplicados lógicos permitidos;
16. DS3/DS4 no afectados;
17. Release sin trazas por frame;
18. reiniciar la app aplica cambios de `controls.txt` sin recompilar.

Host tests y build no sustituyen Vita real.

---

# F. Orden recomendado de trabajo en esta rama

1. Conservar el selector de resolución A/B/C ya presente.
2. No tocar el renderer mientras se depura input.
3. Añadir primero trazas Debug de botones digitales.
4. Reparar la emulación Xbox base hasta que `Cross=A` funcione y `L` deje de provocar dos acciones.
5. Verificar en Vita real el mapping fijo mínimo.
6. Implementar `controls.txt` sobre esa capa Xbox ya correcta.
7. Repetir prueba física con mapping default y al menos una variante editada manualmente.
8. Probar resolución 0 -> 960 -> 864 usando el mismo VPK cuando sea estable.
9. Comparar logs y comportamiento.
10. No integrar a `master` hasta tener evidencia física suficiente de ambas áreas.

## Artefactos y evidencia a devolver

Si una prueba falla:

- devolver el `log_XXXX.log` completo de esa ejecución;
- si hay crash, devolver además el `.psp2dmp` correspondiente;
- indicar el valor exacto de `software_width` y el contenido efectivo de `controls.txt` usados;
- usar el ELF/map exacto de la misma build para simbolizar dumps; no mezclar artefactos de builds anteriores.

## Criterio de éxito de la rama

Un mismo Release VPK debe permitir, tras reiniciar la app:

- cambiar resolución editando `config.txt`;
- cambiar equivalencias físicas Vita -> mando Xbox editando `controls.txt`;
- mantener sticks correctos;
- evitar acciones dobles accidentales;
- hacer funcionar `Cross` como Xbox `A` cuando así esté configurado;
- conservar mandos externos, audio, renderer y SO canónico;
- dejar en los logs evidencia inequívoca de qué resolución y qué mapping estuvieron activos.
