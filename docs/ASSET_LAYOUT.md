# Datos que solicita Zombie Shooter

`log_0008.log` registra 3.796 llamadas a `AAssetManager_open`. De las rutas directas (sin la búsqueda preliminar opcional en `i18n/`), 1.256 rutas distintas fallaron en Vita. **790 de ellas están en el `data/assets` local**, por ejemplo `vid/115.vid`, `vid/empty.vid` y recursos de `menus/img/`.

La prueba posterior `log_0009.log`, con los datos copiados, demuestra que `vid/empty.vid` abre y se lee antes de fallar en aperturas posteriores. En el primer fallo de `vid/115.vid` hay 59 `AAsset` abiertos (50 `.vid`); las aperturas de assets válidos dejan de funcionar a partir de ahí. El checkpoint siguiente mantuvo los `.vid` en memoria y liberó sus `FILE*`; la falta de copia no explica el patrón observado.

`log_0010.log` verificó ese buffer en Vita: `vid/115.vid` y `menus/main.men` abren y aparecen imágenes. Más adelante hay 58 streams activos no `.vid`, y fallan imágenes PNG del menú que ya se habían abierto. El siguiente checkpoint incluye en el buffer todos los assets de hasta 256 KiB, además de los `.vid`.

Las 466 restantes aún faltan localmente. **396 figuran en `assets/bundles.config` bajo `[commonAssets;fast-follow]`**. En total esa sección declara 495 archivos y ninguno está en `data/assets` ni en los APK del XAPK examinado. El juego Android puede haberlos obtenido al ejecutarse, pero el origen todavía no está probado. Las otras 70 rutas son principalmente pruebas de extensiones `.bmp` o variantes del menú; por ejemplo `menus/main.android.men` falla y `menus/main.men` existe. No se atribuye todo el bloqueo a esas 70 rutas.

`data/assets` coincide con los 2.432 archivos `assets/` del APK base: ningún nombre ni CRC difiere. `config.es.apk` sólo contiene `resources.arsc`, manifiesto y firmas; no añade assets nativos. `data/res` contiene recursos Android de la interfaz Java, sin lecturas observadas desde el loader nativo. La estructura requerida por el loader en Vita es:

```text
ux0:data/zombieshooter/libzombie_shooter.so
ux0:data/zombieshooter/assets/game.res
ux0:data/zombieshooter/assets/bundles.config
ux0:data/zombieshooter/assets/vid/...
ux0:data/zombieshooter/assets/menus/...
```

Para preparar una copia completa desde la raíz del proyecto:

```bash
python3 scripts/package_vita_data.py
```

El archivo `build-session-debug/vita-data.zip` contiene `zombieshooter/` para extraer **dentro de `ux0:data/`**. El ZIP de datos no va dentro del repositorio ni del VPK. En la siguiente prueba basta reemplazar el VPK Debug: los datos ya copiados sirven. Hay que confirmar en el log que `menus/img/2555_07.png` y `menus/img/2544.png` abren; luego se delimitará el efecto de los recursos de `commonAssets` que siguen ausentes.
