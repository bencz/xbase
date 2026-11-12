# Testing

Tests run through CTest (`ctest --test-dir build --output-on-failure`); `tests/CMakeLists.txt` discovers them.

| Kind | Files | Check |
|---|---|---|
| Execution | `tests/run/<name>.prg`, `<name>.out`, optional `<name>.rc`, `<name>.err`, `<name>.args`, `<name>.with/*.prg`, `<name>.d/*.prg`, `<name>.rt/*.prg` | The program is compiled with `xbc` (together with the modules in `<name>.with/`, in the same run), linked with `xbrt` (and with the modules in `<name>.d/`, each compiled on its own with `--no-entry`, and in `<name>.rt/`, compiled as modules of the runtime with `--runtime`) and run, with the command-line arguments in `<name>.args` (one per line; CTest cannot pass an empty one); standard output must equal `<name>.out`, the exit code must equal `<name>.rc` (default 0), and standard error must contain `<name>.err`. |
| Preprocessor | `tests/pp/<name>.prg`, `<name>.ppo` | `xbc -E` output must equal `<name>.ppo`. More, in `tests/CMakeLists.txt`: `pp.custom_std` (`-u<file>`), `pp.sources_pattern` (several sources and a file pattern, `tests/pp/glob`), and `pp.target_*` (the names predefined for each `--target`, `tests/pp/target`). |
| Compile failure | `tests/fail/<name>.prg`, `<name>.err`, optional `<name>.with/*.prg`, `<name>.args` | Compiling (with the modules in `<name>.with/`, and the `xbc` options in `<name>.args`, one per line) must fail, report the text in `<name>.err`, and report exactly one error. |
| xIR | `tests/xir/<name>.prg`, `<name>.xir` | The xIR `xbc --dump-xir` writes (after the xIR passes, at the default `-O2`) must equal `<name>.xir`. |
| Code generation | `xb_add_codegen_test` in `tests/CMakeLists.txt` | The IR `xbc --dump-ir` emits for a cross target must match a regular expression. |
| Declarations | `runtime.declarations` (`tests/declarations_test.cmake`) | `include/rtl.ch` declares exactly the `xbf_` functions defined in `runtime/src`. |
| Platform | `tests/platform/<name>.c` | A C program linked with `xbrt` exercises the runtime's own services (threads, locks, atomics, waiting on an address) under contention, and exits with 0 when every check passed (`check.h`). |

Every test has a 30-second timeout, so a hang or a loop fails instead of stalling the run. Execution tests run in the
build's `tests` directory; one that writes files names them after itself (`memvars_save_*.mem`), so tests running in
parallel do not share them, and each run overwrites them.

Include files used by a test sit next to it (for example `tests/run/commands.ch`). Execution tests are rebuilt when
`std.ch` or an included file changes (`xbc --depfile`).

A test that creates files (tables, `.mem` files, ...) runs in the build's test directory: its files' names start with
the test's name, so that tests running in parallel never share one, and the test deletes them at the end. A table
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

Every behavior change adds or extends a test. Changes to code generation or to the runtime ABI are also cross-compiled
for a 32-bit and a big-endian target, for example:

```sh
build/compiler/xbc --target=powerpc-linux-gnu tests/run/funcs.prg -o /tmp/claude/funcs-ppc.o
build/compiler/xbc --target=s390x-linux-gnu tests/run/funcs.prg -o /tmp/claude/funcs-s390x.o
```

Running big-endian programs needs a target sysroot and an emulator (for example `qemu-s390x`); this is not automated.

## Benchmarks

`bench/<name>.prg` are benchmark programs: recursive calls (`fib`), an integer loop (`loops`), arrays (`sieve`,
`matmul`), sorting in xBase (`qsort`) and with `ASort` (`asort`), strings (`strings`), the bit operators and string
building (`crc32`), array cells read and changed in place in deeply nested loops (`pi`, digits of pi from a tape-machine
program; `bench_pi <n>` prints n digits), double arithmetic (`float`, the Mandelbrot set) and integers mixed with
doubles (`mixed`: `i + 3.14`, `i / 4`, `Int()`, comparisons across both), and messages to objects of compiled classes
(`oop`: method calls, one through `Super`, variable reads and assignments), generators (`generator`: one feeding
another), the collector (`gc`: strings and small arrays of many sizes replaced at random among others that stay, the
heap's fragmentation; its memory is read with `/usr/bin/time`), and an order over a table (`index`: built, searched with
SEEK, walked, kept up to date as records are appended; its files are deleted at the end). Each one times its own work
with `ElapsedMs()`, checks its result against a value computed independently, and prints one line: `<name>
<milliseconds> ok|WRONG`. The `bench` target (not part of the default build or of `ctest`) compiles them with `-O2`
(`bench/CMakeLists.txt`, through the same `xb_compile_prg` as the tests) and runs `bench/run_bench.cmake`: every program
`XB_BENCH_REPEAT` times (a cache variable, default 3), the best time of each in a table, and a failure when one does not
run or reports `WRONG`. It warns when the build is not Release. Performance changes are measured with it, before and
after.
