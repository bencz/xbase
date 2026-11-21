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
C, Rust, C# and Java are statically typed. Over the benchmarks both ran, xbase's time is, as a geometric mean, 0.11 of
Harbour's, 1.10 times JavaScript's, 1.75 times C#'s, 1.78 times Java's, 2.99 times Rust's and 3.37 times C's.

| benchmark | xbase | Harbour | C | Rust | C# | Java | JavaScript |
|---|---:|---:|---:|---:|---:|---:|---:|
| ackermann | 171 | 1195 | 18 | 74 | 129 | 68 | 174 |
| aeval_heval | 181 | 1229 | 30 | 21 | 87 | 65 | 242 |
| array_append | 194 | 612 | 18 | 18 | 51 | 233 | 144 |
| array_growth | 158 | 621 | 18 | 30 | 84 | 146 | 325 |
| array_nested | 113 | 1375 | 25 | 38 | 41 | 49 | 67 |
| array_of_hashes | 185 | 272 | 46 | 139 | 193 | 171 | 90 |
| array_preallocate | 161 | 3334 | 34 | 63 | 83 | 148 | 171 |
| asort_codeblock | 72 | 342 | 15 | 10 | 45 | 61 | 57 |
| asort | 164 | 622 | 159 | 39 | 223 | 262 | 544 |
| async_generator | 254 | - | - | - | 140 | - | 263 |
| async_pi | 183 | 11902 | 175 | 182 | 178 | 242 | 222 |
| async | 142 | - | - | - | 155 | - | 122 |
| at_rat_strtran | 212 | 524 | 65 | 185 | 69 | 193 | 149 |
| base64 | 118 | 934 | 4 | 6 | 23 | 77 | 122 |
| bfs_dfs | 50 | 523 | 23 | 24 | 29 | 45 | 51 |
| binary_search | 181 | 1974 | 107 | 115 | 115 | 118 | 248 |
| binary_trees | 62 | 364 | 46 | 64 | 34 | 24 | 33 |
| bitlookup | 282 | 3661 | 44 | 35 | 192 | 30 | 44 |
| black_scholes | 70 | WRONG | 23 | 17 | 24 | 33 | 41 |
| chameneos_redux | 196 | 314 | 151 | 149 | 86 | 207 | 426 |
| channels | 428 | - | 368 | 557 | 362 | 231 | 217 |
| closure_capture | 115 | 393 | 15 | 17 | 60 | 47 | 72 |
| codeblock_callback | 165 | 2058 | 24 | 24 | 33 | 40 | 122 |
| codeblock_eval | 222 | 1366 | 24 | 27 | 29 | 25 | 20 |
| counting_sort | 117 | 2012 | 57 | 54 | 54 | 82 | 113 |
| crc32 | 10 | 434 | 5 | 4 | 12 | 24 | 22 |
| crc_adler_fnv | 122 | WRONG | 85 | 79 | 105 | 153 | 324 |
| csv_parse | 221 | 1393 | 94 | 75 | 228 | 123 | 180 |
| date_time_ops | 123 | 218 | 122 | 105 | 141 | 474 | 259 |
| dgemm | 132 | WRONG | 11 | 9 | 28 | 38 | 51 |
| dhrystone | 174 | 1731 | 16 | 13 | 62 | 25 | 36 |
| dijkstra | 74 | 1001 | 34 | 32 | 43 | 67 | 64 |
| fannkuch_redux | 153 | 2849 | 46 | 57 | 54 | 76 | 68 |
| fasta | 241 | 2824 | 127 | 150 | 175 | 212 | 961 |
| fft | 129 | WRONG | 32 | 32 | 36 | 75 | 46 |
| fib | 88 | 1991 | 24 | 43 | 95 | 74 | 172 |
| float | 29 | 1091 | 29 | 29 | 30 | 30 | 32 |
| gaussian_elimination | 148 | 3120 | 20 | 37 | 29 | 59 | 301 |
| gc | 426 | 1202 | 330 | 440 | 3061 | 492 | 484 |
| generator | 59 | - | 7 | 5 | 9 | 17 | 68 |
| geracao_html | 102 | 114 | 63 | 47 | 114 | 143 | 142 |
| hash_chaves_longas | 112 | 2988 | 120 | 106 | 88 | 85 | 371 |
| hash_massivo | 208 | 14863 | 98 | 71 | 184 | 182 | 194 |
| hash_resize | 167 | 135786 | 181 | 127 | 125 | 106 | 170 |
| heap_sort | 137 | 2155 | 69 | 69 | 84 | 79 | 105 |
| higher_order_chain | 235 | - | 24 | 24 | 143 | 61 | 141 |
| indexacao_simulada | 175 | 635 | 80 | 138 | 131 | 158 | 163 |
| index | 152 | 506 | - | 226 | 1193 | 320 | 3165 |
| inheritance_depth | 194 | 3469 | 24 | 32 | 6 | 10 | 19 |
| ini_parse | 168 | 521 | 99 | 108 | 253 | 215 | 131 |
| jaro_winkler | 243 | 2189 | 53 | 48 | 141 | 81 | 160 |
| join_arrays | 98 | 445 | 56 | 29 | 96 | 82 | 267 |
| json_roundtrip | 141 | 470 | 80 | 103 | 197 | 136 | 208 |
| julia_set | 187 | 6457 | 185 | 182 | 185 | 188 | 195 |
| knapsack | 90 | 1141 | 7 | 18 | 16 | 21 | 24 |
| k_nucleotide | 184 | 2089 | 27 | 67 | 150 | 169 | 224 |
| levenshtein | 165 | 3145 | 20 | 23 | 85 | 47 | 173 |
| linked_list | 271 | fail | 69 | 88 | 178 | 94 | 137 |
| linpack | 163 | 2838 | 17 | 11 | 57 | 31 | 56 |
| log_processing | 132 | 342 | 56 | 65 | 164 | 85 | 152 |
| longest_common_subsequence | 180 | 3067 | 22 | 22 | 74 | 41 | 117 |
| long_short_mix | 296 | 568 | 53 | 90 | 354 | 160 | 220 |
| loops | 214 | 11113 | 161 | 68 | 164 | 196 | 9186 |
| lru_cache | 227 | 5533 | 79 | 68 | 98 | 98 | 278 |
| mandelbrot | 152 | 6005 | 151 | 150 | 149 | 149 | 151 |
| map_filter_reduce | 174 | 1193 | 22 | 13 | 49 | 65 | 143 |
| matmul | 55 | 1350 | 9 | 14 | 13 | 18 | 32 |
| md5_sha1 | 164 | WRONG | 54 | 38 | 63 | 105 | 226 |
| memory_footprint | 195 | - | - | 95 | 256 | 311 | 153 |
| merge_sort | 153 | 2323 | 59 | 62 | 172 | 105 | 138 |
| meteor_contest | 323 | 2608 | 61 | 74 | 222 | 106 | 187 |
| method_call_overhead | 183 | 2471 | 8 | 18 | 6 | 17 | 13 |
| mixed | 46 | 1721 | 38 | 50 | 47 | 55 | 66 |
| monte_carlo_pi | 172 | WRONG | 109 | 85 | 199 | 117 | 203 |
| n_body | 141 | 2540 | 23 | 22 | 26 | 38 | 38 |
| n_queens | 193 | 1815 | 95 | 111 | 226 | 116 | 157 |
| numerical_integration | 66 | 1334 | 28 | 37 | 38 | 46 | 89 |
| numeric_formatting | 91 | 141 | 113 | 82 | 402 | 492 | 306 |
| object_churn | 157 | 534 | 52 | 83 | 155 | 157 | 90 |
| object_creation | 105 | 232 | 21 | 34 | 27 | 129 | 21 |
| oop | 11 | 681 | 7 | 9 | 4 | 7 | 7 |
| parallel_map | 92 | 707 | 19 | 13 | 43 | 51 | 167 |
| pidigits | 142 | 5079 | 28 | 29 | 304 | 59 | 164 |
| pipeline_tasks | 107 | - | 94 | 55 | 102 | - | 154 |
| pi | 38 | 705 | 5 | 7 | 24 | 32 | 78 |
| pivot_crosstab | 104 | 443 | 47 | 43 | 39 | 59 | 157 |
| prime_sieve_atkin | 237 | 4713 | 35 | 49 | 71 | 86 | 171 |
| producer_consumer | 193 | 759 | 60 | 95 | 60 | 71 | 249 |
| property_access | 102 | 1469 | 18 | 29 | 6 | 12 | 11 |
| qsort | 115 | 1268 | 54 | 56 | 111 | 72 | 107 |
| radix_sort | 52 | WRONG | 24 | 21 | 29 | 55 | 59 |
| random_massivo | 174 | WRONG | 85 | 82 | 99 | 136 | 2703 |
| ray_tracer | 169 | 1319 | 10 | 9 | 51 | 33 | 43 |
| relatorio_agregacao | 79 | 430 | 20 | 26 | 45 | 51 | 79 |
| reverse_complement | 190 | 4587 | 36 | 73 | 120 | 185 | 2082 |
| sequential_scan | 206 | 1202 | 58 | 29 | 119 | 87 | 197 |
| set_operations | 193 | 12016 | 93 | 94 | 103 | 191 | 164 |
| shared_counter | 436 | - | 63 | 63 | 55 | 64 | 171 |
| short_lived_objects | 152 | 2048 | 30 | 35 | 25 | 20 | 16 |
| sieve | 74 | 2822 | 32 | 42 | 49 | 52 | 71 |
| spectral_norm | 200 | 6387 | 53 | 291 | 161 | 59 | 82 |
| string_builder | 105 | 224 | 58 | 95 | 114 | 65 | 289 |
| string_concatenation | 195 | 816 | 99 | 149 | 138 | 83 | 262 |
| strings | 450 | 2405 | 176 | 249 | 373 | 324 | 607 |
| substr_massivo | 112 | 890 | 21 | 51 | 49 | 81 | 89 |
| sudoku_solver | 248 | 1903 | 31 | 35 | 135 | 39 | 67 |
| tak | 54 | 854 | 24 | 25 | 38 | 23 | 55 |
| tasks | 285 | - | 431 | - | 130 | 153 | 223 |
| tokenize_split | 169 | 423 | 76 | 46 | 207 | 106 | 107 |
| towers_of_hanoi | 71 | 1333 | 10 | 14 | 38 | 19 | 41 |
| trie | 118 | 277 | 21 | 34 | 104 | 45 | 87 |
| upper_lower_alltrim | 117 | 462 | 71 | 55 | 138 | 226 | 153 |
| url_encode | 134 | 1206 | 51 | 95 | 127 | 100 | 188 |
| validacao_dados | 127 | 1061 | 46 | 65 | 123 | 143 | 227 |
| virtual_dispatch | 97 | 345 | 8 | 11 | 11 | 17 | 38 |
| whetstone | 301 | 1197 | 35 | 27 | 65 | 55 | 53 |
| word_count | 173 | 514 | 49 | 44 | 106 | 123 | 145 |
| work_stealing | 88 | - | 27 | - | 97 | 41 | 182 |
| xml_simples | 123 | 186 | 53 | 82 | 86 | 177 | 125 |

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
