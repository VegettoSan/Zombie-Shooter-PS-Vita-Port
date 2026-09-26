# Run #16 — guía de prueba runtime

Rama: `fix/run16-disable-render-scale`

Esta rama prueba resolución y emulación de mando Xbox sin modificar `master` runtime.

## 1. Resolución

Archivo:

`ux0:data/zombieshooter/config.txt`

Probar en este orden, reiniciando completamente el juego entre cambios:

```text
software_width 0
```

Baseline seguro. No instala el hook de render-scale.

Luego:

```text
software_width 960
```

Y sólo si funciona:

```text
software_width 864
```

No hace falta reinstalar el VPK entre valores.

## 2. Controles Xbox

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

## 3. Prueba diagnóstica recomendada de botones

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

## 4. Logs

Release registra el mapping Xbox efectivo una vez al iniciar con una línea `[PERF] [INPUT] xbox_map ...`.

Debug además registra cambios de máscara física Vita -> máscara Xbox y FalsoNDK ya registra transiciones de KeyEvent. Esto permite correlacionar una pulsación física con el control lógico y el evento enviado.

## 5. Qué devolver después de la prueba

Para cada resolución: indicar si inicia, carga menú/tutorial/gameplay y FPS aproximados.

Para controles: indicar la acción real observada para Cross, Circle, Square, Triangle, L, R, rear-left, rear-right, Start, Select y D-pad.

Si hay un comportamiento duplicado o un botón que no responde, adjuntar el log Debug de esa misma configuración. Si hay crash, adjuntar también el `.psp2dmp` correspondiente.
