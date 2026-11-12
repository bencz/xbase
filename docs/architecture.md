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

`xbc` lowers the syntax tree to xIR, its own intermediate representation ([xir.md](xir.md)), then builds an
`llvm::Module` in memory, runs LLVM's optimization pipeline, and emits the object file through a `TargetMachine`. No
textual IR is written (except with the debugging options `--dump-xir` and `--dump-ir`) and no external `llc` or
`clang` runs.

## Decisions

| Topic | Decision |
|---|---|
| Parser | Hand-written recursive descent. |
| Commands | Preprocessor rules in `std.ch`; the compiler knows the grammar only. |
| Typing | The language is untyped: any variable holds any value. Types exist only for FFI declarations and structures. |
| Strings | Immutable byte strings; `Len()` counts bytes. SET CODEPAGE (UTF-8 by default) says how the functions that read characters take them. |
| Names | Compiler `xbc`, runtime `xbrt`, C symbols `xb_*`, xBase functions `xbf_<NAME>` (runtime) and `xbp_<NAME>` (program). |
| Errors | Propagated through the thread context's pending flag; no `setjmp`/`longjmp`, no unwinder. |
| Memory | Tracing GC: precise mark-region with an Immix heap (blocks of lines, bump allocation per thread into free lines; size-class blocks for medium objects); full collections move objects out of fragmented blocks, pinning what the machine stack points to; no reference counting; two generations (sticky marks, aging, remembered set, write barrier). |
| Macros `&` | Compiled by the runtime's own expression compiler into code for a small stack machine, cached per text; no JIT. |
| Classes | Native in the compiler, Clipper-style syntax (`CLASS`/`ENDCLASS`, `METHOD ... CLASS`). *(planned)* |
| async/await | .NET task model on stackless resumable functions and an M:N work-stealing pool; races never corrupt memory. *(planned)* |

## Portability

- The compiler never hard-codes sizes or offsets. Runtime structures are described as LLVM types and laid out by the
  target `DataLayout`, which gives the same layout the C compiler gives the runtime for that target.
- Every program checks the ABI at startup (`xb_abi_check`): ABI version, item size, string header size, and the
  offsets of a codeblock's cells, a frame's slots, a context's frame chain and SET DECIMALS, an array's items, a
  reference's value and a hash's entries.
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

#### Where a C function comes from

```xbase
#ifdef __PLATFORM__WINDOWS
EXTERN C FUNCTION MessageBox( hWnd AS POINTER, cText AS WSTRING, cTitle AS WSTRING, nType AS UINT ) AS INT ;
   LIBRARY "user32.dll" NAME "MessageBoxW" STDCALL
#else
EXTERN C FUNCTION crc32( nCrc AS ULONG, pBuf AS POINTER, nLen AS UINT ) AS ULONG LIBRARY "libz.so.1"
#endif

LIBRARY "user32.dll"
   EXTERN C FUNCTION ...
ENDLIBRARY
```

- Without `LIBRARY`, the linker resolves the symbol (the C library, the runtime, libraries given to the link): a direct
  call.
- `LIBRARY "file"` loads the library at run time: the first call loads it and finds the symbol (through the platform
  layer, `runtime/src/platform/<platform>/`: load a library, find a symbol, free a library), and the compiler keeps the
  address in a global of the function for the next calls. A library or symbol that is missing is an xBase error (the
  library and symbol named) that `BEGIN SEQUENCE` can catch, not a program that fails to start. The block form
  `LIBRARY ... ENDLIBRARY` gives the library to every declaration inside it.
- `NAME "symbol"`: the C symbol, when it differs from the xBase name (the `A`/`W` variants of the Windows API,
  decorated names).
- A calling convention (`CDECL`, `STDCALL`, ...) is applied by LLVM; it matters on 32-bit Windows (the Windows API is
  `STDCALL`) and is the target's C convention everywhere else.
- The library file differs per platform: the predefined target names (`__PLATFORM__WINDOWS`, ...,
  [preprocessor.md](preprocessor.md#predefined-names)) choose it.
- A link dependency of a module (`#pragma link "ws2_32"`) goes into the object where the format has a directive for
  it (COFF), and otherwise to the linker through the driver.

### Libraries written in xBase

A library is written in xBase and distributed compiled, so that programs include its header and link it:

- `xbc --library <name> <sources> -o <file>` compiles the modules (no start function) into a static library (`.a`,
  `.lib`), written in-process by LLVM's archive writer, and generates `<name>.ch`: the library's public functions with
  their parameters (classes once they exist) and its link dependencies (`#pragma link`). A description file beside it
  gives the library's version, the runtime ABI it was compiled for, its include directories, its system libraries per
  platform and the xbase libraries it needs.
- A program uses it with `#include "<name>.ch"` and `--lib=<name>`; directories to search come from `-I`, `-L` and a
  configuration file beside the executable (never environment variables).
- Symbols: a library's public function is `xbl_<library>_<NAME>`, so libraries never clash when linked. In source, a
  name declared by one library is called as written; a name two included libraries declare must be qualified,
  `<library>::Name()`.
- Every module checks `XB_ABI_VERSION` when it registers (not only the program): a library compiled for another ABI is
  a clear error, not a crash. Until the ABI is stable, a library is recompiled with the compiler that uses it.
- Linking: `xbc` gets a link step (`-L<dir>`, `-l<lib>`, `--lib=<name>`, the runtime added by itself), run through the
  system's linker driver (`cc`/`clang`, `link.exe` or `lld-link`) with the dependencies the objects and the description
  files name. An embedded LLD (self-contained, linking for other targets) may replace it later.
- Shared libraries (`.so`, `.dll`, `.dylib`) come after the static ones: they need the runtime as a shared library too
  (one collector, context and symbol table per process), the public functions exported (Windows), and modules that
  register when a library is loaded after the program started.
- A project file (sources, include directories, libraries) lets `xbc <project>` compile and link a program in one
  step.
- The libraries xbase itself provides live in `libs/<name>/`, one directory each (its xBase and C sources, its
  description file, its tests), built by the same mechanism and installed beside the compiler, where `--lib=<name>`
  finds them. They hold what a program uses only by choice, above all what needs the system's libraries: the Window
  terminal driver, the native GUI backends (Win32, Cocoa, GTK 4) with their xBase classes ([terminal.md](terminal.md)),
  and later sockets, regular expressions, JSON, compression. What every program may use without asking and that needs
  nothing outside the C library stays in the runtime (the screen model, the events and the Stream, Dump, VT and
  Console terminal drivers). A library adds what it brings when its module registers (a terminal driver registers
  itself with the terminal layer, so linking the Window driver makes `//TERM:WINDOW` available).

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

*(Planned, [concurrency.md](concurrency.md))*: .NET's task model on stackless resumable functions (the generators'
machinery, [xir.md](xir.md#resumable-functions)), so that a suspended task costs only its frame; an M:N scheduler with
work stealing over every core, which hands a blocked worker's place to another thread; one heap, where objects
reachable by two flows are shared objects whose accesses the runtime makes safe; reactors over io_uring or epoll,
kqueue and IOCP; awaitable database operations for every driver.

## Roadmap

[roadmap.md](roadmap.md) lists what is missing compared with Clipper, the planned extensions, and the order of the
work; [coverage.md](coverage.md) tracks every command and function.
