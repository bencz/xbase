# xbase documentation

Find the task, read the doc it points to, then the code.

| Task | Read |
|---|---|
| Overall design, decisions, roadmap, planned features (classes, async/await, FFI, GC) | [architecture.md](architecture.md) |
| Directives, `#command`/`#translate` rules, `std.ch`, tokenization, comments, string literals | [preprocessor.md](preprocessor.md) |
| Grammar, keywords, name resolution, code generation, object emission, cross-compilation, driver options | [compiler.md](compiler.md) |
| Values (`xb_item`), frames, calling convention, error propagation, operators, the ABI shared with the compiler | [runtime.md](runtime.md) |
| Adding or running tests | [testing.md](testing.md) |

Setup, build and run commands are in the top-level [README.md](../README.md); agent rules are in
[AGENTS.md](../AGENTS.md).

## Source map

| Path | Contents |
|---|---|
| `compiler/src/Lexer.*` | tokenizer (one logical line at a time) |
| `compiler/src/Preprocessor.*` | directives, defines, rule matching and expansion |
| `compiler/src/Parser.*`, `AST.h` | recursive descent parser and syntax tree |
| `compiler/src/Sema.*` | scopes, slot assignment, structural checks |
| `compiler/src/CodeGen.*` | syntax tree to in-memory LLVM module |
| `compiler/src/Emit.*` | target machines, optimization pipeline, object emission |
| `compiler/src/main.cpp` | `xbc` driver |
| `include/std.ch` | standard commands, loaded before every source |
| `runtime/include/xbrt.h` | runtime ABI shared with the compiler |
| `runtime/src/` | runtime implementation |
| `tests/run`, `tests/pp` | execution and preprocessor tests |
