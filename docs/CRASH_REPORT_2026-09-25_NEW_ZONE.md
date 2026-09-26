> Archivo histórico conservado de `2809a6951cec83a460be5777f388edf04a76f259`, respaldado en `backup/before-local-restore-20260925`. Sus observaciones de hardware siguen siendo evidencia de esos builds; las descripciones de código, flags, clocks, MSAA y workflow corresponden a esa rama y no implican que estén activas en master restaurado. Estado actual: `PORT_STATUS.md`.

# Crash report — nueva zona del tutorial — 2026-09-25

Estado: **REAL VITA REPRODUCED / ROOT CAUSE NOT YET PROVEN**

Este caso se mantiene separado de:

- el antiguo bloqueo de audio en `IBufferQueue_Clear` (ya corregido y verificado),
- el crash conocido de segunda entrada al tutorial,
- las pruebas de FPS/MSAA,
- y cualquier hipótesis de agotamiento de memoria todavía no demostrada.

## Build / prueba

El usuario pretendía probar el Pre-release del run #8:

```text
tag: vita-test-8-9a8be03
commit: 9a8be03425d73303c7c5ab13eab9ab5adc65c12a
objetivo: telemetría de memoria + gameplay Release
```

Hardware: PS Vita real.
PSVshell: usuario configuró todos los clocks al máximo.

Resultado observado:

```text
LOADING completó sin crash
tutorial: ~5 FPS
movimiento/disparos: funcionales como antes
entrada a una zona nueva no visitada: crash
```

Archivos de evidencia recibidos:

```text
log_0003.log
psp2core-1790373271-0x00000438c3-eboot.bin.psp2dmp
```

## Hallazgo 1 — el port está sobrescribiendo los clocks de PSVshell

El propio log registra al inicio:

```text
Clocks set: ARM 444 / BUS 222 / GPU 222 / XBAR 166
```

Y `source/utils/init.c` ejecuta actualmente:

```c
scePowerSetArmClockFrequency(444);
scePowerSetBusClockFrequency(222);
scePowerSetGpuClockFrequency(222);
scePowerSetGpuXbarClockFrequency(166);
```

Por tanto, aunque PSVshell se haya configurado al máximo antes de abrir el juego, el ejecutable vuelve a fijar esos valores al iniciar. La prueba anterior NO demuestra rendimiento con el overclock máximo real de PSVshell.

Regla futura: no usar esta ejecución como A/B de clocks. Antes del próximo test de frecuencia, eliminar/neutralizar este override o hacer que el port sólo registre las frecuencias actuales sin reducirlas.

## Hallazgo 2 — el log observado no corresponde de forma demostrable a la Release silenciosa/telemetría esperada

La build de telemetría debería producir líneas:

```text
[PERF] mem phase=before_vgl ...
[PERF] mem phase=after_vgl ...
[PERF] mem phase=runtime ...
```

En `log_0003.log` hay 101 ventanas `[PERF] present`, pero **cero** líneas `[PERF] mem phase=`.

Además el log contiene tráfico completo de Debug/FalsoJNI/FalsoNDK, por ejemplo:

```text
[debug] [FalsoJNI] ...
[debug] [FalsoNDK] ...
```

Esto deja abierta una ambigüedad que no debe ocultarse:

1. pudo haberse instalado el VPK Debug o una build anterior por error, o
2. la configuración Release no está aplicando/silenciando como se esperaba, o
3. el binario que se ejecutó no corresponde al commit esperado.

No declarar todavía que la telemetría de memoria falló en runtime ni que los pools están agotados. Primero hay que hacer inequívoca la identidad de build en el propio log.

Recomendación permanente: añadir un marcador de build al arranque, por ejemplo:

```text
[PERF] build_config=Release commit=<sha>
```

para que ningún test futuro dependa del nombre del VPK o de memoria humana.

## Hallazgo 3 — rendimiento: `eglSwapBuffers` no es el cuello dominante

Del log:

```text
101 ventanas [PERF] present
~1955 frames
~560.235 s agregados
FPS agregado aproximado: 3.49
swap promedio ponderado: ~200 us
```

Ejemplo real:

```text
[PERF] present frames=14 elapsed_ms=5077 fps_x10=27
swap_avg_us=191 swap_max_us=224 frame_max_us=3238205
```

Un `swap` de ~0.2 ms no explica frames de ~200 ms o más. El tiempo se consume mayormente antes de `eglSwapBuffers`: engine/CPU, I/O, JNI/NDK wrappers, esperas/sincronización, carga de recursos, audio u otro trabajo de frame.

No priorizar reducción de resolución/fill-rate por este dato.

## Hallazgo 4 — el crash es un Data Abort dentro del Android SO

El `.psp2dmp` se descomprimió como ELF32 ARM EABI5 core y se analizaron `THREAD_INFO`, `THREAD_REG_INFO`, `STACK_INFO` y los fault registers.

Hilo que falla:

```text
thread: pthread
TID: 0x400301B9
stop reason: 0x30004 = Data abort exception
status: Running
```

El juego carga el Android SO en:

```text
LOAD_ADDRESS = 0x98000000
```

Registros críticos:

```text
PC = 0x98515AE4  => libzombie_shooter.so + 0x00515AE4
LR = 0x98515A5F  => libzombie_shooter.so + 0x00515A5F
DFSR = 0x000000F5
DFAR = 0x07D36E9C
```

`DFAR` es la dirección de datos que produjo el abort. `0x07D36E9C` no pertenece al rango normal del SO cargado ni a los módulos Vita conocidos, por lo que es compatible con un puntero inválido/corrupto/stale. Eso NO demuestra por sí solo de dónde surgió ese puntero.

GPRs del hilo:

```text
R0  = 0x8140411C
R1  = 0x86932D80
R2  = 0x00000000
R3  = 0x866D8508
R4  = 0x8692E800
R5  = 0x00000000
R6  = 0x00000080
R7  = 0x92500EB8
R8  = 0x00000080
R9  = 0xFFFFFFA3
R10 = 0xFFFFFF63
R11 = 0x848932D8
R12 = 0x989442DC
SP  = 0x92500E38
LR  = 0x98515A5F
PC  = 0x98515AE4
```

Uso de stack del hilo:

```text
peak:    7224 bytes
current: 456 bytes
```

Por tanto este crash **no es un stack overflow**.

Direcciones plausibles del mismo SO encontradas alrededor del stack:

```text
0x98515F5F => SO + 0x00515F5F
0x98515961 => SO + 0x00515961
0x9850BB99 => SO + 0x0050BB99
```

La siguiente prueba de causa debe desensamblar/decompilar al menos:

```text
0x00515AE4  crash PC
0x00515A5F  LR
0x00515F5F
0x00515961
0x0050BB99
```

contra el `libzombie_shooter.so` exacto del juego.

## Hallazgo 5 — inmediatamente antes del crash aparecen recursos de zona ausentes

Las últimas líneas útiles de `log_0003.log` muestran primero intentos repetidos de `commonAssets` por JNI:

```text
NewStringUTF(env, "commonAssets")
CallObjectMethodV(... method ID 0 ...)
method ID 0 not found!
```

Después aparecen recursos que no están en los datos actuales:

```text
music/rain.ogg
vid/2010.vid
vid/1071.vid
vid/413.vid
vid/2020.vid
```

Para los `.vid`, el engine informa:

```text
Can't open 'vid/<id>.vid'
[VID] Can't load PAL section
[VID] Can't load DATA section
[VID] Can't read cadr data ...
```

El log termina inmediatamente después de estos fallos y el core registra el Data Abort dentro del SO.

Esto es **correlación fuerte** con el problema ya conocido de PAD/`commonAssets; no es todavía demostración de causalidad. No parchear el crash devolviendo datos falsos ni ignorando indiscriminadamente errores de recursos.

Dato cuantitativo de esta ejecución:

```text
"commonAssets"            ~1977 apariciones
"Wrong AES key length"     ~221 apariciones
"rain.ogg"                  63 apariciones
```

## Hipótesis priorizadas tras esta evidencia

### H1 — recurso faltante / flujo `commonAssets` deja objeto inválido

Prioridad: ALTA.

La zona nueva solicita recursos que no están presentes, el bridge de `commonAssets` no resuelve un método válido y poco después el engine cae leyendo una dirección inválida.

Necesita confirmación por desensamblado en `SO+0x00515AE4`.

### H2 — corrupción/stale pointer independiente de assets

Prioridad: MEDIA.

DFAR es inválido y podría provenir de corrupción previa, use-after-free, estructura parcialmente inicializada u otro bug de compatibilidad. Sólo el contexto de la instrucción del crash puede separarlo de H1.

### H3 — OOM como causa directa

Prioridad: NO DEMOSTRADA.

PSVshell muestra MEM/VMEM/PHY reservados al máximo, pero esta ejecución no contiene la telemetría interna `vglMemFree/vglMemTotal`. No afirmar OOM hasta obtener medición real o evidencia de fallo de allocator.

### H4 — VitaGL/GPU

Prioridad: BAJA para este crash concreto.

El PC del crash está dentro del SO del juego y el stop reason es Data Abort, no GPU exception. Esto no descarta bugs gráficos generales, pero no hay evidencia para responsabilizar a VitaGL de este fallo.

## Próximos pasos correctos

1. Obtener/desensamblar el `libzombie_shooter.so` exacto en `+0x00515AE4` y funciones vecinas.
2. Determinar qué registro/base usa la instrucción que provoca el acceso a `DFAR=0x07D36E9C`.
3. Rastrear esa estructura/objeto hasta el loader de VID/resource/commonAssets si corresponde.
4. Corregir la identificación de build: imprimir configuración + commit al inicio.
5. Dejar de sobrescribir un overclock externo con 444/222/222/166 durante las pruebas A/B.
6. Repetir la telemetría de memoria sólo después de verificar en el log que se está ejecutando la build correcta.
7. Mantener separados: este crash de nueva zona, el crash de segunda entrada y el problema de FPS.

## Regla de continuidad

No afirmar que este crash es causado por memoria o por archivos `.vid` hasta identificar la instrucción en `SO+0x00515AE4`. La evidencia actual permite decir:

```text
Data Abort dentro de libzombie_shooter.so
+ puntero de datos inválido
+ recursos de zona ausentes inmediatamente antes
+ commonAssets/PAD no resuelto
```

pero aún no permite convertir esa correlación en causalidad.
