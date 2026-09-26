# Codex — hallazgos reales de input en PS Vita (2026-09-26)

Este archivo contiene **instrucciones y evidencia de hardware solamente**. No asumir que el problema actual es un simple remapeo. PS Vita real es la autoridad.

## Resultado observado en Vita real

Con el ajuste de controles actualmente presente en `master`:

- los dos joysticks analógicos funcionan correctamente;
- el botón físico `L` provoca **dos acciones del juego al mismo tiempo**: enciende/apaga la linterna y además lanza una granada;
- los demás botones físicos probados no realizan su acción esperada;
- en particular `Cross`, que debería representar el botón Xbox `A`, no funciona como `A`;
- por tanto el estado actual NO puede considerarse un mando Xbox funcional con botones simplemente mal acomodados.

No convertir estas observaciones en una tabla definitiva de acciones del juego: sólo se ha confirmado lo descrito arriba.

## Interpretación técnica que Codex debe usar como hipótesis, no como hecho probado

El contraste es importante:

- sticks correctos -> la ruta de ejes / motion events está suficientemente viva como para que el juego reciba movimiento;
- botones digitales incorrectos o ausentes -> la ruta de botones / key events / aliases / metadatos del dispositivo Xbox lógico debe investigarse por separado;
- `L` disparando dos acciones puede significar que una pulsación física acaba generando más de una representación lógica, que un alias de shoulder/trigger se está duplicando, o que el juego recibe un código/eje equivocado. No asumir cuál de estas causas es correcta sin traza física.

Por ello, NO intentar solucionar el estado actual únicamente intercambiando `A/B/X/Y/LB/RB/LT/RT` en `source/utils/gamepad.c` ni únicamente creando `controls.txt`.

## Objetivo correcto

La Vita portátil debe comportarse primero como **un único mando Xbox lógico coherente**.

Cadena conceptual:

```text
botón físico Vita
    -> exactamente un control Xbox lógico configurado
    -> backend interno del port
    -> evento que Zombie Shooter espera
```

Para una pulsación normal no debe aparecer accidentalmente otro control Xbox lógico adicional.

El archivo `controls.txt` descrito en `CODEX_TODO_RUNTIME_CONFIG_RESOLUTION_CONTROLS.md` será la capa de selección física -> Xbox, pero sólo después de que la emulación base Xbox esté verificada.

## Orden obligatorio de trabajo para input

### Fase A — diagnosticar la ruta digital existente

Antes de implementar el remapeo configurable:

1. Mantener los sticks y su ruta actual sin cambios.
2. Trazar en **Debug** cada transición física de botón, de forma acotada y sólo al cambiar estado.
3. Para cada transición registrar como mínimo:
   - botón físico Vita detectado;
   - máscara física antes/después de `fndk_translate_pad_buttons()`;
   - control Xbox lógico que se pretende producir (`A/B/X/Y/LB/RB/LT/RT/LS/RS/START/BACK/DPAD_*`);
   - evento interno realmente emitido;
   - `deviceId`;
   - `source`;
   - DOWN/UP o valor de trigger;
   - si se encoló un KeyEvent, MotionEvent, trigger axis y/o alias de botón.
4. Una sola pulsación debe poder seguirse de extremo a extremo en el log.
5. Release no debe recibir spam por frame.

Ejemplo de intención de log Debug (formato exacto a elección de Codex):

```text
[INPUT] physical=cross down=1 xbox=A
[INPUT] emit xbox=A key=<...> source=<...> device=<...> action=DOWN
[INPUT] physical=cross down=0 xbox=A
[INPUT] emit xbox=A key=<...> source=<...> device=<...> action=UP
```

Para `L`, la traza debe hacer evidente si se emiten accidentalmente dos controles/eventos distintos.

### Fase B — verificar el modelo Xbox lógico

Usar un mapping temporal fijo y mínimo para validar la infraestructura, no para decidir todavía el layout final:

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

Criterios:

- `cross` debe ser recibido como Xbox `A` y producir una sola acción asociada por pulsación;
- `circle`, `square`, `triangle` deben llegar individualmente como `B/X/Y`;
- `L` como `LB` NO debe producir simultáneamente el comportamiento de `LT` ni otro control lógico;
- `R` como `RB` igual;
- `rear_left` y `rear_right` deben representar `LT/RT` con semántica de trigger completamente liberado/presionado en Vita portátil;
- DOWN y UP deben estar balanceados;
- no dejar botones pegados;
- no emitir una segunda pulsación por frame mientras el botón permanece presionado;
- el `deviceId` y `source` deben corresponder al mismo gamepad lógico que ya expone los sticks.

No declarar éxito sólo porque un host test recibe el código esperado: comprobar en Vita real.

### Fase C — investigar específicamente Cross/A

Como `Cross -> A` está configurado conceptualmente pero no funciona en Vita real, Codex debe revisar de extremo a extremo:

- detección real de `SCE_CTRL_CROSS`;
- traducción física;
- generación del botón Xbox `A` lógico;
- key/event code interno usado por FalsoNDK;
- source GAMEPAD/JOYSTICK/DPAD del dispositivo;
- device ID consistente;
- action DOWN/UP;
- repeat count y scan code;
- enqueue/dequeue del evento;
- que el juego efectivamente consulte/consuma esa ruta.

No asumir que cambiar `Cross` a otro keycode arregla la causa.

### Fase D — investigar específicamente L duplicado

La observación de hardware es:

```text
L -> linterna + granada simultáneamente
```

Codex debe determinar con trazas si una pulsación `L` produce:

- `LB` + `LT` simultáneamente;
- shoulder key + trigger axis;
- dos keycodes distintos;
- alias duplicado;
- dos eventos del mismo control;
- o un único control lógico que el juego interpreta inesperadamente.

No eliminar aliases de trigger a ciegas: los mandos externos pueden necesitarlos y la ruta de `LT/RT` debe conservar semántica Xbox.

La corrección debe lograr **un control Xbox lógico por asignación física**, salvo que un alias interno sea estrictamente necesario para representar ese mismo control y esté demostrado que el juego no lo trata como una segunda acción.

### Fase E — sólo después implementar `controls.txt`

Cuando `A/B/X/Y/LB/RB/LT/RT` funcionen individualmente y de forma estable en Vita real, implementar el archivo descrito en:

`docs/CODEX_TODO_RUNTIME_CONFIG_RESOLUTION_CONTROLS.md`

`controls.txt` debe seleccionar qué control Xbox lógico representa cada botón físico. No debe ocultar ni compensar un backend digital roto.

## Tests host que deben añadirse/ajustarse

Además de los tests ya pedidos:

1. Cross produce exactamente un `A` DOWN y un `A` UP.
2. Mantener Cross presionado no genera spam de DOWN por frame.
3. L produce únicamente el control Xbox lógico configurado.
4. L no produce simultáneamente LB+LT salvo que explícitamente se configure así mediante dos entradas físicas distintas; una entrada física normal representa una sola asignación lógica.
5. Triggers digitales generan valores 0/1 correctos y no quedan pegados.
6. Cada face button es independiente.
7. Device ID/source de botones coincide con el gamepad lógico cuyos sticks funcionan.
8. Cola de input queda balanceada después de secuencias largas.
9. DS3/DS4 no se alteran por la corrección específica de Vita portátil.

## Prueba física requerida después de la corrección

En Release, comprobar primero y por separado:

- Cross
- Circle
- Square
- Triangle
- L
- R
- rear-left
- rear-right
- Start
- Select
- D-pad
- ambos sticks

Anotar para cada uno **qué acción real ocurre en Zombie Shooter**. Si algún botón produce dos acciones, la emulación Xbox todavía no está correcta y no debe avanzarse a decidir el layout definitivo.

Si falla una entrada, repetir en Debug y conservar el log completo. Si hay crash, adjuntar también el `.psp2dmp` correspondiente.

## Prioridad

Para input, el orden correcto queda:

1. conservar sticks que ya funcionan;
2. reparar/verificar ruta digital del gamepad Xbox lógico;
3. eliminar duplicación observada en `L` sin romper triggers externos;
4. lograr `Cross -> A` funcional;
5. verificar todos los controles Xbox lógicos uno por uno en Vita real;
6. sólo entonces implementar/usar `controls.txt` para acomodar el layout final.
