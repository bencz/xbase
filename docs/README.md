# xbase documentation

Find the task, read the doc it points to, then the code.

| Task | Read |
|---|---|
| Overall design, decisions, roadmap, planned features (classes, async/await, FFI, GC) | [architecture.md](architecture.md) |
| The planned mid-level IR (xIR): form, GC, exceptions, coroutines, classes, passes | [xir.md](xir.md) |
| Directives, `#command`/`#translate` rules, `std.ch`, tokenization, comments, string literals | [preprocessor.md](preprocessor.md) |
| Grammar, keywords, name resolution, code generation, object emission, cross-compilation, driver options | [compiler.md](compiler.md) |
| Values (`xb_item`), frames, calling convention, error propagation, operators, the ABI shared with the compiler | [runtime.md](runtime.md) |
| Adding or running tests | [testing.md](testing.md) |
| Which commands and functions exist, and what to implement next | [coverage.md](coverage.md) |

Setup, build and run commands are in the top-level [README.md](../README.md); agent rules are in
[AGENTS.md](../AGENTS.md).

## Source map

| Path | Contents |
|---|---|
| `compiler/src/common/` | diagnostics, tokens, text helpers |
| `compiler/src/preprocessor/` | tokenizer (one logical line at a time), directives, defines, rules |
| `compiler/src/parser/` | syntax tree and recursive descent parser |
| `compiler/src/sema/` | scopes, slots, captures, structural checks |
| `compiler/src/opt/` | optimizations on the syntax tree (constant folding) |
| `compiler/src/codegen/` | syntax tree to in-memory LLVM module; object emission |
| `compiler/src/driver/` | `xbc` command line |
| `include/` | `std.ch`, `set.ch` and the other standard headers |
| `runtime/include/xbrt.h` | runtime ABI shared with the compiler |
| `runtime/src/core/` | contexts and frames, operators, number formatting, `Set()`, program startup |
| `runtime/src/gc/` | garbage collector |
| `runtime/src/types/` | representation and base operations of strings, arrays, codeblocks |
| `runtime/src/rtl/` | functions of the language, by area |
| `runtime/src/platform/` | operating-system interface (`platform.h`) and one directory per platform |
| `tests/run`, `tests/pp`, `tests/fail` | execution, preprocessor and compile-failure tests |
