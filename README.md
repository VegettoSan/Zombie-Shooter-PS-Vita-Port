# Zombie Shooter - PS Vita Port (so_loader)

Plantilla lista basada en **soloader-boilerplate** de v-atamanenko para portar **Zombie Shooter** (versión Android de Sigma Team) a PS Vita.

## ¿Qué es esto?

Este repositorio es una base lista para empezar el port. Usa **so_loader** + **vitaGL** + **FalsoJNI**.

El juego usa:
- `libzombie_shooter.so` (ARMv7)
- OpenGL ES 2.0 (`libGLESv2` + `libEGL`)
- OpenSL ES para audio
- Motor nativo propio de Sigma Team (no es Unity)

## Requisitos

### En el PC (WSL / Ubuntu)
- VitaSDK (softfp) instalado y variable `VITASDK` configurada
- CMake, git, build-essential

### En la PS Vita
- HENkaku / Enso
- **kubridge.skprx** instalado (obligatorio)
- VitaShell

## Cómo usarlo (pasos claros)

### 1. Clonar el repositorio con submódulos

```bash
cd ~
git clone --recurse-submodules https://github.com/VegettoSan/soloader-boilerplate.git ZombieShooter-Vita
cd ZombieShooter-Vita
```

Si ya lo clonaste sin submódulos:

```bash
git submodule update --init --recursive
```

### 2. Poner los archivos del juego

Crea la carpeta de datos (después de compilar el VPK se usará `ux0:data/zombieshooter/`):

En el PC, prepara:

```
libzombie_shooter.so     ← el .so principal
assets/                  ← carpeta assets extraída del APK principal
```

Más adelante los copiarás a la Vita en:

```
ux0:data/zombieshooter/libzombie_shooter.so
ux0:data/zombieshooter/assets/   (o la estructura que use el juego)
```

### 3. Configurar el proyecto

Abre `CMakeLists.txt` y revisa/ajusta estas líneas al principio:

```cmake
set(VITA_APP_NAME "Zombie Shooter")
set(VITA_TITLEID  "ZOMB00001")
set(VITA_VPKNAME  "zombie_shooter")

set(DATA_PATH "ux0:data/zombieshooter/" CACHE STRING "Path to data (with trailing /)")
set(SO_PATH "${DATA_PATH}libzombie_shooter.so" CACHE STRING "Path to .so")
```

### 4. Compilar

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

El VPK saldrá en `build/`.

### 5. Instalar en la Vita

1. Copia el `.vpk` a la Vita e instálalo con VitaShell.
2. Crea la carpeta `ux0:data/zombieshooter/`
3. Copia `libzombie_shooter.so` y los assets ahí.
4. Asegúrate de tener **kubridge** en `*KERNEL` del `config.txt`.

## Archivos importantes a editar

| Archivo              | Para qué sirve                                      |
|----------------------|-----------------------------------------------------|
| `source/dynlib.c`    | Resolver símbolos que faltan (GLES, OpenSL, etc.)  |
| `source/java.c`      | Implementar llamadas JNI que use el juego           |
| `source/patch.c`     | Parches específicos (DRM, crashes, etc.)          |
| `source/main.c`      | Loop principal y controles                          |
| `CMakeLists.txt`     | Nombre, TitleID, rutas                              |

## Notas del análisis del juego

- Librería principal: `libzombie_shooter.so` (~9.3 MB)
- Usa GLES 2.0 → perfecto para vitaGL
- Usa OpenSL ES → hay que stubear o reimplementar audio
- No es Unity ni GameMaker
- Versión recomendada del APK: la que tenía soporte mínimo Android 7 (3.5.3)

## Créditos

- Boilerplate original: [v-atamanenko/soloader-boilerplate](https://github.com/v-atamanenko/soloader-boilerplate)
- so_util / TheFloW
- vitaGL / Rinnegatamante
- kubridge

## Licencia

MIT (igual que el boilerplate original)
