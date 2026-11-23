# Projects and libraries

A project file builds a program or an xbase library in one step: `xbc <name>.xbprj`. A library is distributed
compiled: its archive, and its module file (`.xbm`), which tells `xbc` what the library declares, what it needs and
which build of it a program was compiled with. The runtime is a library like the others: its module file tells `xbc`
how a program links. The code is `compiler/src/driver/` (`Project.*`: the files' syntax, projects and module files;
`Link.*`: archives, module file lookup, the link step; `main.cpp`: the options).

## Project files

```
# A program with every setting
name = report
type = program
sources = report.prg lib/*.prg
includes = include
defines = "GREETING='hi there'" LEVEL=2
defines.windows = SIDE='windows'
libs = cli
libdirs = ../../build/libs
system.windows = ws2_32
options = -O3 -Wno-undeclared
```

One setting a line, `key = value`; a line that starts with `#` is a comment, and a UTF-8 byte order mark before the
first line is skipped. A list setting given again adds to it; `name` and `type` take the last one. A value is words
separated by blanks; double quotes keep blanks inside a word and are removed. Keys and qualifiers take any letter case.

| Setting | Meaning |
|---|---|
| `name` | the program's or the library's name: a file name without folders; default: the project file's name |
| `type` | `program` (default) or `library` |
| `sources` | the sources (a name with `*` or `?` stands for the files it matches, as on the command line) |
| `includes` | include folders (`-I`) |
| `defines` | `#define`s (`-D`): `NAME` or `NAME=value` |
| `libs` | xbase libraries the sources use (`--lib`) |
| `libdirs` | folders to look for their module files in (`-L`) |
| `system` | system libraries to link (`-l`): a name, or a linker argument when it starts with `-` |
| `options` | `xbc` options, before the command line's (which may override them) |

The paths of `sources`, `includes` and `libdirs` are from the project file's folder; those inside `options` are
`xbc`'s own and, as on the command line, from the current folder. A setting qualified with a platform family
(`defines.windows`, `system.unix`: `windows`, `linux`, `darwin`, `bsd`, `unix`, the families `__PLATFORM__` names,
`platformFamilies`) counts only for a target of that family, the target being `--target` or the host; `options` cannot
be qualified, since they may name the target. An unknown key (with any qualifier) or family, a line that is not a
setting, a quote left open, a project without sources, a `type` other than the two and a `name` that is not one file
name are errors that name the file and the line.

`xbc <project>.xbprj [options]` builds the project in the folder `--out-dir=<dir>` (default: the current one): a program
as `<name>` (`<name>.exe` for Windows), or with `-c` its object `<name>.o` only, a library as its archive and module
file (below). `-o` names the program, the object or the archive instead; the folder is created when missing. Sources
given beside the project file are added to its sources, and `--library=<name>` names a library project's library. A
program's or a library's object goes through a temporary file, removed afterwards; nothing is built incrementally: a
build system does that with `--depfile`, which names, besides the files the preprocessor read, the project file, the
libraries' module files and archives and, for a program it links, the runtime's.

The same works from the command line, without a project file, where `xbc` links the program its sources make unless `-c`
asks for the object only (`-o`, by default the first source's name with `.o`, beside it); object files given (`.o`,
`.obj`) are linked with the program, and objects alone are only linked:

| Option | Meaning |
|---|---|
| `-c` | write the object only, without linking the program |
| `--library=<name>` | compile a library: `-o` names its archive (default `lib<name>.a`, `<name>.lib` with the MSVC tools), and its module file `<name>.xbm` goes beside it |
| `--lib=<name>` | use the library `<name>` |
| `-L<dir>` | add a folder to look for module files in |
| `-l<name>` | link the system library `<name>` too |

## Libraries

A library is compiled in one run of `xbc` (all its modules in one object), with no program entry, into an archive of
that one object, written in the target's archive format by LLVM's archive writer. Its file names are in lower case
(`libcli.a`, `cli.xbm`), and its name is a name (letters, digits and `_`, not a digit first), upper-cased in its
symbols:

- A public function of the library is `xbx_LIBRARY$NAME` (`XB_XBLIB_FUNC_PREFIX` in `xbrt.h`), also where another
  module of the library calls it (`CodeGen::setLibrary`): neither a program's function nor another library's clashes
  with it at link time. A class function is a public function (`Greeter()`), so a library's classes are reached the
  same way; a class of the program inherits from a library's class by its name, as from any other module's. A call of
  the library to a name it neither defines nor declares goes, as in any module, to the program's function of that
  name (`xbp_NAME`).
- A program using the library gets a declaration of each of its public functions, as
  `EXTERN FUNCTION NAME( ... ) LIBRARY LIBRARY` would declare it (the attribute `LIBRARY`,
  [compiler.md](compiler.md#calls); a `NATIVE` function belongs to no library), made directly from the module file,
  before the module's own declarations: its calls, `REQUEST`s and function references go to the library's symbols. A
  function the program's module defines comes first, as for any declaration, so the program's own `Decorate()` is the
  program's while the library's calls to its `Decorate()` stay in the library; a call from another module of the
  program, which sees the declaration and not that definition, reaches the library's, as it would reach a runtime
  function of that name. Two libraries that declare one name are an error in the program, at the second module file's
  line; a library's public function named like a declared function (a runtime function that is not `RESERVED`, which
  a program may redefine) is an error when the library is built (`checkLibraryNames`), since its programs would get
  both declarations. Qualified names (`library::Name()`) are planned.
- The library's modules register as any module does ([runtime.md](runtime.md#program-startup)), with their level:
  1 + the highest level of the libraries the library uses (1 without any), the program's modules being above every
  library (`XB_LEVEL_PROGRAM`). `Do()`, macros and function references find a name's function of the highest level
  (the program's over a library's), and modules initialize from the lowest level up and end from the highest down,
  whatever order the system runs their constructors in (MinGW runs them backwards).
- Its stamp: the FNV-1a 64 of every file its sources read (themselves, `std.ch`, their includes) and of its defines.
  The library defines `xbx_LIBRARY$source$HASH`, and every object compiled with it references that symbol from a table
  the linker must resolve (`CodeGen::stampSources`, `keepReferences`). A program compiled against a library and linked
  with another build of it fails to link, naming the symbol; the reference also brings the library's object in when
  the program only inherits from its classes or calls it by name. The runtime's carried sources are stamped alike
  ([xir.md](xir.md#passes)).

## Module files

```
# The module file of the xbase library CLI, written by xbc (docs/projects.md).
format = 1
library = CLI
abi = 63
level = 1
target = x86_64-redhat-linux-gnu
archive = libcli.a
stamp = 6d0f2c1d5a7e9b13
uses = OTHER
system = m
function = COMMANDLINE
```

The same syntax as project files, without platform qualifiers. `format` (`XB_MODULE_FILE_FORMAT` in `xbrt.h`), `library`
and `archive` (a path from the module file's folder) are required; `abi` is the runtime ABI the library was compiled for
(`XB_ABI_VERSION`), `level` its modules' level, `target` its triple, `stamp` its stamp in hexadecimal, `uses` the
libraries it uses, `system` the system libraries it needs, and one `function` line for each public function. `library`,
`uses` and `function` hold names. The runtime's module file (`xbrt.xbm`, written by the build: `runtime/CMakeLists.txt`)
has `library = XBRT`, the archive, and how programs link: `linker` (the C compiler that built the runtime, a full path
in the build tree and its name in an install, or with `XB_BUNDLE_TOOLCHAIN` its path from the module file's folder in
the toolchain installed with the runtime; `link` for the MSVC tools), `style` (`gnu` or `msvc`), `flags` and `system`
(`m`, the threads library, `atomic` where needed; `shell32` on Windows). Its `flags` are the target's C flags
(`XB_TARGET_C_FLAGS`, and `--target` for a toolchain given a target); in the build tree also the toolchain's own
(`CMAKE_C_FLAGS`, `CMAKE_EXE_LINKER_FLAGS`, the sysroot), so that programs link like the build's (a sanitizer's
runtime). Without `target`, it is the runtime of `xbc`'s default target.

`--lib=<name>` looks for `<name>.xbm` (lower case), then `<name>/<name>.xbm`, in the `-L` folders and `libdirs` in
order, then in the install's modules folder, by the path from `xbc`'s folder (`XB_MODULES_FROM_BIN`: `../libs` in a
Windows package, `../lib/xbase` or `../lib64/xbase` in a system's install, where the runtime's archive is `../`), then,
when `xbc` holds the build's paths (`XB_SOURCE_INCLUDE`), the build tree's runtime (`XB_BUILD_MODULE_DIR`). A module
file of another format, of another library than the one asked for, compiled for another target (architecture,
operating system, MSVC or not) or for another ABI, with an unknown setting or an invalid value is an error naming it.
The libraries a library uses are looked up likewise; each library comes before the ones it uses in the link.

## The link step

Linking a program writes its object to a temporary file, then runs the linker the runtime's module file names (a name
found in the system's program path, a relative path from the module file's folder, `fromModuleFile` in `Link.cpp`): with
the `gnu` style `<linker> <flags> -o <program> <objects...> <archives...> <runtime archive> -l<system>...`, with `msvc`
`link /nologo /OUT:<program> <objects...> <archives...> <runtime archive> <system>.lib... <flags>`. The system libraries
are the libraries', the project's and the runtime's, each once, in the place of its last mention. A failed link reports
the linker's own messages and `xbc` fails. The MSVC tools are found only from a developer prompt, so the build does not
link programs with them (no library, project or sample test there).

## Tests and folders

- `libs/<name>/`: a library xbase provides ([libraries.md](libraries.md)): its project file `<name>.xbprj`, its
  sources and `tests/<test>.prg` with `<test>.out`. The build compiles it into `<build>/libs/<name>/`
  (`libs/CMakeLists.txt`, `xb_add_library`, which names the archive by CMake's convention for the toolchain and gives
  it to `xbc` with `-o`; `xb_build_project` in the top-level `CMakeLists.txt`) and installs it in the modules folder;
  its tests are programs linked with it at `-O0` and `-O2`.
- `samples/<name>/<name>.xbprj`: sample programs, built by the tests and run from `tests/samples/`.
- `tests/projects/`: project files that build and run, or must fail with a message, with two test libraries (`greet`,
  which uses `cli`, and `other`), and the scripts of the stamp and of the registration order
  ([testing.md](testing.md)).

Planned: bodies of a library that programs may run in place (its module file carrying their xIR, with the stamps above
guarding them, as the runtime's carried sources are), qualified names, C sources in a library, and shared libraries
([architecture.md](architecture.md#libraries-written-in-xbase)).
