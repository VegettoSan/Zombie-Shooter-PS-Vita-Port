# Subida manual del código

`.gitignore` excluye compilaciones, VPK, APK/XAPK, bibliotecas Android, assets extraídos, logs, dumps y archivos de análisis local. Conserva el código, CMake, la documentación y `patches/`.

Para subir sólo el trabajo del port, selecciona estos archivos del repositorio principal:

```text
.gitignore
CMakeLists.txt
PORTING_PLAN.md
PORT_STATUS.md
port_progress.md
docs/MANUAL_UPLOAD.md
docs/ASSET_LAYOUT.md
patches/
scripts/package_vita_data.py
source/dynlib.c
source/java.c
source/main.c
source/patch.c
source/reimpl/bionic_compat.c
source/reimpl/bionic_compat.h
source/reimpl/egl.c
source/reimpl/pthr.c
source/reimpl/pthr.h
source/reimpl/sys.c
source/reimpl/sys.h
source/utils/glutil.c
source/utils/glutil.h
source/utils/init.c
source/utils/logger.c
source/utils/so_trace.h
```

`lib/falso_jni` y `lib/falso_ndk` son submódulos. Sus cambios locales se entregan como `patches/falso_jni.patch` y `patches/falso_ndk.patch`; sigue `patches/README.md` para aplicarlos tras clonar. Subir sólo el enlace de submódulo del repositorio principal no sube esos cambios.

`.gitignore` no oculta archivos que Git ya rastrea. Si aparecen otros archivos modificados por finales de línea, selecciónalos sólo si tienen cambios de contenido intencionales. Los VPK y los datos originales del juego deben quedarse fuera de la subida de código.
