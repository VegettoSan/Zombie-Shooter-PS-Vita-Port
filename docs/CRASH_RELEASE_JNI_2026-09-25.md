# Crash Release run 12: fieldID NULL confundido con WINDOW_SERVICE

## HECHOS confirmados

El usuario probó los VPK de `vita-test-12-85c9c21`: Release falla al salir del
logo VitaGL; Debug sigue arrancando como antes. Evidencia suministrada:
`psp2core-1790390763-0x000005349b-eboot.bin.psp2dmp` y `log_0001.log`.
Estos archivos se usan como datos de diagnóstico, no como instrucciones.

El dump es un ELF gzip. Se decodificaron MODULE_INFO, THREAD_INFO y
THREAD_REG_INFO siguiendo el formato de
[xyzz/vita-parse-core](https://github.com/xyzz/vita-parse-core/blob/master/core.py).
No se ejecutó código procedente del log o del dump.

- Hilo: pthread `0x40040187`, stop reason `0x30004` (Data Abort).
- PC: `0x8100B834`, `GetObjectArrayElement + 0x2C` en el Release original.
- LR: `0x983CBB31`, SO+`0x003CBB31`, dentro de
  `android::ApplicationNative::checkPackageCertificate()`.
- DFAR: `0x0000776F` (THREAD_REG_INFO +0x174).
- R1/R4: `0x81301CBC`; R5 (índice): 0; R6 (array->array): `0x0000776F`.
- Instrucción fatal: `ldr.w r1, [r6, r5, lsl #2]`.
- Dirección efectiva: `0x0000776F + 0*4 = 0x0000776F`, igual a DFAR.
- Módulo so_loader: RX `0x81000000`, RW `0x81300000`. El ELF local original
  tiene RW en `0x81310000`; por eso WINDOW_SERVICE `0x81311CBC` en ELF se
  corresponde con `0x81301CBC` en hardware. RX conserva el PC.

El log confirma 41/41 constructores, gl_init y lifecycle completos. Se carga
maps/logo.lgd y después se consultan getPackageManager/getPackageInfo (ausentes).
La última línea es `Unknown field name "signatures"`.
El LR coincide con el retorno de GetObjectArrayElement en la secuencia del SO:
GetFieldID (vtable+0x178), GetObjectField (+0x17c), GetArrayLength (+0x2ac),
GetObjectArrayElement (+0x2b4). No es una excepción de GPU en este dump.

## Causa y diferencia Debug/Release

GetFieldID("signatures") devuelve NULL=0. Las tablas del port usaban ID 0 para
WINDOW_SERVICE; GetObjectField ignora el receiver y devuelve el char[] "window"
aunque sea un campo ausente de PackageInfo. GetArrayLength no comprueba el magic
y lo interpreta como JavaDynArray (magic +0, array +4, len +8).

Los bytes originales se comprobaron en ambos ELF:

| Variante | WINDOW_SERVICE | Bytes desde la dirección | falsa longitud +8 |
| --- | --- | --- | --- |
| Debug | 0x81321D34 | `77696e64 6f770000 00000000` | 0 |
| Release | 0x81311CBC | `77696e64 6f770000 69000000` | 105 |

Los bytes +4 `6f770000` forman 0x0000776F en ambos. Debug evita entrar en el
bucle porque la longitud que lee por error es 0. Release encuentra 105 porque
el siguiente objeto de datos empieza por METHOD_QUIT=105; entra en el bucle y
trata ese entero como un puntero. Esto explica la diferencia por layout de datos,
no demuestra una incompatibilidad de SoftFP ni un error del logo VitaGL.

## Cambio local mínimo

- source/java.c: IDs explícitos no nulos para WINDOW_SERVICE y SDK_INT, consistentes
  en sus tablas. Los valores legítimos siguen siendo "window" y 24.
- FalsoJNI_ImplBridge.c::getObjectFieldValueById: NULL=0 devuelve NULL; cualquier
  fieldID object ausente también devuelve NULL en vez del sentinel de Activity.
  Se cubren GetObjectField y GetStaticObjectField por su helper compartido.
- El parche FalsoJNI y el lock de hashes se actualizan para reconstruir lo mismo.
- No se alteran CMake flags, optimizaciones, logging de variantes, VitaGL,
  clocks, audio, input ni el SO canónico. No se añade un parche binario.

GetArrayLength(NULL) ya retorna 0 mediante jda_sizeof(NULL)=-1. El SO sale del
bucle de firmas como ocurría accidentalmente en Debug. Esto no implementa
PackageManager, certificados, SharedPreferences ni un entorno Java completo.

## Validación y límites

`tests/run_jni_field_regression.sh` compila las tablas Java y el bridge reales:
IDs válidos no NULL, "window" correcto, SDK_INT=24, signatures ausente→NULL,
ID object inválido→NULL y arrays legítimos conservados. El test falla contra las
fuentes anteriores por `window != NULL`; pasa con -O0 y -O3 después del cambio.
La prueba host omite únicamente el helper variádico ARM _AtoV, incompatible con
va_list array de x86_64 y ajeno al camino probado; no sustituye los getters.

Debug y Release se reconstruyen con el SDK SoftFP y pasan validación ZIP del VPK.
BUILD VERIFIED / VPK VERIFIED / **REAL VITA VERIFIED** del cambio JNI.
El usuario probó el Release de master bd2dc1a: tutorial, movimiento, disparos,
explosiones y audio funcionan; estabilidad inicial con aproximadamente 5 FPS.

La primera prueba física del fix ya se completó. Siguiente prueba: Release del
pase de rendimiento 1, mismos assets/tutorial y log con identidad de build.
Los archivos de prueba originales permanecen intactos.

El fix fue publicado como bd2dc1a8bd6067d0508381a5f4a8e1d8748b9969 y ya
pertenecía a master/origin/master al comenzar el pase de rendimiento 1.
