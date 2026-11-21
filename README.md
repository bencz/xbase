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
run (Harbour's `-gc3` rounds integer products past 2^53, which the generators of the `WRONG` rows need exact; its
`linked_list` crashes). Measured on an Intel Core i9-11900K (8 cores, 16 threads), Linux 7.3: xbase 0.1 Release (`xbc
-O2`), Harbour 3.2.1dev (`hbmk2 -gc3`, its C output compiled with `gcc -O2`), C (`gcc` 16.2 `-O2`), Rust 1.97 (`-C
opt-level=2`), C# (.NET SDK 11.0.100-rc.3, Release, the default concurrent collector, a 4 GB heap), Java (OpenJDK 27),
JavaScript (Node.js 24; `loops` sums in `BigInt`, since its sum passes a double's exact integers). Python (PyPy,
CPython) was not measured in this run; Go was not installed. xbase is dynamically typed, as Harbour and JavaScript are;
C, Rust, C# and Java are statically typed. Over the benchmarks both ran, xbase's time is, as a geometric mean, 0.10 of
Harbour's, 1.02 times JavaScript's, 1.62 times C#'s, 1.64 times Java's, 2.74 times Rust's and 3.12 times C's.

| benchmark | xbase | Harbour | C | Rust | C# | Java | JavaScript |
|---|---:|---:|---:|---:|---:|---:|---:|
| ackermann | 166 | 1160 | 22 | 75 | 130 | 69 | 175 |
| aeval_heval | 169 | 1220 | 30 | 21 | 85 | 64 | 236 |
| array_append | 116 | 604 | 12 | 17 | 52 | 221 | 143 |
| array_growth | 152 | 618 | 20 | 32 | 83 | 146 | 323 |
| array_nested | 110 | 1367 | 25 | 38 | 41 | 49 | 69 |
| array_of_hashes | 175 | 271 | 46 | 140 | 191 | 167 | 94 |
| array_preallocate | 162 | 3315 | 30 | 60 | 84 | 147 | 168 |
| asort_codeblock | 69 | 338 | 16 | 11 | 45 | 64 | 56 |
| asort | 157 | 622 | 156 | 40 | 224 | 268 | 554 |
| async_generator | 252 | - | - | - | 139 | - | 267 |
| async_pi | 181 | 11823 | 180 | 181 | 181 | 242 | 221 |
| async | 142 | - | - | - | 155 | - | 118 |
| at_rat_strtran | 209 | 517 | 66 | 187 | 69 | 198 | 147 |
| base64 | 75 | 932 | 4 | 6 | 23 | 79 | 120 |
| bfs_dfs | 49 | 523 | 22 | 28 | 28 | 47 | 51 |
| binary_search | 183 | 1959 | 109 | 116 | 115 | 116 | 243 |
| binary_trees | 61 | 359 | 45 | 62 | 33 | 24 | 33 |
| bitlookup | 288 | 3646 | 44 | 35 | 190 | 30 | 44 |
| black_scholes | 70 | WRONG | 24 | 17 | 23 | 33 | 40 |
| chameneos_redux | 203 | 308 | 155 | 148 | 85 | 205 | 425 |
| channels | 424 | - | 358 | 543 | 393 | 239 | 210 |
| closure_capture | 96 | 391 | 14 | 18 | 60 | 45 | 71 |
| codeblock_callback | 140 | 2051 | 25 | 28 | 34 | 40 | 121 |
| codeblock_eval | 183 | 1370 | 24 | 26 | 29 | 25 | 20 |
| counting_sort | 116 | 1992 | 56 | 54 | 54 | 81 | 111 |
| crc32 | 11 | 433 | 5 | 7 | 12 | 23 | 23 |
| crc_adler_fnv | 119 | WRONG | 84 | 78 | 104 | 153 | 322 |
| csv_parse | 204 | 1384 | 92 | 73 | 223 | 121 | 180 |
| date_time_ops | 117 | 218 | 122 | 101 | 142 | 484 | 260 |
| dgemm | 131 | WRONG | 9 | 10 | 27 | 38 | 51 |
| dhrystone | 168 | 1722 | 16 | 13 | 62 | 30 | 36 |
| dijkstra | 71 | 997 | 33 | 32 | 43 | 61 | 62 |
| fannkuch_redux | 152 | 2828 | 45 | 56 | 55 | 74 | 68 |
| fasta | 250 | 2807 | 127 | 147 | 173 | 209 | 960 |
| fft | 123 | WRONG | 34 | 32 | 35 | 57 | 47 |
| fib | 87 | 1982 | 23 | 42 | 92 | 73 | 172 |
| float | 29 | 1090 | 29 | 29 | 30 | 31 | 33 |
| gaussian_elimination | 148 | 3104 | 20 | 37 | 29 | 59 | 299 |
| gc | 423 | 1200 | 322 | 436 | 3001 | 500 | 479 |
| generator | 3 | - | 7 | 4 | 9 | 17 | 65 |
| geracao_html | 96 | 111 | 63 | 47 | 114 | 142 | 142 |
| hash_chaves_longas | 112 | 2981 | 120 | 106 | 83 | 84 | 367 |
| hash_massivo | 207 | 14735 | 97 | 69 | 172 | 181 | 195 |
| hash_resize | 163 | 134968 | 179 | 126 | 127 | 103 | 169 |
| heap_sort | 135 | 2122 | 69 | 69 | 83 | 78 | 105 |
| higher_order_chain | 106 | - | 25 | 24 | 138 | 61 | 139 |
| indexacao_simulada | 168 | 628 | 79 | 137 | 126 | 159 | 161 |
| index | 148 | 508 | - | 222 | 1186 | 314 | 3154 |
| inheritance_depth | 86 | 3447 | 24 | 32 | 6 | 10 | 19 |
| ini_parse | 142 | 518 | 99 | 106 | 253 | 214 | 129 |
| jaro_winkler | 237 | 2177 | 53 | 47 | 144 | 81 | 161 |
| join_arrays | 95 | 441 | 56 | 30 | 94 | 82 | 264 |
| json_roundtrip | 134 | 467 | 80 | 103 | 194 | 134 | 208 |
| julia_set | 199 | 6421 | 183 | 184 | 184 | 187 | 193 |
| knapsack | 103 | 1136 | 7 | 23 | 16 | 20 | 24 |
| k_nucleotide | 174 | 2067 | 26 | 65 | 152 | 165 | 224 |
| levenshtein | 152 | 3112 | 20 | 24 | 83 | 47 | 173 |
| linked_list | 260 | fail | 68 | 87 | 175 | 93 | 136 |
| linpack | 156 | 2826 | 20 | 12 | 55 | 31 | 56 |
| log_processing | 122 | 342 | 55 | 64 | 162 | 84 | 153 |
| longest_common_subsequence | 178 | 3031 | 22 | 22 | 73 | 41 | 115 |
| long_short_mix | 295 | 564 | 53 | 91 | 353 | 156 | 219 |
| loops | 214 | 11048 | 162 | 68 | 163 | 196 | 9065 |
| lru_cache | 219 | 5505 | 77 | 69 | 97 | 97 | 275 |
| mandelbrot | 152 | 5961 | 150 | 148 | 148 | 148 | 151 |
| map_filter_reduce | 159 | 1184 | 22 | 13 | 49 | 64 | 143 |
| matmul | 55 | 1350 | 10 | 14 | 13 | 18 | 32 |
| md5_sha1 | 156 | WRONG | 54 | 38 | 63 | 116 | 222 |
| memory_footprint | 190 | - | - | 92 | 249 | 315 | 151 |
| merge_sort | 154 | 2306 | 58 | 61 | 169 | 106 | 137 |
| meteor_contest | 235 | 2587 | 59 | 73 | 220 | 106 | 184 |
| method_call_overhead | 181 | 2449 | 7 | 20 | 7 | 18 | 13 |
| mixed | 48 | 1712 | 38 | 49 | 46 | 54 | 66 |
| monte_carlo_pi | 172 | WRONG | 109 | 84 | 200 | 115 | 197 |
| n_body | 138 | 2517 | 22 | 22 | 26 | 35 | 38 |
| n_queens | 192 | 1803 | 94 | 109 | 224 | 114 | 156 |
| numerical_integration | 65 | 1329 | 29 | 38 | 37 | 46 | 90 |
| numeric_formatting | 89 | 140 | 112 | 81 | 404 | 474 | 302 |
| object_churn | 156 | 531 | 55 | 83 | 156 | 155 | 87 |
| object_creation | 107 | 234 | 21 | 23 | 26 | 127 | 19 |
| oop | 10 | 679 | 5 | 12 | 4 | 7 | 6 |
| parallel_map | 93 | 700 | 18 | 13 | 45 | 55 | 167 |
| pidigits | 141 | 5043 | 28 | 30 | 303 | 58 | 164 |
| pipeline_tasks | 102 | - | 94 | 53 | 95 | - | 152 |
| pi | 36 | 702 | 5 | 7 | 23 | 31 | 87 |
| pivot_crosstab | 99 | 442 | 46 | 43 | 40 | 58 | 154 |
| prime_sieve_atkin | 231 | 4664 | 37 | 50 | 71 | 85 | 166 |
| producer_consumer | 195 | 762 | 60 | 90 | 62 | 67 | 245 |
| property_access | 103 | 1468 | 21 | 28 | 6 | 12 | 11 |
| qsort | 114 | 1258 | 54 | 56 | 110 | 80 | 106 |
| radix_sort | 51 | WRONG | 24 | 21 | 28 | 55 | 59 |
| random_massivo | 166 | WRONG | 85 | 80 | 100 | 135 | 2693 |
| ray_tracer | 157 | 1308 | 10 | 10 | 52 | 35 | 43 |
| relatorio_agregacao | 79 | 432 | 20 | 25 | 47 | 51 | 79 |
| reverse_complement | 186 | 4551 | 37 | 72 | 119 | 185 | 2070 |
| sequential_scan | 201 | 1196 | 59 | 28 | 121 | 78 | 196 |
| set_operations | 185 | 11906 | 90 | 92 | 105 | 187 | 165 |
| shared_counter | 425 | - | 67 | 61 | 49 | 71 | 149 |
| short_lived_objects | 148 | 2037 | 30 | 36 | 25 | 20 | 16 |
| sieve | 71 | 2794 | 27 | 42 | 49 | 51 | 70 |
| spectral_norm | 189 | 6346 | 52 | 292 | 159 | 59 | 83 |
| string_builder | 76 | 223 | 58 | 95 | 112 | 64 | 288 |
| string_concatenation | 164 | 811 | 99 | 151 | 133 | 80 | 259 |
| strings | 364 | 2382 | 174 | 250 | 365 | 321 | 602 |
| substr_massivo | 88 | 888 | 20 | 50 | 49 | 82 | 89 |
| sudoku_solver | 239 | 1885 | 30 | 34 | 133 | 40 | 67 |
| tak | 53 | 851 | 24 | 24 | 37 | 23 | 55 |
| tasks | 297 | - | 434 | - | 131 | 145 | 228 |
| tokenize_split | 158 | 424 | 76 | 46 | 203 | 107 | 109 |
| towers_of_hanoi | 67 | 1323 | 9 | 13 | 38 | 20 | 40 |
| trie | 111 | 275 | 21 | 32 | 100 | 45 | 81 |
| upper_lower_alltrim | 103 | 460 | 68 | 54 | 134 | 224 | 151 |
| url_encode | 121 | 1202 | 51 | 93 | 125 | 99 | 183 |
| validacao_dados | 122 | 1054 | 46 | 65 | 122 | 136 | 227 |
| virtual_dispatch | 91 | 346 | 10 | 7 | 11 | 17 | 40 |
| whetstone | 298 | 1194 | 34 | 27 | 62 | 55 | 53 |
| word_count | 166 | 512 | 50 | 43 | 105 | 120 | 144 |
| work_stealing | 89 | - | 26 | - | 101 | 44 | 179 |
| xml_simples | 123 | 183 | 51 | 81 | 87 | 138 | 125 |

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
