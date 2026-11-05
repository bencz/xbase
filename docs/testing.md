# Testing

Tests run through CTest (`ctest --test-dir build --output-on-failure`); `tests/CMakeLists.txt` discovers them.

| Kind | Files | Check |
|---|---|---|
| Execution | `tests/run/<name>.prg`, `<name>.out`, optional `<name>.rc` | The program is compiled with `xbc`, linked with `xbrt` and run; standard output must equal `<name>.out` and the exit code must equal `<name>.rc` (default 0). |
| Preprocessor | `tests/pp/<name>.prg`, `<name>.ppo` | `xbc -E` output must equal `<name>.ppo`. |
| Compile failure | `tests/fail/<name>.prg`, `<name>.err` | Compiling must fail, report the text in `<name>.err`, and report exactly one error. |
| Code generation | `xb_add_codegen_test` in `tests/CMakeLists.txt` | The IR `xbc --dump-ir` emits for a cross target must match a regular expression. |

Every test has a 30-second timeout, so a hang or a loop fails instead of stalling the run.

Include files used by a test sit next to it (for example `tests/run/commands.ch`). Execution tests are rebuilt when
`std.ch` or an included file changes (`xbc --depfile`).

## Expected outputs

Expected outputs follow Clipper behavior; `?` starts with a newline, so `.out` files usually begin with an empty line
and end without one. When Harbour is installed, an execution test's expected output is checked against it:

```sh
(ulimit -v 4000000; timeout 120 hbmk2 -static -gtcgi -q test.prg)
(ulimit -v 2000000; timeout 10 ./test | head -c 100000) > harbour.out
```

`-gtcgi` gives plain console output. A difference is either fixed or documented as a deliberate one in the matching
doc (for example the last digits of large doubles in [runtime.md](runtime.md#display)). Preprocessor output can be
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
