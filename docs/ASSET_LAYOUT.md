# Datos que solicita Zombie Shooter

`log_0008.log` registra 3.796 llamadas a `AAssetManager_open`. De las rutas directas (sin la búsqueda preliminar opcional en `i18n/`), 1.256 rutas distintas fallaron en Vita. **790 de ellas ya están en el `data/assets` local actual**, por ejemplo `vid/115.vid`, `vid/empty.vid` y recursos de `menus/img/`. Hay que copiar el árbol actualizado a `ux0:data/zombieshooter/assets/` para comprobar su efecto; dejarlo sólo en el PC no cambia la instalación de Vita.

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

El archivo `build-session-debug/vita-data.zip` contiene `zombieshooter/` para extraer **dentro de `ux0:data/`**. El ZIP de datos no va dentro del repositorio ni del VPK. Sobrescribe los archivos anteriores en Vita y conserva los datos de guardado que estén fuera de `assets/`. La siguiente prueba debe confirmar en el log que `vid/115.vid` y `vid/empty.vid` ya abren; luego delimitará el efecto de los 396 recursos de `commonAssets` que siguen ausentes.
