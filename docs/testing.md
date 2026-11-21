# Testing

Tests run through CTest (`ctest --test-dir build --output-on-failure`); `tests/CMakeLists.txt` discovers them.

| Kind | Files | Check |
|---|---|---|
| Execution | `tests/run/<name>.prg`, `<name>.out`, optional `<name>.rc`, `<name>.err`, `<name>.args`, `<name>.in`, `<name>.serial`, `<name>.with/*.prg`, `<name>.d/*.prg`, `<name>.rt/*.prg` | The program is compiled with `xbc` (together with the modules in `<name>.with/`, in the same run), linked with `xbrt` (and with the modules in `<name>.d/`, each compiled on its own with `--no-entry`, and in `<name>.rt/`, compiled as modules of the runtime with `--runtime`) and run, with the command-line arguments in `<name>.args` (one per line; CTest cannot pass an empty one) and `<name>.in` as standard input; standard output must equal `<name>.out`, the exit code must equal `<name>.rc` (default 0), and standard error must contain `<name>.err`. A test with `<name>.serial` (empty) runs alone: it commits much memory (a recursion to the end of the stack, about 1 GB on 64-bit targets). |
| Preprocessor | `tests/pp/<name>.prg`, `<name>.ppo` | `xbc -E` output must equal `<name>.ppo`. More, in `tests/CMakeLists.txt`: `pp.custom_std` (`-u<file>`), `pp.sources_pattern` (several sources and a file pattern, `tests/pp/glob`), and `pp.target_*` (the names predefined for each `--target`, `tests/pp/target`). |
| Compile failure | `tests/fail/<name>.prg`, `<name>.err`, optional `<name>.with/*.prg`, `<name>.args` | Compiling (with the modules in `<name>.with/`, and the `xbc` options in `<name>.args`, one per line) must fail, report the text in `<name>.err`, and report exactly one error. |
| Warning | `tests/warn/<name>.prg`, `<name>.warn` | Compiling at `-O2` must succeed and report the text in `<name>.warn` (no warning at all when it is empty). |
| xIR | `tests/xir/<name>.prg`, `<name>.xir` | The xIR `xbc --dump-xir` writes (after the xIR passes, at the default `-O2`; a `types_<name>` test with `--dump-xir-types`) must equal `<name>.xir`. |
| Code generation | `xb_add_codegen_test` in `tests/CMakeLists.txt` | The IR `xbc --dump-ir` emits for a cross target must match a regular expression. |
| Declarations | `runtime.declarations` (`tests/declarations_test.cmake`) | `include/rtl.ch` declares exactly the `xbf_` functions defined in `runtime/src`. |
| Package | `package.install`, `package.program` (`tests/package/`, a CMake project of its own: `-DXBASE_ROOT=<install>`, optional `-DXBC_TARGET=<triple>` and `CMAKE_CROSSCOMPILING_EMULATOR` for a host `xbc` with a runtime built for another target) | The build is installed into a folder of the build (`cmake --install`); a program of two modules built from that install alone (`README.md`, Packages) prints `expected.txt` (line ends as the system writes them), and `xbc`'s dependency file shows it read `std.ch` from the install's include folder without `-I`. The release workflow runs the same project on every Windows package. |
| Platform | `tests/platform/<name>.c`, optional `<name>.prg` | A C program linked with `xbrt` (and with `<name>.prg`, compiled with `--no-entry`, whose functions it calls as `xbp_NAME`) exercises the runtime's own services (threads, locks, atomics, waiting on an address, collections while compiled loops run) under contention, and exits with 0 when every check passed (`check.h`). |

Every test has a 30-second timeout, so a hang or a loop fails instead of stalling the run. Every execution test is
built twice: `run.<name>` as `xbc` builds by default (`-O2`) and `run0.<name>` with `-O0` (no xIR passes); both must
give `<name>.out`, so that no optimization changes what a program does. Each runs in a directory of its own in the
build (`tests/work/run.<name>`, `tests/work/run0.<name>`), so tests running at once never see each other's files, even
those that list a directory; a test that writes files still names them after itself (`memvars_save_*.mem`) and
deletes them at the end.

Include files used by a test sit next to it (for example `tests/run/commands.ch`). Execution tests are rebuilt when
`std.ch` or an included file changes (`xbc --depfile`).

A table
written by the reference that a test needs is kept inside the test (its bytes in hexadecimal, as in
`tests/run/rdd_format.prg`), written out when the test runs.

## Expected outputs

Expected outputs follow Clipper behavior; `?` starts with a newline, so `.out` files usually begin with an empty line
and end without one. When Harbour is installed, an execution test's expected output is checked against it:

```sh
(ulimit -v 4000000; timeout 120 hbmk2 -static -gtcgi -q test.prg)
(ulimit -v 2000000; timeout 10 ./test | head -c 100000) > harbour.out
```

`-gtcgi` gives plain console output. Harbour's own mode has compiler extensions that change results (its optimizer
turns `x := x op v` into `x op= v` and reads `x` last); language rules such as evaluation order are checked in its
Clipper mode, adding `-prgflag=-kc` to `hbmk2`. A difference is either fixed or documented as a deliberate one in the
matching doc (for example the last digits of large doubles in [runtime.md](runtime.md#display), or each operand of
`a[i] op= v` evaluated once in [compiler.md](compiler.md#syntax-tree-to-xir)). Preprocessor output can be
compared with `harbour -p -s -q test.prg`, whose `.ppo` keeps a line's statements joined by `;` and differs in
spacing. Expressions on literals are folded at compile time, with results that can differ from the run-time operators
(`10 / 5` is `2`, `n / 5` is `2.00`): tests of run-time operators use variables.

A rule is tested across its inputs, not on one example: every pair of a list of values with every operator
(`number_matrix`: integers and doubles, signs, fractions, the display widths' limits, products past 64 bits;
`string_matrix`: empty, blank and one-byte strings, case, trailing spaces, NUL and bytes above 127, with every
comparison under both SET EXACT settings), and each pair as a program meets it: literals (folded when compiled), values
computed at run time (elements of an array made then), values the compiler knows once calls are inlined (folded in xIR),
macros compiled from text, and input (`Val()` of the command line, or its text with `<n>` for `Chr( n )`). Code shapes
get theirs too: a function far past the optimizer's size limit (`large_function`), what each kind of function returns as
its callers see it (`return_types`), `TRY` nested at N levels (`try_nested`: every choice of the levels that have a
`CATCH`, the request raised at each level by `Throw()`, by an error, again in each `CATCH` or in a `FINALLY`, nesting
through recursion 60 levels deep, and `EXIT`, `LOOP` and `RETURN` through every level).
Code that takes text at run time is tested with text read as input (`eval_input`: codeblocks compiled from the
command line, run with every kind of argument, and the errors bad text and bad arguments give). The reference shows
what a rule gives; where it is wrong, xbase does better, and the difference is documented.

Every behavior change adds or extends a test. Changes to code generation or to the runtime ABI are also cross-compiled
for a 32-bit and a big-endian target, for example:

```sh
build/compiler/xbc --target=powerpc-linux-gnu tests/run/funcs.prg -o /tmp/claude/funcs-ppc.o
build/compiler/xbc --target=s390x-linux-gnu tests/run/funcs.prg -o /tmp/claude/funcs-s390x.o
```

A 32-bit build runs the whole suite on a 64-bit host: `-DXB_TARGET_C_FLAGS=-m32 -DXB_TARGET=i686-pc-linux-gnu`
([README.md](../README.md#build)); the tests of the address space (`platform.address_space`, `platform.workers_most`)
measure the process's `RLIMIT_AS`, so the suite runs without a `ulimit -v` of its own (a cgroup's memory limit bounds
it instead). Running big-endian programs needs a target sysroot and an emulator (for example `qemu-s390x`); this is not
automated.

## Sanitizers

A build of its own, configured with the sanitizer in `CMAKE_C_FLAGS`, `CMAKE_CXX_FLAGS` and `CMAKE_EXE_LINKER_FLAGS`,
runs the whole suite (AddressSanitizer with UndefinedBehaviorSanitizer) or the platform tests (ThreadSanitizer, built
with Clang: GCC refuses the fences of `platform/atomic.h` under it; its runs need the address space the sanitizer
reserves, so no `ulimit -v`). The
AddressSanitizer build adds `-fsanitize-address-use-after-return=never`: the collector scans the threads' stacks for the
addresses they hold ([runtime.md](runtime.md#garbage-collector)), and the "fake stack" frames that option would
otherwise use keep local variables off the stack, where no scan finds them. `run.space_too_large`, which asks for more
memory than there is, expects the allocation to fail instead of the sanitizer to stop the program
(`ASAN_OPTIONS=allocator_may_return_null=1` for that run).

## Benchmarks

`bench/primitives/<name>.prg` time the primitives of the language one by one (`strings`: every string function and
operator, appends, iteration over a string's bytes; `iteration`: `FOR EACH` over each kind of collection, nested, by
index, and `Enumerable`'s chains next to the loops that do their work), each inline in a loop on data the pass chooses,
in nanoseconds per operation; `<name>.c` does the same work in C as a C program would write it. `cmake --build
build-release --target primitives` builds both and prints them side by side: where a primitive stands against C.

`bench/<name>.prg` are benchmark programs: recursive calls (`fib`), an integer loop (`loops`), arrays (`sieve`,
`matmul`), sorting in xBase (`qsort`) and with `ASort` (`asort`), strings (`strings`), the bit operators and string
building (`crc32`), array cells read and changed in place in deeply nested loops (`pi`, digits of pi from a tape-machine
program; `bench_pi <n>` prints n digits), double arithmetic (`float`, the Mandelbrot set) and integers mixed with
doubles (`mixed`: `i + 3.14`, `i / 4`, `Int()`, comparisons across both), and messages to objects of compiled classes
(`oop`: method calls, one through `Super`, variable reads and assignments), generators (`generator`: one feeding
another), the collector (`gc`: strings and small arrays of many sizes replaced at random among others that stay, the
heap's fragmentation; its memory is read with `/usr/bin/time`), and an order over a table (`index`: built, searched with
SEEK, walked, kept up to date as records are appended; its files are deleted at the end), tasks (`tasks`: 200,000 short
tasks joined, a 50,000-link continuation chain, tasks waiting for tasks they start) and channels (`channels`: producer
and consumer tasks over a bounded channel, then `TrySend`/`TryReceive` pairs), asynchronous functions (`async`: a
million calls awaited that complete at once, round trips between two of them over channels, ten thousand suspended at
once; `async_pi`: the Basel sum over 32 of them awaiting CPU-bound tasks, every processor busy), asynchronous generators
(`async_generator`: a pipeline of three, a source of a million numbers that yields the thread every thousand, a filter
and a map, through `AWAIT FOR EACH`; in C#, JavaScript and Python with their asynchronous iterators), and calls of a
function with a short loop (`bitlookup`: the bitwise AND from a table of nibbles). Beside these, programs after the
classic benchmark suites and everyday xBase work, each named for what it measures: numeric kernels (`n_body`,
`spectral_norm`, `fft`, `linpack`, `dgemm`, `whetstone`, `dhrystone`, `ray_tracer`, ...), recursion and search (`tak`,
`ackermann`, `n_queens`, `sudoku_solver`, `knapsack`, `dijkstra`, ...), sorting (`merge_sort`, `heap_sort`,
`radix_sort`, ...), checksums and hashes (`crc_adler_fnv`, `md5_sha1`), text (`fasta`, `k_nucleotide`,
`reverse_complement`, `levenshtein`, `base64`, `csv_parse`, ...), hashes and arrays (`hash_massivo`, `lru_cache`,
`trie`, `array_growth`, ...), codeblocks (`codeblock_eval`, `map_filter_reduce`, `closure_capture`, ...), objects
(`method_call_overhead`, `virtual_dispatch`, `object_churn`, ...), business data (`relatorio_agregacao`,
`pivot_crosstab`, `sequential_scan` over a DBF, `json_roundtrip`, `log_processing`, ...), threads and tasks
(`producer_consumer`, `work_stealing`, `parallel_map`, `shared_counter`, `pipeline_tasks`, `chameneos_redux`) and the
memory objects take (`memory_footprint`, checked against `__GcMemory()`). Each one times its own work with
`ElapsedMs()`, checks its result against a value computed independently, and prints one line: `<name> <milliseconds>
ok|WRONG`. The `bench` target (not part of the default build or of `ctest`) compiles them with `-O2`
(`bench/CMakeLists.txt`, through the same `xb_compile_prg` as the tests) and runs `bench/run_bench.cmake`: every program
`XB_BENCH_REPEAT` times (a cache variable, default 3), the best time of each in a table, and a failure when one does not
run or reports `WRONG`. It warns when the build is not Release. Performance changes are measured with it, before and
after.

The same programs in other languages, for a comparative base (the same algorithm, sizes and checks, the same output
line), are in `bench/c/` (one file each, `bench.h` shared; 64-bit integers as `int64_t`), `bench/rust/` (standard
library only), `bench/csharp/` (one project, `Bench <name>`; `Xbase.cs` holds the number formatting several programs
need), `bench/java/` (one class each), `bench/js/` (Node.js), `bench/python/` (CPython and PyPy; `base64` is
`base64_bench.py`, since a file named for a standard module would hide that module from the ports that import it) and
`bench/harbour/` (Harbour, for the programs whose `.prg` it cannot compile as they are: its bit functions for xbase's
bit operators, its hash functions and token splitter for xbase's, position loops for `FOR EACH` over a hash's pairs,
its threads for tasks; it compiles the others themselves). Each uses its language's own tools where xBase uses the
runtime's (its sort, strings, hash tables, threads, tasks, channels and `async`); where its library has none, the port
writes one (C's hash tables, queues and pool of tasks, JSON in C and Java). `bench/c/pi.c` and the other `pi` ports
are translated statement by statement from `bench/pi.prg`. Where a compiler would remove the work the benchmark
measures, the port keeps it as xbase does it: `bitlookup` makes its 7 calls of `AND()` a pair (IOR and XOR written out
as the `.prg`'s macros expand them; C `noinline, noipa`, Rust `inline(never)` with `black_box` arguments, C#
`NoInlining`; the Java and JavaScript JITs may still inline them), C's `generator` steps through function pointers it
cannot follow, and the calls, dispatch and objects the object benchmarks measure go through the same barriers. The
ports follow two xBase rules that matter for them: a `FOR` loop's limit is evaluated at every test (`TO 3 + Rand( 6 )`
draws again), and `%` gives a number with SET DECIMALS decimals (`Str( i % 100 )` is 13 columns wide).

The differences that remain:

- Text built by appending is a growing buffer in C (`strings`, `crc32` and the text benchmarks), a `StringBuilder` in
  C# and Java, and a list joined at the end in Python (PyPy copies a string at every `+=`).
- No DBF or NTX outside xbase and Harbour: `sequential_scan` writes its records, fixed-width text as a DBF keeps them
  but without a header, to `bench_seqscan.dat` and scans that; `index` keeps its records in `bench_index.dat` (Java's
  in memory) and its order in memory (a sorted set, or a sorted array in JavaScript and Python).
- `memory_footprint` checks heap bytes only, where the language can measure them (Rust through a counting allocator, C#,
  Java, JavaScript), without the counts of live objects.
- Threads and tasks: Rust's channel has one receiver, which `channels`' two consumers share under a mutex, and its
  `parallel_map` runs 64 threads; JavaScript's `tasks`, `channels`, `async` and `pipeline_tasks` take turns of the event
  loop, while its threaded programs use `worker_threads` with shared memory (its `AtomicInt` 32-bit); Python's
  `parallel_map` runs on a pool of processes (the interpreter lock) and its `work_stealing` on a pool of threads, with
  no parallel speed-up; C's and JavaScript's `work_stealing` share one queue among the workers (none steals).
- JavaScript computes in `BigInt` where integers pass 2^53 (`loops`, `random_massivo`, `radix_sort`'s total,
  `meteor_contest`'s masks).
- Harbour (`-gc3`) multiplies integers in doubles, so products above 2^53 are rounded: the programs that draw numbers
  with `seed * 1103515245` print `WRONG` there (`black_scholes`, `dgemm`, `fft`, `md5_sha1`, `monte_carlo_pi`,
  `radix_sort`, `random_massivo`), as `crc_adler_fnv` does (its FNV product loses its low bits there with any flags);
  `linked_list` crashes it (it frees nested arrays recursively, and the 1,000,000-deep chain overflows its stack), and
  `hash_resize` takes minutes (a hash's key deletion).

A language without a counterpart of a program has no version of it: C none of `async`, `async_generator`, `index`
(an index on disk would be a table engine) and `memory_footprint` (no collector); Rust none of `async` and
`async_generator` (no executor in its standard library), `tasks` and `work_stealing` (no pool of tasks); Java none of
`async`, `async_generator` and `pipeline_tasks`; Python none of `memory_footprint`; Harbour none of `generator`,
`tasks`, `channels`, `async`, `async_generator`, `higher_order_chain` (lazy enumerables), `pipeline_tasks`,
`work_stealing`, `shared_counter` (atomics) and `memory_footprint`. Go versions are not written yet.

`bench/compare.sh` (bash 4 and GNU `timeout`) and `bench/compare.ps1` (PowerShell 7.2) build each with every compiler or
interpreter found and print a Markdown table of the best time of each (`-n`/`-Runs` runs, CPython once; `-l`/`-Langs`
picks languages by key: `xbase`, `harbour`, `c`, `rust`, `csharp`, `java`, `go`, `node`, `pypy`, `python`): xbase from a
Release build (`build-release`, configured when missing; another build is refused), each benchmark target built again
and a failed build shown as `fail`; Harbour from the `.prg` themselves, or their forms in `bench/harbour/`, when it is
installed (`hbmk2 -gc3`, its C output compiled with `-O2` as `xbc -O2`, its class syntax header given on the command
line, its CA-Tools library linked for the math functions, its threaded runtime for the programs that run threads;
`bench/compat.ch` gives `ElapsedMs()` from `Date()` and `Seconds()` for it), C with `gcc -O2`, Rust with `rustc -C
opt-level=2`, C# in Release, Python with `-B` (no compiled files beside the sources). The programs run in
`<build>/compare/run`, under a memory limit (`compare.sh`: 16 GB of address space; C# with a 4 GB heap, whose
collector would otherwise reserve 8 GB of addresses) and the time limit (`-t`/`-Limit`). The README shows its table.
