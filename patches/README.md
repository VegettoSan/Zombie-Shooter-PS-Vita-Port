# Parches reproducibles de dependencias

CMake ejecuta `scripts/prepare_build.py` automáticamente para Debug y Release.
Verifica el SDK SoftFP original, las revisiones fijadas y los hashes de todos los
archivos versionados de FalsoJNI, FalsoNDK, so_util y VitaGL. Aplica los parches
cuando faltan y acepta una segunda ejecución sólo si las fuentes coinciden.
No actualiza submódulos ni descarta ediciones del usuario.

`falso_jni.patch` conserva PopLocalFrame, IsInstanceOf(Context) y el logging local.
`falso_ndk.patch` conserva assets/buffering/FD, input y lifecycle locales,
incluida la protección de FD para música `.m4a` no soportada.
`submodules.lock.json` describe el contenido final esperado; diferencias de CRLF
se normalizan exclusivamente para verificar hashes. El contexto de los parches
se aplica con `--ignore-space-change` por los finales de línea mixtos originales.

En una copia limpia:

```bash
git submodule update --init --recursive
export VITASDK=/usr/local/vitasdk
export PATH="$VITASDK/bin:$PATH"
cmake -S . -B build-session-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-session-debug -j"$(nproc)"
```

Véase `docs/REPRODUCIBLE_BUILD.md` para instalar el SDK exacto y ambos builds.
