# xbase

A compiler for the xBase language (Clipper 5.x dialect plus extensions) that produces native code through LLVM, and
its runtime.

## Requirements

- CMake 3.20 or newer and a build tool (Ninja recommended)
- A C11 and C++20 compiler: GCC or Clang (the runtime uses GCC/Clang overflow builtins)
- LLVM development files (tested with LLVM 22), built with the targets you want to generate code for

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The build is warning-free on GCC and Clang (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion`); warnings are errors by
default (`-DXB_WARNINGS_AS_ERRORS=OFF` exists for local experiments only).

Part of the runtime is written in xBase and compiled by the `xbc` the build makes, as are the tests and benchmarks,
for the C compiler's target. A build whose C compiler targets another machine through its flags names it, e.g. a
32-bit build with GCC: `-DCMAKE_C_FLAGS=-m32 -DXB_TARGET=i686-pc-linux-gnu`. The `xbc` of the build runs during the
build, so a cross build whose toolchain builds `xbc` for another machine cannot build the runtime yet.

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
themselves, so it has none of the ones that use xbase's own language: classes, bit operators, `YIELD`, tasks,
`ASYNC`). Measured on an Intel Core i9-11900K (8 cores, 16 threads), Linux 7.3, idle: xbase 0.1 Release (`xbc -O2`),
Harbour 3.2.1dev (`hbmk2 -gc3`, its C output compiled with `gcc -O2`), C (`gcc` 16.2 `-O2`), Rust 1.97 (`-C
opt-level=2`), C# (.NET SDK 11.0.100-rc.3, Release, the default concurrent collector), Java (OpenJDK 27), JavaScript
(Node.js 24; `loops` sums in `BigInt`, since its sum passes a double's exact integers), Python (PyPy 8.0, CPython
3.14). xbase is dynamically typed, as Harbour, JavaScript and Python are; C, Rust, C# and Java are statically typed.

| benchmark | xbase | Harbour | C | Rust | C# | Java | JavaScript | Python (PyPy) | Python (CPython) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| asort | 164 | 627 | 158 | 39 | 225 | 253 | 567 | 990 | 850 |
| async_pi | 188 | - | 174 | 181 | 181 | 238 | 221 | 282 | 10752 |
| async | 157 | - | - | - | 156 | - | 123 | 774 | 822 |
| bitlookup | 685 | - | 44 | 35 | 191 | 30 | 43 | 179 | 2611 |
| channels | 1057 | - | - | 557 | 387 | 236 | - | 7000 | 1170 |
| crc32 | 14 | - | 5 | 4 | 12 | 24 | 21 | 39 | 325 |
| fib | 247 | 2039 | 27 | 43 | 90 | 75 | 173 | 221 | 1817 |
| float | 31 | 1071 | 29 | 28 | 30 | 31 | 31 | 35 | 859 |
| gc | 454 | 1209 | 345 | 438 | 3013 | 513 | 495 | 897 | 2837 |
| generator | 87 | - | 4 | 6 | 9 | 17 | 64 | 50 | 166 |
| index | 221 | 511 | - | - | - | - | - | - | - |
| loops | 214 | 11210 | 163 | 69 | 162 | 195 | 9138 | 389 | 21820 |
| matmul | 56 | 1354 | 9 | 14 | 13 | 17 | 31 | 46 | 522 |
| mixed | 65 | 1617 | 38 | 49 | 47 | 53 | 68 | 65 | 2700 |
| oop | 83 | - | 5 | 6 | 4 | 7 | 6 | 7 | 262 |
| pi | 34 | 704 | 6 | 6 | 24 | 32 | 86 | 34 | 662 |
| qsort | 120 | 1333 | 54 | 55 | 110 | 75 | 107 | 135 | 956 |
| sieve | 223 | 2784 | 28 | 42 | 49 | 51 | 71 | 210 | 1214 |
| strings | 421 | 2397 | 177 | 252 | 367 | 347 | 606 | 198 | 2361 |
| tasks | 380 | - | - | - | 132 | 147 | - | 13410 | 2357 |

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

Documentation for contributors starts at [docs/README.md](docs/README.md).
