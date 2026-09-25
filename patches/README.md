# Parches de submódulos para una subida manual

`lib/falso_jni` y `lib/falso_ndk` son submódulos Git. El repositorio principal sólo guarda sus revisiones, así que los cambios locales dentro de ellos no entran en una subida del repositorio principal aunque `.gitignore` esté correcto.

Incluye `falso_jni.patch` y `falso_ndk.patch` en la subida. Los parches contienen únicamente cambios de código reales; se omitieron diferencias de finales de línea. En una copia nueva con los submódulos en las revisiones fijadas por el proyecto, aplícalos desde la raíz:

```bash
git -C lib/falso_jni apply ../../patches/falso_jni.patch
git -C lib/falso_ndk apply ../../patches/falso_ndk.patch
```

Los parches se comprobaron contra el estado local con `git apply --reverse --check`. No contienen APK, XAPK, `.so`, assets, VPK, logs ni volcados de Vita.
