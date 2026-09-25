# Diagnóstico — Release del run #8 seguía produciendo logs Debug

Fecha: 2026-09-25

Estado: **USER CONFIRMED RELEASE VPK / BUILD CONTRACT WAS NOT VERIFIED / FIX PREPARED**

## Aclaración del usuario

El usuario confirmó explícitamente que en la prueba asociada a `log_0003.log` instaló:

```text
Zombie-Shooter-Vita-Release.vpk
```

del Pre-release del run #8:

```text
tag: vita-test-8-9a8be03
commit: 9a8be03425d73303c7c5ab13eab9ab5adc65c12a
```

No volver a explicar este resultado como un simple error del usuario al elegir Debug.

## Evidencia contradictoria observada en hardware

Aunque el archivo instalado fue Release, `log_0003.log` contiene decenas de miles de líneas, incluyendo tráfico como:

```text
[debug] [FalsoJNI] ...
[debug] [FalsoNDK] ...
```

Además, la instrumentación añadida antes del run #8 debía producir:

```text
[PERF] mem phase=before_vgl ...
[PERF] mem phase=after_vgl ...
[PERF] mem phase=runtime ...
```

pero el log real contiene cero líneas `mem phase=` mientras sí contiene muchas ventanas `[PERF] present`.

Conclusión: el comportamiento real de la Release del run #8 no coincidió con el contrato de logging/telemetría que se pretendía compilar.

## Lo que sí confirma CI del run #8

El workflow configuró y compiló dos árboles separados:

```text
build-ci-debug   -DCMAKE_BUILD_TYPE=Debug
build-ci-release -DCMAKE_BUILD_TYPE=Release
```

y copió el segundo como:

```text
dist/Zombie-Shooter-Vita-Release.vpk
```

Por tanto no hay evidencia de que el workflow haya copiado deliberadamente el VPK Debug con nombre Release.

## Limitación retrospectiva

El run #8 NO guardaba `flags.make` ni ejecutaba una aserción sobre las definiciones reales del compilador. Los logs de Actions tampoco mostraban las líneas completas de compilación.

Por eso no es posible demostrar retrospectivamente cuál de estas condiciones provocó el comportamiento:

- dependencia poco fiable de `NDEBUG` para seleccionar el logger Release,
- propagación inesperada de definiciones globales de CMake,
- una discrepancia de instalación/eboot que no estaba identificada desde el propio binario,
- u otra interacción del toolchain.

No inventar una causa exacta sin el `flags.make` de ese run.

## Corrección estructural

Se eliminó la dependencia de `NDEBUG` como identidad principal del port.

CMake ahora define explícitamente y de forma target-scoped exactamente una variante:

```text
Debug:
  ZOMBIE_DEBUG_BUILD=1
  FALSOJNI_DEBUGLEVEL=0
  ZOMBIE_BUILD_VARIANT="Debug"

Release:
  ZOMBIE_RELEASE_BUILD=1
  FALSOJNI_DEBUGLEVEL=4
  ZOMBIE_BUILD_VARIANT="Release"
```

`logger.h` usa `ZOMBIE_DEBUG_BUILD` / `ZOMBIE_RELEASE_BUILD`, no `NDEBUG`, para decidir qué tráfico conservar.

Cada log nuevo incluye además una cabecera inequívoca:

```text
=== BUILD variant=<Debug|Release> id=<short git sha> ===
```

Esto evita depender del nombre del VPK o de memoria humana para saber qué ejecutó la Vita.

## Invariantes nuevas del workflow

Antes de compilar/publicar, CI inspecciona:

```text
build-ci-debug/CMakeFiles/so_loader.dir/flags.make
build-ci-release/CMakeFiles/so_loader.dir/flags.make
```

y falla si no encuentra:

```text
Debug:   ZOMBIE_DEBUG_BUILD=1 + FALSOJNI_DEBUGLEVEL=0
Release: ZOMBIE_RELEASE_BUILD=1 + FALSOJNI_DEBUGLEVEL=4
```

o si encuentra la identidad contraria.

El artifact del workflow guarda también:

```text
build-flags.txt
```

para auditoría posterior.

## Telemetría de memoria

Las llamadas existentes `[PERF] mem ...` permanecen. Con la identidad Release explícita deben pasar por el filtro `[PERF]` del logger.

La siguiente prueba se considera válida sólo si el log empieza con:

```text
=== BUILD variant=Release id=<sha> ===
```

y contiene al menos:

```text
[PERF] mem phase=before_vgl
[PERF] mem phase=after_vgl
```

Si no ocurre, detener el A/B y corregir instrumentación antes de sacar conclusiones de memoria.

## Clocks / PSVshell

Se descubrió además que builds anteriores hacían:

```text
ARM 444 / BUS 222 / GPU 222 / XBAR 166
```

mediante `scePowerSet*`, anulando el perfil máximo que el usuario elegía en PSVshell.

El port ya no fija esos clocks durante la prueba. Ahora conserva el perfil externo y registra:

```text
[PERF] clocks_preserved arm=... bus=... gpu=... xbar=...
```

Por tanto la próxima prueba podrá medir por primera vez el perfil real elegido por PSVshell.

## SO canónico y crash de zona nueva

`demo/libzombie_shooter.so` queda como copia canónica protegida de análisis. El workflow genera una ventana mínima de desensamblado alrededor de:

```text
SO+0x00515AE4  crash PC
SO+0x00515A5F  LR
```

junto con `addr2line` y símbolos cercanos. El resultado se guarda sólo en el artifact del workflow como:

```text
crash-zone-disasm.txt
```

No se publica el `.so` ni el desensamblado como asset del Pre-release.

## Próxima validación

1. Ejecutar un workflow nuevo desde `master`.
2. Confirmar que CI supera las aserciones Debug/Release.
3. Revisar `crash-zone-disasm.txt` para identificar la instrucción de `SO+0x00515AE4`.
4. Instalar `Zombie-Shooter-Vita-Release.vpk`.
5. Confirmar en el nuevo log la cabecera `variant=Release`, telemetría de memoria y clocks preservados.
6. Sólo entonces repetir FPS/memoria y, si se reproduce, el crash de zona nueva.
