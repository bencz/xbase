# xbase

A compiler for the xBase language (Clipper 5.x dialect plus extensions) that produces native code through LLVM, and
its runtime.

## Requirements

- CMake 3.20 or newer and a build tool (Ninja recommended)
- A C11 and C++20 compiler: GCC or Clang (the runtime uses GCC/Clang overflow builtins)
- LLVM development files (tested with LLVM 23), built with the targets you want to generate code for

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The build is warning-free on GCC and Clang (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion`); warnings are errors by
default (`-DXB_WARNINGS_AS_ERRORS=OFF` exists for local experiments only).

Part of the runtime is written in xBase and compiled by the `xbc` the build makes, as are the tests and benchmarks,
for the C compiler's target. A build whose runtime targets another machine through C compiler flags gives them, for
the runtime and the programs only (`xbc` runs on the host), and names the target, e.g. a 32-bit build with GCC:
`-DXB_TARGET_C_FLAGS=-m32 -DXB_TARGET=i686-pc-linux-gnu`. The `xbc` of the build runs during the build, so a cross
build whose toolchain builds `xbc` for another machine cannot build the runtime yet.

## Packages

`cmake --install build --prefix <dir>` installs `xbc`, the standard headers (`std.ch`, `fileio.ch`, ...) with the
runtime's C headers (`xbrt.h`), the runtime library and the documentation. On Windows it makes the package a GitHub
release carries (built with MSVC and with MinGW-w64 by `.github/workflows/release.yml`); elsewhere it follows the
system's places (with the default prefix, `/usr/local`):

| | Windows package (`--prefix <dir>/xbc`) | Linux, macOS, other systems |
|---|---|---|
| `xbc` | `bin/` | `bin/` |
| headers | `include/` | `include/xbase/` |
| runtime library | `libs/` (`xbrt.lib` from MSVC, `libxbrt.a` from MinGW) | `lib/` or `lib64/` (`libxbrt.a`) |
| documentation | `docs/`, with `README.md` and `LICENSE` at the top | `share/doc/xbase/` |

`xbc` finds the standard headers by the path from its own folder to where the install put them: a program needs no `-I`
for them (on Linux, `cc hello.o -lxbrt -lm -lpthread` links a program, the library being in `/usr/local/lib` or
`lib64`). A package is built with `-DXB_SOURCE_INCLUDE=OFF` (its `xbc` then holds no path to the source tree it came
from) and, where LLVM has a shared library, `-DXB_LLVM_STATIC=ON` (its `xbc` needs no LLVM DLL). With the MSVC package,
from a Visual Studio command prompt:

```bat
xbc\bin\xbc hello.prg -o hello.obj
link hello.obj xbc\libs\xbrt.lib shell32.lib /out:hello.exe
```

With the MinGW-w64 package:

```sh
xbc/bin/xbc hello.prg -o hello.o
gcc hello.o xbc/libs/libxbrt.a -lshell32 -o hello.exe
```

`tests/package` builds a program that way with CMake (`-DXBASE_ROOT=<dir>/xbc`), as the release workflow does on
each package before it attaches it (and the tests do on an install into the build folder). Each package's `xbc`
cross-compiles to the targets its LLVM was built with: the MSVC one to those of the LLVM project's Windows release
(x86, ARM, AArch64, RISC-V, WebAssembly, BPF, NVPTX), the MinGW one to all of MSYS2's (PowerPC, MIPS, SystemZ, SPARC,
LoongArch, ... too); `xbc --print-targets` lists them.

## Test

```sh
ctest --test-dir build --output-on-failure
```

## Benchmarks

```sh
cmake --build build --target bench
```

Runs the programs in `bench/` (compiled with `-O2`) and prints the best time of each; measure Release builds only.

### Compared with other languages

`bench/` also holds each benchmark in C, Rust, C#, Java, JavaScript and Python (the same algorithm, sizes and checks;
see [docs/testing.md](docs/testing.md#benchmarks), which lists where a port differs). This builds every one with each
compiler or interpreter found and prints the table below (best of 3 runs; CPython once):

```sh
bench/compare.sh                    # -l xbase,c,... picks languages, -n the runs (bash 4, GNU timeout)
pwsh bench/compare.ps1              # -Langs xbase,c,... and -Runs on any system with PowerShell 7.2
```

Milliseconds, lower is better; `-` where a language has no version of that benchmark (Harbour compiles the `.prg`
themselves, or its own forms in `bench/harbour/` where it has functions or threads for xbase's bit operators and tasks;
it has no `YIELD`, pool of tasks, bounded channel or `ASYNC`). Measured on an Intel Core i9-11900K (8 cores, 16
threads), Linux 7.3, idle: xbase 0.1 Release (`xbc -O2`), Harbour 3.2.1dev (`hbmk2 -gc3`, its C output compiled with
`gcc -O2`), C (`gcc` 16.2 `-O2`), Rust 1.97 (`-C opt-level=2`), C# (.NET SDK 11.0.100-rc.3, Release, the default
concurrent collector), Java (OpenJDK 27), JavaScript (Node.js 24; `loops` sums in `BigInt`, since its sum passes a
double's exact integers), Python (PyPy 8.0, CPython 3.14). xbase is dynamically typed, as Harbour, JavaScript and Python
are; C, Rust, C# and Java are statically typed. CPython's `loops` was not measured: minutes of one core at full
turbo froze that machine each time (a hardware fault, not the program's).

| benchmark | xbase | Harbour | C | Rust | C# | Java | JavaScript | Python (PyPy) | Python (CPython) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| asort | 169 | 650 | 162 | 40 | 228 | 269 | 575 | 998 | 864 |
| async_generator | 364 | - | - | - | 143 | - | 269 | 55 | 319 |
| async_pi | 185 | 12526 | 177 | 184 | 186 | 245 | 224 | 287 | 12116 |
| async | 149 | - | - | - | 154 | - | 122 | 790 | 832 |
| bitlookup | 317 | 3840 | 45 | 35 | 200 | 31 | 46 | 183 | 2664 |
| channels | 435 | - | - | 552 | 371 | 216 | - | 7205 | 1208 |
| crc32 | 13 | 458 | 4 | 4 | 12 | 24 | 23 | 42 | 337 |
| fib | 169 | 2117 | 27 | 44 | 96 | 77 | 178 | 223 | 1940 |
| float | 30 | 1130 | 31 | 30 | 32 | 32 | 34 | 38 | 926 |
| gc | 484 | 1285 | 360 | 487 | 3154 | 418 | 498 | 934 | 2911 |
| generator | 61 | - | 8 | 4 | 10 | 17 | 70 | 51 | 172 |
| index | 231 | 531 | - | - | - | - | - | - | - |
| loops | 222 | 11707 | 163 | 69 | 166 | 202 | 10311 | 394 | - |
| matmul | 57 | 1421 | 9 | 15 | 13 | 18 | 32 | 49 | 581 |
| mixed | 50 | 1692 | 38 | 50 | 48 | 57 | 69 | 66 | 2817 |
| oop | 12 | 708 | 5 | 6 | 4 | 7 | 6 | 7 | 271 |
| pi | 35 | 725 | 6 | 9 | 24 | 32 | 81 | 35 | 689 |
| qsort | 119 | 1380 | 57 | 57 | 114 | 76 | 110 | 143 | 1052 |
| sieve | 206 | 2898 | 28 | 42 | 51 | 53 | 72 | 221 | 1288 |
| strings | 436 | 2472 | 181 | 259 | 376 | 358 | 636 | 202 | 2470 |
| tasks | 301 | - | - | - | 129 | 157 | - | 13670 | 2384 |

## Use

```sh
build/compiler/xbc hello.prg -o hello.o        # compile to a native object
cc hello.o build/runtime/libxbrt.a -lm -o hello  # link with the runtime
./hello
```

A program of several sources compiles in one run into one object (`xbc *.prg -o app.o`); calls between its modules
need no declaration, and it starts at `Main()`.

Useful options (`xbc --help` lists all of them):

| Option | Meaning |
|---|---|
| `-E` | print the preprocessed source and stop |
| `-p` | also write each preprocessed source to `<file>.ppo` |
| `-I<dir>`, `-D<name>[=<value>]` | include directory, `#define` (the target defines a few names itself: [preprocessor.md](docs/preprocessor.md#predefined-names)) |
| `-u`, `-u<file>` | compile without `std.ch`, or with another standard header |
| `--target=<triple>` | cross-compile, e.g. `--target=s390x-linux-gnu` |
| `--print-targets` | list the targets this build of `xbc` supports |
| `--depfile=<file>` | write a Make-style dependency file (sources and includes read) |
| `-O0` .. `-O3` | optimization level (default `-O2`) |
| `--cpu=<name>` | target CPU; default `generic` (runs on any CPU of the target), `native` for this machine only |
| `--dump-xir=<file>`, `--dump-ir=<file>` | debugging: write the module's xIR or LLVM IR as text |
| `--dump-xir-types` | with `--dump-xir`: each item value and block parameter followed by its inferred types |

Documentation for contributors starts at [docs/README.md](docs/README.md).
