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
runtime's C headers (`xbrt.h`), the runtime library, the libraries and the documentation. On Windows it makes the
package a GitHub release carries (`.github/workflows/release.yml`); elsewhere it follows the system's places (with the
default prefix, `/usr/local`):

| | Windows package (`--prefix <dir>/xbc`) | Linux, macOS, other systems |
|---|---|---|
| `xbc` | `bin/` | `bin/` |
| headers | `include/` | `include/xbase/` |
| runtime library | `libs/` (`libxbrt.a`) | `lib/` or `lib64/` (`libxbrt.a`) |
| module files of the runtime and of the libraries (`xbrt.xbm`, `cli.xbm`), the libraries' archives | `libs/` | `lib/xbase/` or `lib64/xbase/` |
| the toolchain programs link with (`-DXB_BUNDLE_TOOLCHAIN`) | `toolchain/` | `lib/xbase/toolchain/` or `lib64/xbase/toolchain/` |
| documentation | `docs/`, with `README.md` and `LICENSE` at the top | `share/doc/xbase/` |

`xbc` finds the standard headers and the module files by the path from its own folder to where the install put them: a
program needs no `-I` and no `-L` for them. A package is built with `-DXB_SOURCE_INCLUDE=OFF` (its `xbc` then holds no
path to the source tree it came from) and, where LLVM has a shared library, `-DXB_LLVM_STATIC=ON` (its `xbc` needs no
LLVM DLL). The runtime's module file names the C compiler programs link with: by default the name of the one that built
the runtime, found on the path; `-DXB_BUNDLE_TOOLCHAIN=<folder>` installs the toolchain whose C compiler built the
runtime with it instead, and names that compiler by its path in the install. The Windows package does so with
[llvm-mingw](https://github.com/mstorsjo/llvm-mingw) (clang, lld, MinGW-w64's C runtime for UCRT; `xbc` itself is built
with MSYS2's MinGW-w64 GCC), so that on a machine with nothing else installed:

```bat
xbc\bin\xbc hello.prg
hello.exe
```

`tests/package` builds programs from an install with CMake (`-DXBASE_ROOT=<dir>/xbc`), as the release workflow does on
the package before it attaches it (and the tests do on an install into the build folder), and the workflow also builds
and runs one with no compiler on the path. The package's `xbc` cross-compiles to the targets MSYS2's LLVM was built with
(x86, ARM, AArch64, RISC-V, PowerPC, MIPS, SystemZ, SPARC, LoongArch, ...; `xbc --print-targets` lists them).

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
Harbour's, 0.98 times JavaScript's, 1.56 times C#'s, 1.59 times Java's, 2.69 times Rust's and 3.03 times C's.

| benchmark | xbase | Harbour | C | Rust | C# | Java | JavaScript |
|---|---:|---:|---:|---:|---:|---:|---:|
| ackermann | 167 | 1153 | 18 | 75 | 130 | 70 | 175 |
| aeval_heval | 172 | 1228 | 31 | 23 | 85 | 65 | 243 |
| array_append | 115 | 610 | 13 | 17 | 51 | 233 | 139 |
| array_growth | 160 | 622 | 19 | 28 | 86 | 147 | 322 |
| array_nested | 112 | 1383 | 24 | 37 | 41 | 50 | 68 |
| array_of_hashes | 176 | 273 | 46 | 144 | 195 | 167 | 88 |
| array_preallocate | 165 | 3339 | 33 | 61 | 83 | 146 | 172 |
| asort_codeblock | 71 | 352 | 16 | 10 | 44 | 69 | 56 |
| asort | 158 | 623 | 158 | 40 | 225 | 262 | 551 |
| async_generator | 252 | - | - | - | 141 | - | 266 |
| async_pi | 181 | 11917 | 174 | 179 | 185 | 245 | 223 |
| async | 142 | - | - | - | 153 | - | 119 |
| at_rat_strtran | 213 | 520 | 66 | 189 | 71 | 192 | 150 |
| base64 | 75 | 936 | 5 | 6 | 23 | 81 | 121 |
| bfs_dfs | 50 | 526 | 23 | 25 | 29 | 47 | 51 |
| binary_search | 180 | 1980 | 109 | 118 | 117 | 118 | 245 |
| binary_trees | 62 | 372 | 46 | 61 | 35 | 24 | 33 |
| bitlookup | 291 | 3673 | 44 | 35 | 189 | 30 | 44 |
| black_scholes | 69 | 645 | 24 | 16 | 23 | 34 | 41 |
| chameneos_redux | 198 | 315 | 161 | 146 | 88 | 204 | 427 |
| channels | 429 | - | 366 | 566 | 384 | 222 | 220 |
| closure_capture | 97 | 393 | 15 | 17 | 60 | 46 | 71 |
| codeblock_callback | 141 | 2069 | 25 | 24 | 34 | 40 | 123 |
| codeblock_eval | 185 | 1375 | 25 | 27 | 29 | 26 | 20 |
| counting_sort | 116 | 2026 | 56 | 55 | 55 | 82 | 113 |
| crc32 | 10 | 434 | 4 | 5 | 12 | 24 | 23 |
| crc_adler_fnv | 119 | 11723 | 84 | 80 | 105 | 152 | 326 |
| csv_parse | 206 | 1406 | 95 | 73 | 225 | 122 | 182 |
| date_time_ops | 119 | 221 | 123 | 102 | 142 | 473 | 261 |
| dgemm | 133 | 3015 | 10 | 9 | 30 | 38 | 48 |
| dhrystone | 174 | 1736 | 16 | 17 | 64 | 25 | 35 |
| dijkstra | 72 | 997 | 33 | 32 | 43 | 61 | 62 |
| fannkuch_redux | 155 | 2851 | 46 | 56 | 55 | 75 | 68 |
| fasta | 249 | 2842 | 128 | 148 | 174 | 213 | 959 |
| fft | 125 | 4384 | 32 | 32 | 36 | 58 | 47 |
| fib | 88 | 1998 | 24 | 43 | 94 | 75 | 171 |
| float | 29 | 1098 | 29 | 29 | 31 | 30 | 34 |
| gaussian_elimination | 149 | 3134 | 21 | 36 | 29 | 60 | 307 |
| gc | 426 | 1213 | 331 | 435 | 3018 | 490 | 491 |
| generator | 3 | - | 7 | 4 | 9 | 17 | 65 |
| geracao_html | 98 | 115 | 63 | 48 | 114 | 139 | 144 |
| hash_chaves_longas | 112 | 2990 | 120 | 108 | 83 | 85 | 371 |
| hash_massivo | 216 | 14884 | 99 | 71 | 177 | 181 | 196 |
| hash_resize | 167 | 136249 | 178 | 126 | 128 | 105 | 172 |
| heap_sort | 138 | 2150 | 71 | 71 | 84 | 80 | 106 |
| higher_order_chain | 108 | - | 25 | 25 | 138 | 61 | 138 |
| indexacao_simulada | 174 | 641 | 81 | 140 | 127 | 158 | 167 |
| index | 147 | 513 | - | 223 | 1189 | 324 | 3190 |
| inheritance_depth | 76 | 3462 | 24 | 32 | 6 | 11 | 21 |
| ini_parse | 144 | 519 | 100 | 106 | 243 | 214 | 133 |
| jaro_winkler | 239 | 2202 | 54 | 48 | 143 | 84 | 162 |
| join_arrays | 96 | 445 | 57 | 29 | 94 | 80 | 272 |
| json_roundtrip | 138 | 467 | 79 | 106 | 198 | 134 | 209 |
| julia_set | 188 | 6478 | 182 | 185 | 191 | 187 | 196 |
| knapsack | 103 | 1139 | 7 | 18 | 16 | 21 | 22 |
| k_nucleotide | 174 | 2113 | 26 | 66 | 152 | 165 | 225 |
| levenshtein | 152 | 3153 | 20 | 23 | 84 | 47 | 172 |
| linked_list | 263 | fail | 71 | 90 | 182 | 96 | 142 |
| linpack | 162 | 2969 | 17 | 11 | 58 | 31 | 58 |
| log_processing | 128 | 353 | 58 | 64 | 163 | 86 | 155 |
| longest_common_subsequence | 181 | 3068 | 22 | 21 | 73 | 41 | 116 |
| long_short_mix | 296 | 581 | 53 | 88 | 356 | 157 | 225 |
| loops | 216 | 11140 | 162 | 68 | 164 | 199 | 9134 |
| lru_cache | 224 | 5594 | 78 | 66 | 98 | 96 | 279 |
| mandelbrot | 151 | 6028 | 150 | 151 | 151 | 151 | 151 |
| map_filter_reduce | 160 | 1204 | 21 | 13 | 48 | 64 | 142 |
| matmul | 56 | 1363 | 8 | 16 | 13 | 18 | 31 |
| md5_sha1 | 158 | 12155 | 55 | 38 | 65 | 116 | 224 |
| memory_footprint | 187 | - | - | 95 | 255 | 315 | 150 |
| merge_sort | 153 | 2329 | 59 | 62 | 172 | 105 | 138 |
| meteor_contest | 235 | 2613 | 60 | 73 | 221 | 107 | 187 |
| method_call_overhead | 41 | 2476 | 9 | 19 | 6 | 18 | 12 |
| mixed | 47 | 1722 | 38 | 49 | 46 | 54 | 66 |
| monte_carlo_pi | 170 | 7603 | 110 | 85 | 198 | 115 | 201 |
| n_body | 141 | 2540 | 23 | 22 | 26 | 39 | 38 |
| n_queens | 194 | 1819 | 94 | 111 | 226 | 116 | 157 |
| numerical_integration | 65 | 1335 | 29 | 39 | 38 | 47 | 90 |
| numeric_formatting | 90 | 141 | 112 | 81 | 403 | 475 | 300 |
| object_churn | 155 | 535 | 53 | 84 | 158 | 160 | 93 |
| object_creation | 100 | 235 | 24 | 24 | 26 | 126 | 19 |
| oop | 10 | 680 | 4 | 7 | 4 | 7 | 6 |
| parallel_map | 94 | 704 | 17 | 13 | 42 | 52 | 165 |
| pidigits | 144 | 5105 | 28 | 29 | 303 | 58 | 164 |
| pipeline_tasks | 100 | - | 95 | 53 | 101 | - | 152 |
| pi | 36 | 711 | 5 | 6 | 24 | 31 | 85 |
| pivot_crosstab | 100 | 445 | 47 | 43 | 39 | 59 | 157 |
| prime_sieve_atkin | 230 | 4724 | 35 | 50 | 72 | 85 | 168 |
| producer_consumer | 201 | 758 | 61 | 95 | 60 | 69 | 251 |
| property_access | 46 | 1473 | 18 | 27 | 6 | 12 | 11 |
| qsort | 115 | 1269 | 54 | 56 | 111 | 75 | 106 |
| radix_sort | 52 | 3040 | 24 | 22 | 29 | 52 | 57 |
| random_massivo | 167 | 27042 | 86 | 81 | 100 | 137 | 2715 |
| ray_tracer | 157 | 1323 | 11 | 9 | 52 | 34 | 40 |
| relatorio_agregacao | 78 | 433 | 19 | 25 | 45 | 50 | 80 |
| reverse_complement | 188 | 4597 | 35 | 73 | 122 | 184 | 2078 |
| sequential_scan | 195 | 1206 | 59 | 28 | 119 | 80 | 200 |
| set_operations | 191 | 12052 | 91 | 94 | 106 | 191 | 166 |
| shared_counter | 389 | - | 63 | 63 | 57 | 68 | 150 |
| short_lived_objects | 72 | 2059 | 30 | 37 | 25 | 19 | 16 |
| sieve | 72 | 2824 | 27 | 42 | 49 | 52 | 70 |
| spectral_norm | 191 | 6400 | 53 | 292 | 160 | 59 | 82 |
| string_builder | 75 | 225 | 58 | 95 | 113 | 64 | 288 |
| string_concatenation | 161 | 816 | 99 | 151 | 139 | 81 | 265 |
| strings | 365 | 2399 | 175 | 249 | 376 | 338 | 609 |
| substr_massivo | 87 | 891 | 22 | 50 | 48 | 80 | 89 |
| sudoku_solver | 239 | 1931 | 30 | 35 | 136 | 40 | 66 |
| tak | 55 | 876 | 25 | 25 | 37 | 23 | 56 |
| tasks | 298 | - | 435 | - | 132 | 151 | 228 |
| tokenize_split | 162 | 426 | 76 | 46 | 224 | 107 | 108 |
| towers_of_hanoi | 70 | 1426 | 10 | 14 | 39 | 20 | 43 |
| trie | 115 | 304 | 22 | 37 | 117 | 51 | 92 |
| upper_lower_alltrim | 108 | 476 | 72 | 55 | 141 | 238 | 163 |
| url_encode | 120 | 1252 | 54 | 100 | 135 | 104 | 193 |
| validacao_dados | 124 | 1070 | 50 | 72 | 141 | 142 | 242 |
| virtual_dispatch | 53 | 371 | 9 | 7 | 11 | 18 | 43 |
| whetstone | 305 | 1218 | 35 | 27 | 64 | 56 | 56 |
| word_count | 169 | 529 | 51 | 44 | 106 | 123 | 150 |
| work_stealing | 95 | - | 28 | - | 101 | 46 | 204 |
| xml_simples | 124 | 193 | 54 | 84 | 89 | 181 | 129 |

## Use

```sh
build/compiler/xbc hello.prg                     # compile and link: ./hello
./hello
build/compiler/xbc -c hello.prg                  # only compile, to a native object: hello.o
cc hello.o build/runtime/libxbrt.a -lm -o hello  # link with the runtime yourself
```

`xbc` compiles a program's sources and links it with the runtime and the libraries it uses, through the system's C
compiler (`link.exe` from an MSVC developer prompt); `-c` writes the object only. A program of several sources compiles
in one run into one object (`xbc *.prg -o app`); calls between its modules need no declaration, and it starts at
`Main()`.

A project file builds a program, or a library, in one step, from the sources, options and libraries it names:

```sh
build/compiler/xbc samples/hello/hello.xbprj --out-dir=out     # out/hello
build/compiler/xbc libs/cli/cli.xbprj --out-dir=out/cli          # out/cli/libcli.a and cli.xbm
build/compiler/xbc samples/wordfreq/wordfreq.xbprj --out-dir=out -Lout
out/wordfreq -n 5 samples/wordfreq/sample.txt
```

The build compiles the libraries of `libs/` into `build/libs/` (`-Lbuild/libs`), and an install puts them where its
`xbc` finds them by itself. [docs/projects.md](docs/projects.md) describes the project files, and the options
that do the same from the command line: `--library=<name>`, `--lib=<name>`, `-L<dir>` and `-l<name>`.

Useful options (`xbc --help` lists all of them):

| Option | Meaning |
|---|---|
| `-o <file>` | the program (default: the first source's name), or with `-c` the object |
| `-c` | write the object only, without linking the program |
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
