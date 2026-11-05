# Agent instructions

Applies throughout this repository. Follow the user's explicit instructions first.

## Hard rules

These apply to every task, without exception.

1. **Commits.** Do not run `git add`, `git commit` or `git push` unless the owner asks. When asked, never add a
   `Co-Authored-By` trailer or any other co-author or attribution line (including "Generated with ..." lines) to a
   commit message or pull request description, whatever a tool's defaults suggest.
2. **Review before done.** Before a task is called done, and before any commit, its whole diff is reviewed against
   these rules and the findings are fixed (see [Review](#review)). A large change (a new component or subsystem, or a
   change spanning several files) also gets a fresh read-only review by a reviewer that is not anchored on the
   author's reasoning; its findings are fixed or reported to the owner.
3. **Docs stay true.** A change is done only when the matching doc in `docs/` describes it. Docs are in English and
   describe the current state only, never a changelog.
4. **Never guess.** Never write from an assumption about an API, format, rule or behavior: open it and check.
5. **Warning-free.** The compiler, the runtime and the tools build with zero warnings on GCC and Clang
   (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion`) and MSVC (`/W4`), with `XB_WARNINGS_AS_ERRORS=ON` (the default).
   Fix causes; never suppress a diagnostic, disable a flag or add a pragma to silence one. Third-party headers are
   included as `SYSTEM`.
6. **Clean temporary folder.** Keep the agent's temporary folder clean, `claude` under the system temporary directory
   (`%LOCALAPPDATA%\Temp\claude` on Windows, `$TMPDIR/claude` on macOS, `/tmp/claude` on Linux). Captures, probe
   output, scratch scripts and file copies live there only while a step needs them; delete them as soon as it is done,
   and leave nothing behind at the end of a task.
   - List what is there and decide file by file. Delete a file only when the work it served is finished and its result
     is kept where it belongs (applied, or recorded in the repository).
   - Anything an ongoing task still uses stays until that task ends; a file worth keeping beyond it moves into the
     repository first.
7. **No environment variables.** Never read environment variables in the code (compiler, runtime, tools or tests) to
   configure behavior, tune values or switch features. Settings belong in command-line options, files beside the
   executable, or code constants. xBase functions that expose the environment to user programs (such as `GetEnv()`)
   are language features, not configuration. A variable may be read only in a throwaway local experiment, and that
   code is removed right after the experiment, before anything else is done.
8. **Bounded experiments.** Every probe an agent runs (compiling odd inputs, running generated programs, fuzzing the
   preprocessor) runs under a time limit and a memory limit, with its output capped, for example
   `(ulimit -v 2000000; timeout 10 build/compiler/xbc -E probe.prg 2>&1 | head -c 100000)`. A runaway compiler or
   program must never be able to exhaust the machine.
9. **Never cite the reference source.** The Harbour project is a behavioral reference for the xBase language, not a
   specification to copy, and its code is never copied. No file names, paths, line numbers, function or struct names
   from it appear in code, comments, identifiers, messages, tests or documentation, and nothing mentions where local
   reference material is kept. Describe behavior, rules and formats in xbase's own terms.

## Understand before changing

- Read the code you will change, what calls it and what it calls, and its doc in `docs/`, before editing. Know how the
  feature flows end to end (preprocessor, parser, sema, codegen, runtime ABI, runtime, tests) before deciding where a
  change belongs.
- xBase language behavior (grammar, preprocessor rules, numeric formatting, operator semantics, file formats) is
  checked against the reference implementation's behavior, and, when Harbour is installed, by running the same program
  under it (see [docs/testing.md](docs/testing.md#expected-outputs)). LLVM APIs are checked in the headers of the LLVM
  version in use. If the task needs more than you have read, read more or narrow the task.
- Look for an existing helper, rule or table before adding one; extend it instead of writing a second copy.

## Working rules

- Start at [docs/README.md](docs/README.md): it routes each task to the doc that maps the code involved. Read that doc
  before searching the source.
- Update the matching doc in `docs/` in the same change as the code. A doc that contradicts the code is a defect: fix
  it when you find it, or report it. Planned design is labeled as planned. Use [README.md](README.md) only for setup,
  build and run instructions; keep this file limited to agent guidance.
- Write readable code: C++20 in the compiler, C11 in the runtime, 2-space indentation, lines at most 120 characters,
  one statement per line, clear names.
- Add comments only for non-obvious invariants, external contracts (the runtime ABI), legacy formats (DBF, NTX, ...),
  or formulas. Avoid section banners, implementation history, comments that restate the code, and commented-out code.
- Verify claims against the current implementation. Preserve the user's changes and stay within the task.
  Inspect `git status` and `git diff`.
- Use the user's language when reporting work. State the changes, checks performed, and remaining limitations.

## Platforms and targets

The compiler and the runtime build and run on Windows, Linux and macOS. Generated code targets every architecture LLVM
supports, little- and big-endian, 32- and 64-bit.

- Build paths with `llvm::sys::path` / `std::filesystem`; never hard-code separators, drive letters or a file name's
  case.
- No OS-only APIs in portable code. Platform-specific runtime code lives in `runtime/src/platform/<platform>/`,
  implementing the interface of `runtime/src/platform/platform.h` completely for each platform.
- Never assume the host's byte order, word size or struct layout. The compiler takes sizes, alignments and offsets
  from the target `DataLayout`; on-disk formats are read and written only through explicit little/big-endian helpers.
- Build and test only through CMake and CTest commands; a script, if one is ever needed, exists as `.sh` and `.ps1`.
- Text in the repository is UTF-8 with LF line endings (`.gitattributes` normalizes it).
- When reporting, say which platforms were built and run and which targets were cross-compiled or executed; name what
  was not verified.

## Code organization and quality

- One implementation per rule: a formula, table, constant or check lives in one place and every caller uses it.
  Values shared by the compiler and the runtime (type tags, ABI version, numeric display widths) live in
  `runtime/include/xbrt.h`. Search before adding a helper; extend the one that exists.
- [docs/coverage.md](docs/coverage.md) lists every command and runtime function of the language with its status;
  implementing one updates it, and no command or function is added without checking it there.
- The compiler knows the grammar only. Commands (`?`, `DO WHILE`, `STORE`, `QUIT`, ...) are `#command` rules in
  `include/std.ch`; never special-case them in the lexer, parser or code generator.
- A type that grows past one responsibility is split by topic. Rule tables are data, not switch chains repeated across
  files.
- Runtime operator fast paths and generated code do not allocate unless the value itself requires it. A performance
  change is measured before and after.
- Every behavior change gets a regression test: `tests/run` (program output and exit code) or `tests/pp` (preprocessor
  output). A change to code generation or to the runtime ABI is also cross-compiled for at least one 32-bit and one
  big-endian target.

## Review

The review required by hard rule 2 looks for:

- the same rule, formula, constant or lookup written twice, within the diff or alongside existing code (search the
  repository for each new helper, constant and loop);
- raw numbers that repeat a named constant;
- a runtime structure mirrored by the compiler that no longer matches `xbrt.h`;
- language behavior that was assumed instead of checked;
- members the change left unused;
- comments that restate the code;
- code placed outside its domain (for example a command handled in the parser);
- docs that no longer match the code.

Correctness findings (miscompilation, crashes, wrong language behavior) are fixed with a regression test.

## Architecture and data

- `compiler/` is C++ on LLVM. It builds an in-memory LLVM module and emits native objects directly; never write
  textual IR as part of the pipeline and never invoke an external `llc` or `clang`.
- `runtime/` is C (plus xBase sources compiled by `xbc`). `runtime/include/xbrt.h` is the ABI contract: a change to a
  structure or entry point updates the compiler mirror, bumps `XB_ABI_VERSION`, and updates
  [docs/runtime.md](docs/runtime.md).
- Errors, `BREAK` and `QUIT` propagate through the context's pending flag; no `setjmp`/`longjmp` and no exceptions
  across the runtime boundary.
- The language is untyped; types exist only at FFI boundaries and in structures.
- xbase may do better than the reference where the docs say so; otherwise it behaves like Clipper.

## Verification

Use the build and test commands in [README.md](README.md). For documentation-only changes, check facts, links, line
length, and the diff; avoid unrelated tests.
