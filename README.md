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
| module files of the runtime and of the libraries (`xbrt.xbm`, `cli.xbm`), the libraries' archives | `libs/` | `lib/xbase/` or `lib64/xbase/` |
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
it has no `YIELD`, pool of tasks, bounded channel or `ASYNC`), `WRONG` where its check failed, `fail` where it did not
run (Harbour's `linked_list` crashes). Measured on an Intel Core i9-11900K (8 cores, 16 threads), Linux 7.3: xbase 0.1
Release (`xbc -O2`), Harbour 3.2.1dev (`hbmk2 -gc3`, its C output compiled with `gcc -O2`; `-gc2` for the programs whose
generators need integer products past 2^53 exact, which `-gc3` computes in doubles), C (`gcc` 16.2 `-O2`), Rust 1.97
(`-C opt-level=2`), C# (.NET SDK 11.0.100-rc.3, Release, the default concurrent collector, a 4 GB heap), Java (OpenJDK
27), JavaScript (Node.js 24; `loops` sums in `BigInt`, since its sum passes a double's exact integers). Python (PyPy,
CPython) was not measured in this run; Go was not installed. xbase is dynamically typed, as Harbour and JavaScript are;
C, Rust, C# and Java are statically typed. Over the benchmarks both ran, xbase's time is, as a geometric mean, 0.09 of
Harbour's, 1.01 times JavaScript's, 1.62 times C#'s, 1.63 times Java's, 2.74 times Rust's and 3.08 times C's.

| benchmark | xbase | Harbour | C | Rust | C# | Java | JavaScript |
|---|---:|---:|---:|---:|---:|---:|---:|
| ackermann | 165 | 1192 | 18 | 78 | 131 | 69 | 175 |
| aeval_heval | 170 | 1229 | 31 | 22 | 86 | 64 | 236 |
| array_append | 116 | 609 | 14 | 19 | 51 | 230 | 143 |
| array_growth | 153 | 618 | 24 | 28 | 84 | 146 | 324 |
| array_nested | 109 | 1369 | 25 | 37 | 41 | 48 | 67 |
| array_of_hashes | 182 | 270 | 45 | 141 | 193 | 167 | 89 |
| array_preallocate | 162 | 3326 | 36 | 61 | 86 | 145 | 168 |
| asort_codeblock | 69 | 341 | 16 | 11 | 45 | 61 | 57 |
| asort | 158 | 626 | 158 | 40 | 224 | 269 | 557 |
| async_generator | 258 | - | - | - | 144 | - | 270 |
| async_pi | 183 | 11564 | 179 | 179 | 182 | 243 | 218 |
| async | 141 | - | - | - | 153 | - | 121 |
| at_rat_strtran | 212 | 526 | 67 | 189 | 71 | 194 | 150 |
| base64 | 77 | 939 | 5 | 7 | 23 | 75 | 123 |
| bfs_dfs | 51 | 541 | 22 | 26 | 29 | 47 | 50 |
| binary_search | 179 | 1944 | 108 | 116 | 115 | 117 | 241 |
| binary_trees | 61 | 361 | 45 | 61 | 33 | 24 | 35 |
| bitlookup | 286 | 3664 | 44 | 37 | 187 | 30 | 44 |
| black_scholes | 69 | 688 | 23 | 17 | 24 | 34 | 45 |
| chameneos_redux | 215 | 326 | 154 | 155 | 88 | 206 | 431 |
| channels | 435 | - | 365 | 560 | 378 | 232 | 210 |
| closure_capture | 96 | 392 | 14 | 16 | 59 | 45 | 71 |
| codeblock_callback | 140 | 2046 | 24 | 24 | 34 | 40 | 121 |
| codeblock_eval | 182 | 1367 | 26 | 27 | 29 | 25 | 20 |
| counting_sort | 117 | 2048 | 58 | 54 | 54 | 83 | 114 |
| crc32 | 10 | 455 | 5 | 4 | 13 | 24 | 23 |
| crc_adler_fnv | 121 | 11631 | 84 | 79 | 104 | 147 | 324 |
| csv_parse | 205 | 1390 | 96 | 75 | 232 | 122 | 185 |
| date_time_ops | 123 | 222 | 129 | 102 | 141 | 485 | 298 |
| dgemm | 139 | 3009 | 10 | 16 | 28 | 38 | 52 |
| dhrystone | 170 | 1732 | 16 | 14 | 62 | 22 | 36 |
| dijkstra | 72 | 1008 | 33 | 31 | 43 | 66 | 64 |
| fannkuch_redux | 154 | 2836 | 45 | 57 | 56 | 78 | 68 |
| fasta | 249 | 2814 | 126 | 147 | 176 | 216 | 974 |
| fft | 124 | 4395 | 32 | 35 | 37 | 59 | 47 |
| fib | 87 | 2010 | 25 | 43 | 95 | 75 | 172 |
| float | 29 | 1092 | 29 | 29 | 30 | 31 | 32 |
| gaussian_elimination | 147 | 3101 | 21 | 38 | 29 | 59 | 301 |
| gc | 422 | 1198 | 323 | 429 | 2992 | 497 | 484 |
| generator | 2 | - | 7 | 4 | 9 | 17 | 65 |
| geracao_html | 97 | 112 | 64 | 47 | 114 | 141 | 145 |
| hash_chaves_longas | 112 | 2967 | 123 | 108 | 83 | 87 | 366 |
| hash_massivo | 205 | 14824 | 99 | 72 | 178 | 182 | 199 |
| hash_resize | 168 | 134848 | 177 | 124 | 125 | 104 | 171 |
| heap_sort | 136 | 2100 | 69 | 69 | 83 | 78 | 105 |
| higher_order_chain | 107 | - | 24 | 25 | 138 | 60 | 139 |
| indexacao_simulada | 167 | 626 | 78 | 136 | 125 | 159 | 161 |
| index | 148 | 517 | - | 221 | 1189 | 315 | 3146 |
| inheritance_depth | 86 | 3444 | 24 | 33 | 6 | 10 | 19 |
| ini_parse | 145 | 516 | 98 | 106 | 242 | 212 | 129 |
| jaro_winkler | 238 | 2173 | 53 | 47 | 140 | 85 | 160 |
| join_arrays | 95 | 437 | 56 | 29 | 95 | 81 | 264 |
| json_roundtrip | 134 | 465 | 80 | 103 | 192 | 134 | 207 |
| julia_set | 193 | 6406 | 186 | 189 | 184 | 185 | 193 |
| knapsack | 104 | 1135 | 7 | 18 | 16 | 20 | 22 |
| k_nucleotide | 174 | 2057 | 25 | 64 | 149 | 166 | 223 |
| levenshtein | 151 | 3116 | 20 | 25 | 83 | 47 | 171 |
| linked_list | 259 | fail | 67 | 86 | 174 | 92 | 135 |
| linpack | 154 | 2819 | 17 | 11 | 56 | 31 | 56 |
| log_processing | 122 | 340 | 55 | 64 | 163 | 85 | 155 |
| longest_common_subsequence | 178 | 3035 | 23 | 24 | 74 | 41 | 114 |
| long_short_mix | 294 | 561 | 52 | 88 | 352 | 157 | 215 |
| loops | 214 | 11042 | 162 | 68 | 162 | 195 | 9111 |
| lru_cache | 219 | 5490 | 77 | 70 | 97 | 96 | 277 |
| mandelbrot | 152 | 5959 | 150 | 149 | 148 | 151 | 151 |
| map_filter_reduce | 160 | 1194 | 22 | 13 | 49 | 66 | 141 |
| matmul | 56 | 1346 | 11 | 14 | 12 | 18 | 31 |
| md5_sha1 | 156 | 12044 | 54 | 38 | 62 | 115 | 223 |
| memory_footprint | 190 | - | - | 92 | 248 | 314 | 150 |
| merge_sort | 152 | 2302 | 59 | 61 | 169 | 104 | 140 |
| meteor_contest | 233 | 2578 | 60 | 73 | 220 | 105 | 184 |
| method_call_overhead | 180 | 2452 | 7 | 19 | 6 | 17 | 13 |
| mixed | 46 | 1714 | 38 | 49 | 47 | 54 | 65 |
| monte_carlo_pi | 168 | 7527 | 110 | 84 | 197 | 116 | 200 |
| n_body | 138 | 2520 | 23 | 25 | 26 | 35 | 38 |
| n_queens | 192 | 1804 | 94 | 110 | 224 | 116 | 157 |
| numerical_integration | 65 | 1325 | 29 | 38 | 38 | 46 | 90 |
| numeric_formatting | 89 | 138 | 111 | 82 | 402 | 492 | 301 |
| object_churn | 155 | 523 | 53 | 81 | 155 | 157 | 87 |
| object_creation | 106 | 232 | 21 | 25 | 26 | 127 | 21 |
| oop | 10 | 674 | 8 | 6 | 4 | 7 | 6 |
| parallel_map | 93 | 700 | 17 | 13 | 42 | 55 | 167 |
| pidigits | 142 | 5038 | 31 | 29 | 303 | 57 | 165 |
| pipeline_tasks | 102 | - | 95 | 51 | 96 | - | 152 |
| pi | 35 | 702 | 5 | 7 | 24 | 32 | 86 |
| pivot_crosstab | 99 | 444 | 46 | 43 | 38 | 58 | 155 |
| prime_sieve_atkin | 229 | 4630 | 34 | 50 | 69 | 85 | 162 |
| producer_consumer | 194 | 766 | 62 | 99 | 56 | 73 | 246 |
| property_access | 103 | 1470 | 18 | 27 | 6 | 13 | 12 |
| qsort | 114 | 1257 | 54 | 56 | 111 | 75 | 106 |
| radix_sort | 52 | 3002 | 25 | 22 | 29 | 56 | 59 |
| random_massivo | 166 | 26726 | 85 | 80 | 100 | 135 | 2684 |
| ray_tracer | 157 | 1307 | 10 | 10 | 51 | 34 | 40 |
| relatorio_agregacao | 78 | 432 | 19 | 25 | 44 | 50 | 79 |
| reverse_complement | 186 | 4532 | 36 | 72 | 130 | 185 | 2070 |
| sequential_scan | 200 | 1193 | 59 | 28 | 118 | 79 | 195 |
| set_operations | 183 | 11802 | 90 | 91 | 103 | 188 | 166 |
| shared_counter | 381 | - | 64 | 62 | 55 | 69 | 149 |
| short_lived_objects | 147 | 2036 | 30 | 36 | 25 | 20 | 16 |
| sieve | 71 | 2783 | 27 | 42 | 49 | 51 | 70 |
| spectral_norm | 188 | 6350 | 53 | 289 | 158 | 62 | 82 |
| string_builder | 76 | 223 | 58 | 94 | 113 | 65 | 287 |
| string_concatenation | 164 | 812 | 99 | 147 | 136 | 81 | 259 |
| strings | 364 | 2386 | 175 | 249 | 362 | 334 | 605 |
| substr_massivo | 87 | 887 | 21 | 50 | 49 | 81 | 89 |
| sudoku_solver | 238 | 1880 | 31 | 34 | 134 | 40 | 67 |
| tak | 53 | 849 | 25 | 25 | 37 | 23 | 55 |
| tasks | 299 | - | 433 | - | 131 | 150 | 229 |
| tokenize_split | 156 | 424 | 75 | 46 | 206 | 109 | 108 |
| towers_of_hanoi | 67 | 1319 | 9 | 14 | 37 | 19 | 41 |
| trie | 111 | 270 | 20 | 32 | 99 | 45 | 82 |
| upper_lower_alltrim | 104 | 460 | 69 | 54 | 134 | 225 | 151 |
| url_encode | 120 | 1200 | 51 | 93 | 125 | 99 | 183 |
| validacao_dados | 121 | 1054 | 46 | 65 | 122 | 135 | 227 |
| virtual_dispatch | 90 | 345 | 9 | 7 | 11 | 18 | 38 |
| whetstone | 297 | 1191 | 35 | 27 | 62 | 55 | 53 |
| word_count | 166 | 511 | 49 | 43 | 104 | 120 | 144 |
| work_stealing | 90 | - | 26 | - | 89 | 42 | 180 |
| xml_simples | 122 | 184 | 52 | 81 | 87 | 138 | 125 |

## Use

```sh
build/compiler/xbc hello.prg -o hello.o        # compile to a native object
cc hello.o build/runtime/libxbrt.a -lm -o hello  # link with the runtime
./hello
```

A program of several sources compiles in one run into one object (`xbc *.prg -o app.o`); calls between its modules
need no declaration, and it starts at `Main()`.

A project file builds a program, or a library, in one step: `xbc` compiles its sources and links the program with the
runtime and the libraries it names, through the system's C compiler (`link.exe` from an MSVC developer prompt):

```sh
build/compiler/xbc samples/hello/hello.xbprj --out-dir=out     # out/hello
build/compiler/xbc libs/cli/cli.xbprj --out-dir=out/cli          # out/cli/libcli.a and cli.xbm
build/compiler/xbc samples/wordfreq/wordfreq.xbprj --out-dir=out -Lout
out/wordfreq -n 5 samples/wordfreq/sample.txt
```

The build compiles the libraries of `libs/` into `build/libs/` (`-Lbuild/libs`), and an install puts them where its
`xbc` finds them by itself. [docs/projects.md](docs/projects.md) describes the project files, and `--link`,
`--library=<name>`, `--lib=<name>`, `-L<dir>` and `-l<name>`, which do the same from the command line.

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
