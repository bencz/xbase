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
- Every program checks the ABI at startup (`xb_abi_check`): ABI version, item size, string header size, and the offsets
  of a codeblock's parameter count and cells, a frame's slots, a context's frame chain, SET DECIMALS, stack limit, lent
  arguments and allocation hole, an array's items and the items in its own allocation, a reference's value, a hash's
  entries, an object's variables, a string buffer's bytes and a method descriptor's direct entry.
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

A library is written in xBase and distributed compiled: its archive and its module file (`.xbm`), which a program's
compilation reads ([projects.md](projects.md), done):

- A project file (`<name>.xbprj`: sources, include folders, defines, libraries, system libraries, options, settings
  per platform) builds a program or a library in one step (`xbc <name>.xbprj`); `--library=<name>`, `--lib=<name>`,
  `-L` and `-l` do the same from the command line, where `xbc` links a program unless `-c` asks for its object.
- The module file gives the library's public functions (which a program gets declared, `EXTERN FUNCTION ... LIBRARY`),
  the runtime ABI and the target it was compiled for, the libraries and system libraries it needs, and the stamp of
  its sources, which every program compiled with it references, so that another build of the library fails to link.
  The runtime's module file tells `xbc` how to link (the C compiler driver or `link`, the target's flags, the system
  libraries).
- Symbols: a library's public function is `xbx_<LIBRARY>$<NAME>` (`XB_XBLIB_FUNC_PREFIX`), so it clashes neither with
  the program's functions nor with another library's.
- Linking: `xbc` runs the system's linker driver (`cc`/`clang`/`gcc`, or `link.exe`) with the object, the libraries'
  archives, the runtime's and the system libraries. An embedded LLD (self-contained, linking for other targets) may
  replace it later.
- The libraries xbase itself provides live in `libs/<name>/`, one directory each (its sources, its project file, its
  tests), built by the same mechanism and installed beside the runtime's module file, where `--lib=<name>` finds them
  ([libraries.md](libraries.md)). They hold what a program uses only by choice, above all what needs the system's
  libraries: the Window terminal driver, the native GUI backends (Win32, Cocoa, GTK 4) with their xBase classes
  ([terminal.md](terminal.md)), and later sockets, regular expressions, JSON, compression. What every program may use
  without asking and that needs nothing outside the C library stays in the runtime (the screen model, the events and
  the Stream, Dump, VT and Console terminal drivers). A library adds what it brings when its module registers (a
  terminal driver registers itself with the terminal layer, so linking the Window driver makes `//TERM:WINDOW`
  available).

Planned:

- Optimizing across a library's boundary. Inside a version bubble (a project and the libraries built with it) calls
  inline freely; across it, a library's module file carries the interface (classes, result types) and the serialized
  xIR of the bodies its author marks as stable, which programs may run in place, as they run the runtime's carried
  sources. Each body run in place is covered by the library's stamp: a mismatch is a link error, never a silent
  difference.
- Replaceable drivers (terminal drivers, database drivers): a program never assumes which one is active. Compiled code
  may call a driver directly only behind a guard on the active driver's identity, as it calls the runtime's classes of
  built-in types behind `xb_scalar_replaced` ([xir.md](xir.md#passes)). Linking a library is all it takes to change
  drivers: its module registers its drivers when it initializes, before the program's code runs; a driver registered
  under the name of one the runtime has replaces it, and one registered as the default becomes the one in use (the
  terminal driver chosen at the start, unless `//TERM:` names another; the database driver that `USE` takes without
  `VIA`). Drivers are found by name in the registry (`//TERM:`, `RddSetDefault()`, `USE ... VIA`). A driver is written
  in C, against an interface in `xbrt.h` (a structure of entry points, versioned with the ABI), or in xBase, as a class
  that inherits the driver's base class and that the runtime calls through an adapter. Today both kinds of driver are
  tables in the runtime (`term/drivers.c`, `rdd/area.c`); the terminal's interface moves into `xbrt.h` once the VT
  driver completes it, the database's at the SQL engines.
- Qualified names (`<library>::Name()`) for a name two libraries declare; C sources in a library (its `NATIVE`
  functions); shared libraries (`.so`, `.dll`, `.dylib`), which need the runtime as a shared library too (one
  collector, context and symbol table per process), the public functions exported (Windows), and modules that
  register when a library is loaded after the program started.

### Classes

`CLASS`, `INHERIT`, `DATA`/`VAR`, `METHOD`, `ACCESS`/`ASSIGN`, `Super` and constructors are compiled natively. Since the
language is untyped, `o:Msg()` dispatches by interned selector with an inline cache at the call site, which compiled
code performs in place on a hit; where the compiler knows the object's class (made in the function, or a parameter of
a copy typed for it), it resolves the message by the runtime's own rules (`xbclass.h`): a method call it may inline, a
variable read or written at its slot. Object layouts come from the target `DataLayout`.

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
