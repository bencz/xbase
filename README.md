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
