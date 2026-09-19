# SizeCheck

**Un CLI pequeño, rápido, seguro y sin conexión para analizar el uso de espacio en disco.**

SizeCheck escanea un directorio (o un único archivo) e informa cuánto espacio
en disco ocupan sus archivos y directorios. No tiene dependencias, nunca
escribe en tus archivos, nunca sigue enlaces simbólicos de directorio hacia
fuera del árbol escaneado, y funciona igual en Linux, macOS y Windows.

**Leer en otros idiomas:** [English](README.md) · [Português (Brasil)](README.pt-BR.md)

---

## Características

- **Rápido** — recorre todo en una sola pasada con listas top-N de memoria
  constante.
- **Seguro** — totalmente de solo lectura. *Nunca* modifica, mueve, enlaza ni
  borra nada; solo lee metadatos.
- **Consciente de enlaces simbólicos** — los enlaces de directorio nunca se
  siguen (sin bucles, sin doble conteo, sin fugas fuera del árbol); los
  enlaces de archivo solo se contabilizan cuando su destino permanece dentro
  del árbol escaneado.
- **Salida legible por máquina** — JSON válido con `--json` para herramientas
  y scripts.
- **Cero dependencias en tiempo de ejecución** — solo la biblioteca estándar
  de C++20. No hay nada que descargar o instalar además del ejecutable que
  compilas.
- **Multiplataforma** — Linux, macOS y Windows.

---

## Requisitos

Para compilar solo necesitas:

- Un compilador con soporte de **C++20** (GCC 11+, Clang 14+, MSVC 2022+,
  Apple Clang).
- **CMake** 3.20 o más reciente.
- Cualquier generador compatible con CMake: Ninja, Make o el generador de
  Visual Studio. No se requiere ninguna biblioteca de terceros.

---

## Compilar e instalar

### Linux / macOS

```bash
git clone https://github.com/Victordebrito2293/SizeCheck.git
cd SizeCheck

cmake -S . -B build          # configura
cmake --build build          # compila (Release por defecto)
./build/sizecheck --help     # ejecuta
```

Para instalarlo en el sistema (por defecto: `/usr/local`):

```bash
cmake --install build
```

### Windows (Visual Studio)

```bat
git clone https://github.com/Victordebrito2293/SizeCheck.git
cd SizeCheck

cmake -S . -B build
cmake --build build --config Release
build\Release\sizecheck.exe --help
```

> El proyecto genera un único ejecutable autocontenido. Una vez compilado,
> puedes copiar `sizecheck` a cualquier lugar de tu máquina (o a otra máquina
> compatible) y ejecutarlo; no se requiere instalación.

---

## Uso

```
sizecheck [OPCIONES] [RUTA]
```

`RUTA` es el directorio o archivo a analizar. Si se omite, se escanea el
directorio actual (`.`).

| Opción                         | Descripción                                                 |
| ------------------------------ | ----------------------------------------------------------- |
| `-h`, `--help`                 | Muestra la ayuda y sale.                                    |
| `-V`, `--version`              | Muestra la versión y sale.                                  |
| `--top N`                      | Muestra los `N` archivos y directorios más grandes (por defecto: 10). |
| `--depth N`                    | Escanea solo `N` niveles de directorio por debajo de `RUTA` (por defecto: ilimitado). `--depth 0` analiza solo el nivel superior. |
| `--exclude NOMBRE`             | Omite cualquier elemento cuyo nombre sea `NOMBRE`. Se puede repetir. |
| `--hidden`                     | Incluye archivos y directorios ocultos (prefijo `.`).       |
| `--json`                       | Emite un informe JSON legible por máquina.                  |
| `--no-color`                   | Desactiva la salida de color.                               |

### Ejemplos

```bash
sizecheck .                          # analiza el directorio actual
sizecheck ./proyecto --top 20        # muestra los 20 elementos más grandes
sizecheck ./proyecto --depth 3       # no desciende más de 3 niveles
sizecheck ./proyecto --exclude node_modules --exclude build
sizecheck ./proyecto --hidden        # incluye archivos ocultos
sizecheck ./proyecto --json > informe.json
sizecheck ~/Descargas/video.iso      # analiza un solo archivo
```

### Códigos de salida

| Código | Significado                                                    |
| ------ | -------------------------------------------------------------- |
| `0`    | El análisis se completó con éxito.                             |
| `1`    | El análisis falló (p. ej. `RUTA` no existe o no es un archivo/directorio regular). |
| `2`    | Uso inválido de la línea de comandos.                          |

---

## Salida

### Informe de texto

```
SizeCheck

Scanning: ./proyecto

Files:        12,842
Directories:  1,284
Total size:   3.82 GB

Largest directories:
1. node_modules/src/  842 MB
2. assets/            512 MB

Largest files:
142 MB  assets/video.mp4
91 MB   backups/db.sql.gz

Scan completed in 1.42s
```

### Informe JSON

```json
{
  "path": "./proyecto",
  "files": 12842,
  "directories": 1284,
  "total_bytes": 4103116800,
  "largest_files": [
    { "path": "assets/video.mp4", "bytes": 148897792 }
  ],
  "largest_directories": [
    { "path": "node_modules", "bytes": 883097600 }
  ],
  "errors": 0,
  "scan_duration_ms": 1420
}
```

> Todas las rutas listadas son relativas a la raíz analizada y nunca exponen
> rutas absolutas de la máquina. Las unidades usan factores binarios
> (1 KB = 1024 B).

---

## Notas de seguridad

- **Solo lectura.** SizeCheck nunca modifica, mueve, enlaza ni borra nada.
- **Archivos ocultos** se excluyen por defecto (como la convención del shell);
  usa `--hidden` para incluirlos.
- **Los enlaces simbólicos de directorio** nunca se siguen. Los enlaces cuyo
  destino es un directorio se omiten por completo — sin bucles, sin doble
  conteo, sin fugas fuera del árbol. Los enlaces rotos o en bucle se informan
  como errores de acceso.
- **Los errores de acceso** (p. ej. archivos que no puedes leer) se informan,
  pero no detienen el análisis.

---

## Ejecutar las pruebas

```bash
cmake -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

La suite incluye pruebas unitarias de argumentos, escaneo, formato e informes,
además de pruebas de extremo a extremo del CLI. Todas se ejecutan en
directorios temporales desechables y nunca tocan datos reales del usuario.

### Opciones de compilación opcionales

| Opción                         | Descripción                                                         |
| ------------------------------ | ------------------------------------------------------------------- |
| `-DSIZECHECK_BUILD_TESTS`      | Compila la suite de pruebas (por defecto: `ON`).                    |
| `-DSIZECHECK_WERROR`           | Trata los avisos del compilador como errores (por defecto: `OFF`).  |
| `-DSIZECHECK_SANITIZE`         | Compila con AddressSanitizer + UndefinedBehaviorSanitizer (por defecto: `OFF`). |
| `-DSIZECHECK_BUILD_FUZZERS`    | Compila los harnesses libFuzzer; requiere Clang + libFuzzer (por defecto: `OFF`). |
| `-DSIZECHECK_ENABLE_CLANG_TIDY` | Ejecuta clang-tidy durante la compilación (por defecto: `OFF`).   |

---

## Licencia

[MIT](LICENSE)