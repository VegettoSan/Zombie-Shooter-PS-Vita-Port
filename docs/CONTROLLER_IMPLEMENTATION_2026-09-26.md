# Controles Vita → Android/Xbox: checkpoint 2026-09-26

## Resultado y límites

Implementación local sobre el port real de WSL, conservando el baseline de FPS
(pase 7, HEAD e149e0e y modificaciones existentes de dependencias). No se hicieron
commits ni publicaciones. Snapshot previo:
`/home/vegettosandev/zombie-controls-baseline-20260926/source.tar.gz` y `working.diff`.
NativeActivity sigue siendo la arquitectura del port. Sin cambios en renderer,
patches del motor, audio, resolución, frecuencia o deadzone de sticks.

Estado: STATICALLY VERIFIED, HOST TEST VERIFIED, BUILD VERIFIED y VPK VERIFIED.
REAL VITA VERIFIED y GAMEPLAY VERIFIED están pendientes. No se afirma que haya
mejorado el mapping observado, los FPS o la estabilidad física sin esa prueba.

## Evidencia APK/SO

JADX 1.5.6 sobre `com.sigmateam.zombieshooter.free.apk`, classes4.dex:

- `com.sigmateam.sige.InputDeviceHelper.getInputSources()I`, static: enumera
  `InputDevice.getDeviceIds()`, llama `getDevice(int)` y combina `getSources()`.
- `InputDeviceHelper.getMotionRanges(Landroid/view/InputDevice;)V`, static:
  recorre rangos y llama a `onAxisInfo(IFFF)V`, static native. Pasa
  `(axis, min, max, max(flat+fuzz, 0.1f))`.
- `com.sigmateam.sige.RegistryEnumerator.enumerateKeys(Landroid/app/Activity;)V`,
  public static: enumera `activity.getPreferences(0).getAll().keySet()` y llama
  a `onKey(Ljava/lang/String;)V`, private static native. Es almacenamiento de
  preferencias, no enumeración de controles.

El SO canónico conserva SHA256
`5e5e2e1bbe86e126c3e2dce5ad97c2e9dc6282305ea7d3e24b49ee4769c5b5b7`.
Referencias ARM resueltas desde literales PC relativos, no offsets copiados:

| Función / consulta | Evidencia |
| --- | --- |
| `input::InputDevice::registerCallback()` | SO+0x3d01b0; literales 0x3d01cc/0x3d01d0/0x3d01d4 → clase helper, `onAxisInfo`, `(IFFF)V` |
| `InputDevice(int)` | SO+0x3d0228; literales 0x3d0354/0x3d0360/0x3d0364 → `android.view.InputDevice`, `getDevice`, `(I)Landroid/view/InputDevice;` |
| `fillMotionRanges()` | SO+0x3d03d0; literales 0x3d04ec/0x3d04f0 → `getMotionRanges`, `(Landroid/view/InputDevice;)V` |
| `onAxisInfo()` | SO+0x3d01d8: callback SoftFP, almacena axis y tres floats en `s_tempRanges` |
| `AndroidJoystickControl::updateDeviceInfo()` | SO+0x3cf274: consume los rangos devueltos para habilitar/configurar ejes |

`source/java.c` reproduce directamente el pequeño bucle Java del helper mediante
el callback registrado en JNI. No introduce un modelo incompleto de List o
MotionRange. Se conservan min/max y el mínimo flat=0.1 del helper original.
Se resuelven clase, firma y static/instance exactos; los IDs nuevos son 200+.
El objeto virtual tiene identidad estable y referencias globales prestadas.

`getId()` se incluye como mínimo requerido por el plan. No se implementan
`getName`, descriptor o vendor: no hay evidencia de que el juego los necesite.
`RegistryEnumerator` conserva su estado de método desconocido, con diagnóstico
acotado en Debug: implementar almacenamiento o fingir preferencias vacías excede
esta corrección de input y podría modificar guardados. Su firma y semántica ya
se recuperaron, eliminando la incertidumbre de investigación de la fase 6.

## Implementación

- FalsoNDK: `device_id` separado de source. Gamepad=1, touch frontal=2.
- Todos los KeyEvents se crean con zero-init, repeatcount=0 y scancode=0.
- Botones GAMEPAD, ejes JOYSTICK y touch TOUCHSCREEN. Sources del dispositivo:
  GAMEPAD | JOYSTICK | DPAD, construidos con constantes Android.
- Cross=A, Circle=B, Square=X, Triangle=Y; sticks X/Y y Z/RZ sin cambios.
- D-pad emite keycodes y HAT_X/HAT_Y. Direcciones opuestas cancelan el HAT.
- Triggers mantienen eje, aliases BRAKE/GAS y botones L2/R2. Externos conservan
  presión 0..1; digital sin presión funciona como 0/1.
- L3/R3 externos → THUMBL/THUMBR. No hay combinaciones nuevas en Vita portátil.
- Traducción física separada en `source/utils/gamepad.c`, override fuerte de un
  hook weak. FalsoNDK sigue produciendo los mismos controles lógicos Android.
- Tipos de puertos comprobados una vez por segundo. DS3/DS4 usan su puerto y
  perfil estándar incluso cuando el candidato Shooter está habilitado.
- Rear touch solo se usa en Vita portátil, y ambos lados funcionan al mismo
  tiempo que hombros físicos. Read/disconnect neutraliza sticks y libera botones.
- Debug: una línea por eje solicitado, transiciones de botón y hasta 24 consultas
  JNI relacionadas distintas. Esas trazas no se compilan en Release.

Todos los cambios de dependencias están reproducidos en `patches/falso_ndk.patch`
y `patches/falso_jni.patch`, con hashes actualizados en `submodules.lock.json`.
Se aplicaron a copias prístinas de las revisiones fijadas y se verificó cada hash.

## Perfil físico y configuración

Por defecto `vita_shooter 0`: L=LB, R=RB, rear superior izquierdo=LT, derecho=RT.
El documento exige medir el tutorial antes de convertir el candidato en default.

Para probar el candidato, añadir o cambiar una única línea en
`ux0:data/zombieshooter/config.txt` y reiniciar el juego:

```text
vita_shooter 1
```

Con esa opción, solo en Vita portátil: L=LT, R=RT, rear superior izquierdo=LB,
derecho=RB. Los márgenes y la mitad inferior del panel trasero siguen sin usarse.
`vita_shooter 0` restaura el perfil estándar. No sustituir el resto del config.
No se empaqueta un config que sobrescriba los ajustes existentes.

## Verificación y siguiente prueba física

`bash tests/run_gamepad_regression.sh`: código real de poll, cola, getters y
vtable JNI, servicios Vita simulados. O0/Debug y O3/Release pasan: identidad,
repeat/scancode, DOWN/UP, face/Start/Select/L3/R3, extremos y centro de sticks,
HAT y direcciones opuestas, aliases, perfiles, chords, presión externa,
desconexión, touch frontal, 1000 polls neutrales sin crecimiento de cola,
metadatos, firmas incorrectas rechazadas, referencias y callback nativo.
La traza de ejes se comprueba una vez por cada uno de los 10 ejes y cero en Release.
`run_jni_field_regression.sh` también pasa en O0/O3.
Se añadió el test al workflow manual, sin ejecutarlo ni publicar remotamente.

Compilar con SDK SoftFP `/usr/local/vitasdk` en directorios propios:
`build-controls-debug` y `build-controls-release`; no se sustituyeron los builds
previos. Flags Release revisados: O3, NDEBUG, SoftFP, FALSOJNI_DEBUGLEVEL=3 y
ZOMBIE_RELEASE_BUILD, sin trazas pesadas. ZIP/VPK y eboot deben coincidir.

En Vita real, instalar Release y recorrer tutorial con perfil estándar primero:

| Xbox | Acción real | Vita estándar | Candidato Shooter |
| --- | --- | --- | --- |
| A/B/X/Y | medir | Cross/Circle/Square/Triangle | igual |
| LB | granada, observación previa | L | rear superior izquierdo |
| RB | medir | R | rear superior derecho |
| LT | medir | rear superior izquierdo | L |
| RT | medir | rear superior derecho | R |
| LS/RS | confirmar movimiento/apuntado | sticks | igual |
| D-pad | medir gameplay/menús | D-pad | igual |
| Start/Select | comprobar | Start/Select | igual |

Después habilitar Shooter si LT/RT son las acciones primarias. Verificar que cada
pulsación de trigger provoca una sola acción, ambos rear funcionan manteniendo
L/R, los sticks no están invertidos, el touch frontal sigue operativo y no hay
regresión de audio/FPS. Probar DS3/DS4 si está disponible.
Si falla: repetir en Debug y devolver solo el log nuevo completo; si hay crash,
también el nuevo `.psp2dmp`. El ELF Debug de esta build queda disponible para
simbolizarlo. Sin esos resultados no se declara éxito de jugabilidad.

Referencias oficiales:
- https://developer.android.com/games/sdk/game-controller/controller-input
- https://developer.android.com/ndk/reference/group/input
- https://docs.vitasdk.org/group__SceCtrlUser.html
- https://github.com/skylot/jadx/releases/tag/v1.5.6

## Artefactos finales

Source ID: `local-e149e0e-1f2866adf6`.

- Debug VPK SHA256: `7156e7a26554c433b4ff7335ff07523d601bc904962dcd44cc7d15198b4be630`.
- Release VPK SHA256: `afbff4112fc508dd03b909f41614d0fdcbe2712a8c1edaf58046c4b3bf5ff414`.
