# Reproducir el local funcional (2026-09-25)

Fuente de verdad: `23d92dc1d60286aaa402c7f544cafdcb4a06ac04` y los cambios que
estaban en disco antes de esta restauración. Se conservan las fuentes del juego
sin cambiar su comportamiento. El remoto anterior se respaldó en
`backup/before-local-restore-20260925` (`2809a6951cec83a460be5777f388edf04a76f259`).

## SDK exacto

El antiguo workflow usaba una imagen nightly móvil con GCC 15 y añadía código
exclusivo de CI. El local usa GCC 10.3.0. Ahora ambos usan la misma copia del SDK,
con sus herramientas, headers, bibliotecas y licencias originales:

- Ruta única: `/usr/local/vitasdk`.
- Asset: release `sdk-functional-local-20260925`, `vitasdk-softfp-local-20260925.tar.gz`.
- SHA-256: `25e1271b50aef4e0b4e7be837b6c2316507c47a0f56127a5b2d633f6a6aa0878`.
- Versiones originales: `scripts/vitasdk-version.txt`.
- Hashes de archivos: `scripts/vitasdk-files.sha256.json`.
- Nunca modificar `/usr/local/vitasdk-hardfp`.

En una máquina Linux x86_64 nueva, con Git, Python 3, CMake >= 3.19, Make, curl,
tar y unzip disponibles:

```bash
git clone --recurse-submodules https://github.com/VegettoSan/Zombie-Shooter-PS-Vita-Port.git
cd Zombie-Shooter-PS-Vita-Port
export VITASDK=/usr/local/vitasdk
bash scripts/install_baseline_sdk.sh
export PATH="$VITASDK/bin:$PATH"

cmake -S . -B build-session-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-session-debug -j"$(nproc)"
cmake -S . -B build-session-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-session-release -j"$(nproc)"
```

El instalador rechaza sustituir un SDK existente diferente; si ya existe sólo
lo verifica. CMake aplica/verifica automáticamente los parches. Para recompilar
desde cero se pueden retirar únicamente los directorios de build propios;
no hace falta borrar submódulos ni datos del juego. El alias `vita-softfp`
del usuario equivale a seleccionar este VITASDK y su `bin` en PATH.

## Receta observada y conservada

- C/C++/link: `-mfloat-abi=softfp`; toolchain y bibliotecas originales.
- Debug C: `-Wl,-q -mfloat-abi=softfp -std=gnu11 -Wno-deprecated -O1 -g3 -O0 -g -DDEBUG -D_DEBUG`.
- Release C: `-Wl,-q -mfloat-abi=softfp -std=gnu11 -Wno-deprecated -O2 -g1 -O3 -DNDEBUG`.
- Los últimos flags de optimización prevalecen: Debug O0, Release O3.
- Ambos: `DEBUG_SOLOADER`, `SO_UTIL_VERBOSE=1`, `NDK_PORT`, `USE_SCELIBC_IO`,
  `USE_GLSL_SHADERS`, `_GNU_SOURCE`, `__POSIX_VISIBLE=999999`, rutas locales.
- Debug añade `FALSOJNI_DEBUGLEVEL=0`, `ZOMBIE_THREAD_TRACE=1`, `ZOMBIE_STALL_DUMP=1`.
- No se redefine FALSOJNI_DEBUGLEVEL para Release: se conserva su default local.
- VitaGL Debug: `SOFTFP_ABI=1 TEST=1 LOG_ERRORS=1`.
- VitaGL Release: `SOFTFP_ABI=1 LOG_ERRORS=1`.
- VitaGL limpia sus objetos al construir cada variante, evitando reutilizar flags
  de otro build en su árbol de fuentes compartido. Compilar las variantes en serie.
- FalsoJNI: fuentes C incorporadas en `so_loader`, con parche versionado.
- FalsoNDK: static target C++20, flags heredados, con parche versionado.
- OpenSLES: copia del archivo SDK a `build-session-*/libOpenSLES_zombie.a`;
  se reemplazan IBufferQueue.o, CAudioPlayer.o y Vita.o, `-std=gnu17 -O3`,
  defines `USE_OUTPUTMIXEXT USE_SDL USE_SNDFILE`, compiler SoftFP default verificado.
- Se conserva el orden de link local, pthread whole-archive y las bibliotecas
  OpenSLES/codecs/GL/SDK. No shim getentropy ni include exclusivo de CI.
- Únicamente Debug y Release; sin Perf/Profile/Benchmark.

## Revisiones fijadas

| Dependencia | Commit |
| --- | --- |
| FalsoJNI | `083d5a07e025c6dfbb54ac68fd1acf696c23a86c` |
| FalsoNDK | `7dceb3bb34de5c71a3c5b98ed30c57a0eb53c73a` |
| VitaGL | `9c23758ff17893db63887f95e9a4d9350c986d88` |
| so_util | `c4732373e33d808cd885dec3c2c75302cc4c739b` |

`patches/submodules.lock.json` comprueba también el contenido completo parcheado.
Los submódulos FalsoJNI/NDK aparecen modificados tras aplicar los parches: esto
es esperado, y todo cambio semántico está versionado en el repositorio principal.

## Validación local

Ambos builds se configuraron en directorios nuevos después de preservar los
originales en `~/zombie-restore-backup-20260925/`. Compilan y pasan `unzip -t`.
Se compararon flags/defines y link.txt con los builds originales: idénticos.
Los parches sobre archivos extraídos de commits limpios reproducen TODOS los
hashes de las fuentes locales de FalsoJNI y FalsoNDK.

SHA-256 VPK Debug: `0aaecd248e8779220db0cfded215af6f113037b6fb373c3c37d8abbf5b961845`.
SHA-256 VPK Release: `f54800370ed640453ee0db69d5c1229648dbadec3a1c334a814484e3c2918efb`.

La igualdad exigida corresponde a fuentes, SDK y receta, no a un ZIP bit a bit:
los timestamps del empaquetado, rutas Debug y versión de CMake pueden variar.
Esta restauración no constituye una nueva prueba física ni garantiza estabilidad
completa del juego. Las observaciones previas y los pendientes están en PORT_STATUS.
