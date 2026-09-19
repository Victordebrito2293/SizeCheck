# SizeCheck

**A small, fast, safe, offline CLI that analyzes disk usage.**

SizeCheck scans a directory (or a single file) and reports how much disk space
its files and directories use. It is dependency-free, never writes to your
files, never follows directory symlinks outside the scanned tree, and works
identically on Linux, macOS and Windows.

**Read this in other languages:** [Português (Brasil)](README.pt-BR.md) · [Español](README.es.md)

---

## Features

- **Fast** — a single-pass iterative traversal with constant memory top-N lists.
- **Safe** — purely read-only. It *never* modifies, moves, links, or deletes
  anything; it only reads metadata.
- **Symlink-aware** — directory symlinks are never followed (no loops, no
  double counting, no escapes); file symlinks are only counted when their
  target stays inside the scanned tree.
- **Machine-readable output** — valid `--json` for tools and scripting.
- **Zero runtime dependencies** — C++20 standard library only. Nothing to
  download or install besides the executable you build.
- **Cross-platform** — Linux, macOS, and Windows.

---

## Requirements

To build you only need:

- A **C++20** compiler (GCC 11+, Clang 14+, MSVC 2022+, Apple Clang).
- **CMake** 3.20 or newer.
- Any build tool supported by CMake: Ninja, Make, or the Visual Studio
  generator. No third-party libraries are required.

---

## Build & install

### Linux / macOS

```bash
git clone https://github.com/Victordebrito2293/SizeCheck.git
cd SizeCheck

cmake -S . -B build          # configure
cmake --build build          # build (Release by default)
./build/sizecheck --help     # run
```

To install system-wide (defaults to `/usr/local`):

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

> The project is a single self-contained executable. Once built, you can copy
> `sizecheck` anywhere on your machine (or to any compatible machine) and run
> it — no installation required.

---

## Usage

```
sizecheck [OPTIONS] [PATH]
```

`PATH` is the directory or file to analyze. When omitted, the current
directory (`.`) is scanned.

| Option                         | Description                                               |
| ------------------------------ | --------------------------------------------------------- |
| `-h`, `--help`                 | Show help and exit.                                       |
| `-V`, `--version`              | Show version and exit.                                    |
| `--top N`                      | Show the `N` largest files and directories (default: 10). |
| `--depth N`                    | Traverse only `N` directory levels below `PATH` (default: unlimited). `--depth 0` scans only the top level. |
| `--exclude NAME`               | Skip any entry whose name is `NAME`. Can be repeated.     |
| `--hidden`                     | Include hidden (dot-prefixed) files and directories.      |
| `--json`                       | Emit a machine-readable JSON report.                      |
| `--no-color`                   | Disable colored output.                                   |

### Examples

```bash
sizecheck .                          # analyze the current directory
sizecheck ./project --top 20         # show the 20 largest entries
sizecheck ./project --depth 3        # do not descend deeper than 3 levels
sizecheck ./project --exclude node_modules --exclude build
sizecheck ./project --hidden         # include hidden files
sizecheck ./project --json > report.json
sizecheck ~/Downloads/big.iso        # analyze a single file
```

### Exit codes

| Code | Meaning                                          |
| ---- | ------------------------------------------------ |
| `0`  | Scan completed successfully.                     |
| `1`  | The scan failed (e.g. `PATH` does not exist or is not a regular file/directory). |
| `2`  | Invalid command line usage.                      |

---

## Output

### Text report

```
SizeCheck

Scanning: ./project

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

### JSON report

```json
{
  "path": "./project",
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

> All entry paths are relative to the scanned root and never expose absolute
> machine paths. Sizes use binary units (1 KB = 1024 B).

---

## Safety notes

- **Read-only.** SizeCheck never modifies, moves, links, or deletes anything.
- **Hidden files** are excluded by default (like the shell convention);
  use `--hidden` to include them.
- **Directory symlinks** are never followed. Symlinks whose target is a
  directory are skipped entirely — no loops, no double counting, no escapes
  outside the tree. Broken or looping symlinks are reported as access errors.
- **Access errors** (e.g. files you cannot read) are reported but do not stop
  the scan.

---

## Running the tests

```bash
cmake -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

The test suite includes unit tests for parsing, scanning, formatting and
reporting, plus end-to-end CLI tests. All tests run in throwaway temporary
directories and never touch real user data.

### Optional build options

| Option                    | Description                                             |
| ------------------------- | ------------------------------------------------------- |
| `-DSIZECHECK_BUILD_TESTS` | Build the test suite (default: `ON`).                   |
| `-DSIZECHECK_WERROR`      | Treat compiler warnings as errors (default: `OFF`).     |
| `-DSIZECHECK_SANITIZE`    | Build with AddressSanitizer + UndefinedBehaviorSanitizer (default: `OFF`). |
| `-DSIZECHECK_BUILD_FUZZERS` | Build libFuzzer harnesses, requires Clang + libFuzzer (default: `OFF`). |
| `-DSIZECHECK_ENABLE_CLANG_TIDY` | Run clang-tidy as part of the build (default: `OFF`). |

---

## License

[MIT](LICENSE)