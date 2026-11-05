# Architecture

xbase is a new compiler for the xBase language: the Clipper 5.x dialect plus extensions. It compiles straight to native
code through LLVM. There is no p-code and no virtual machine.

```
            xbc (C++)                                              xbrt (C + xBase)
  .prg -> preprocessor -> parser -> sema -> codegen -> object ---+
              ^                                 |                 +--> linker --> executable
           std.ch                in-memory LLVM module  libxbrt --+
```

| Component | Language | Path | Doc |
|---|---|---|---|
| Compiler `xbc` | C++20 + LLVM | `compiler/` | [compiler.md](compiler.md), [preprocessor.md](preprocessor.md) |
| Runtime `xbrt` | C11 (xBase parts planned) | `runtime/` | [runtime.md](runtime.md) |
| Standard header | xBase preprocessor rules | `include/std.ch` | [preprocessor.md](preprocessor.md) |

`xbc` builds an `llvm::Module` in memory, runs LLVM's optimization pipeline, and emits the object file through a
`TargetMachine`. No textual IR is written (except with the debugging option `--dump-ir`) and no external `llc` or
`clang` runs.

## Decisions

| Topic | Decision |
|---|---|
| Parser | Hand-written recursive descent. |
| Commands | Preprocessor rules in `std.ch`; the compiler knows the grammar only. |
| Typing | The language is untyped: any variable holds any value. Types exist only for FFI declarations and structures. |
| Strings | Immutable byte strings with a codepage; UTF-8 is the default codepage. `Len()` counts bytes. |
| Names | Compiler `xbc`, runtime `xbrt`, C symbols `xb_*`, xBase functions `xbf_<NAME>`. |
| Errors | Propagated through the task context's pending flag; no `setjmp`/`longjmp`, no unwinder. |
| Memory | Tracing GC (mark-sweep, non-moving, per-thread allocation); no reference counting. *(planned)* |
| Macros `&` | Evaluated by an expression interpreter in the runtime; no JIT. *(planned)* |
| Classes | Native in the compiler, Clipper-style syntax (`CLASS`/`ENDCLASS`, `METHOD ... CLASS`). *(planned)* |
| async/await | .NET task model on stackless coroutines and a thread pool. *(planned)* |

## Portability

- The compiler never hard-codes sizes or offsets. Runtime structures are described as LLVM types and laid out by the
  target `DataLayout`, which gives the same layout the C compiler gives the runtime for that target.
- Every program checks the ABI at startup (`xb_abi_check`): ABI version, item size and string header size.
- On-disk formats (DBF and NTX are little-endian; CDX keys are big-endian) are read and written only through explicit
  byte-order helpers, never by copying structures. *(planned)*
- Objects have been produced and their layout inspected for x86-64, i686, AArch64, ARMv7, RISC-V 64, LoongArch64,
  s390x, PowerPC 64 and 32, MIPS, SPARC v9, wasm32, Windows COFF and Mach-O. Execution has been verified on x86-64 Linux
  only; the build has been verified with GCC and Clang on Linux (Windows/MSVC and macOS are not verified yet).

## Planned design

### Runtime declarations

The runtime is described to the compiler by declaration files written in xBase, loaded automatically, instead of
hand-written C wrappers:

```xbase
DECLARE FUNCTION Str( n, nLen, nDec ) EXTERNAL C "xb_str" PURE
EXTERN C FUNCTION send( fd AS INT, buf AS POINTER, len AS SIZE_T, flags AS INT ) AS SSIZE_T
STRUCTURE sockaddr_in
   MEMBER sin_family AS UINT16
ENDSTRUCTURE
```

- The compiler generates the item-to-C conversions at the call site.
- `PURE`/`INTRINSIC` allow constant folding and inlining.
- For dynamic calls (`&cFunc`, `Do()`), each module emits a name-to-thunk table; thunks for C functions are generated
  from the declarations as well.

### Classes

`CLASS`, `INHERIT`, `DATA`/`VAR`, `METHOD`, `ACCESS`/`ASSIGN`, `Super` and constructors are compiled natively. Since the
language is untyped, `o:Msg()` dispatches by interned selector with an inline cache at the call site; object layouts
come from the target `DataLayout`.

### async/await

```xbase
ASYNC FUNCTION FetchOrders( cUrl )
   LOCAL cJson := AWAIT HttpGet( cUrl )
   RETURN ParseOrders( cJson )
```

- `ASYNC FUNCTION`/`ASYNC METHOD` return a `Task` and run synchronously until the first `AWAIT` of an incomplete task;
  a task that completes synchronously allocates no frame.
- Runtime library: `Task`, `Task.Run`, `Task.Delay`, `WhenAll`, `WhenAny`, `CancellationToken`, `TaskCompletionSource`.
- Stackless coroutines (LLVM coroutine intrinsics, target-independent lowering): a suspended task costs only its frame,
  so hundreds of thousands of concurrent requests fit in memory.
- Continuations run on a work-stealing thread pool; a task may resume on another thread.
- Per-task state (`PRIVATE`/`PUBLIC`, current work area, `SET` values, `ErrorBlock`) lives in the task context and
  flows with the task.
- I/O: io_uring with an epoll fallback on Linux, kqueue on BSD/macOS, IOCP on Windows. Work without real asynchronous
  I/O (DBF/NTX, locks) runs on a separate blocking pool.
- Arrays and hashes shared between threads: races are the program's responsibility, but the runtime never corrupts
  memory (a light per-object lock guards structural operations such as `AAdd`, `ASize`, hash insertion).

## Roadmap

1. Compiler gaps: constant folding (it changes results, see [compiler.md](compiler.md#code-generation)), references
   (`@var`, `DO ... WITH` by reference). Preprocessor gaps: macro tokens (`&var.suffix`), `__FILE__`/`__LINE__`,
   `#pragma`, `e"..."` strings, date literals, precompiled `std.ch` rules.
2. Core language: arrays, codeblocks, `STATIC`, `MEMVAR`/`PRIVATE`/`PUBLIC`, `FIELD`/aliases, `BEGIN SEQUENCE`,
   `ErrorBlock`, `DO CASE`, `SWITCH`, `FOR EACH`, hashes.
3. Tracing GC, dynamic symbol table, runtime macro evaluation.
4. Runtime declarations, FFI, runtime parts written in xBase.
5. Native classes.
6. async/await, thread pool, asynchronous I/O.
7. RDD (DBF, NTX, CDX, FPT) and terminal (GT) layer.
8. Embedded linker (LLD as a library), debug information (DWARF/CodeView).
