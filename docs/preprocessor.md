# Preprocessor

Code: `compiler/src/preprocessor/`, `include/std.ch`. Tests: `tests/pp`. `Lexer.*` tokenizes; the `Preprocessor` class
(`Preprocessor.h`) is implemented by topic: `Preprocessor.cpp` (files and includes, statement rewriting, output text),
`Directives.cpp` (directives and `#if` expressions), `RuleParse.cpp`, `RuleMatch.cpp` and `RuleResult.cpp` (defining,
matching and applying `#command`/`#translate` rules); `Internal.h` holds the preprocessor's limits and the token helpers
its files use.

The preprocessor owns tokenization. The parser only sees its output: statements separated by end-of-statement tokens.
Every command of the language (`?`, `DO WHILE`, `STORE`, `QUIT`, ...) is a rule in `include/std.ch`; the compiler has
no knowledge of commands.

## Tokenization

The lexer delivers one logical line at a time. A UTF-8 byte order mark at the start of a file is skipped.

- A `;` followed only by blanks or a line comment up to the newline continues the line. Any other `;` separates
  statements.
- Comments: `//` and `&&` to the end of the line; `/* ... */` (may span lines); `*` when it is the first token of a
  line; `NOTE` when it is the first token of a statement.
- Strings: `"..."`, `'...'` and `[...]`. A `[` starts a string unless it follows something that can be indexed (an
  identifier, a literal or a closing bracket), and only when a `]` follows on the same line.
- Logical literals and operators: `.T.` `.Y.` `.F.` `.N.` `.AND.` `.OR.` `.NOT.`, case-insensitive.
- Numbers: decimal with optional fraction, and `0x` hexadecimal. The lexer computes the display width of numeric
  literals (see [compiler.md](compiler.md#numeric-literals)).
- A line whose first token is `#` is a directive line. There `[` never starts a string, `\` escapes the next token, and
  `#` followed by `<` is a dumb-stringify result marker.
- Identifiers keep their spelling (for `#define` and stringification) plus an upper-cased form used for keywords and
  names.

## Directives

Directive names may be abbreviated to four letters, like dBASE keywords (`#ifde`, `#endi`, `#xtra`).

| Directive | Behavior |
|---|---|
| `#define NAME [value]` | Object-like macro. Names are case-sensitive. Redefining with a different body warns. |
| `#define NAME(a, b) body` | Function-like when `(` follows the name with no space. A use with another number of arguments is not a macro call and is left as written. |
| `#undef NAME` | Removes a define. |
| `#ifdef`, `#ifndef`, `#if`, `#elif`, `#else`, `#endif` | Conditional compilation; nesting is tracked inside skipped regions. |
| `#include "file"` | An absolute path is used as is; otherwise searched in the including file's directory, then `-I` directories, then the default include directories. |
| `#command`, `#xcommand`, `#translate`, `#xtranslate` | Rules (below). |
| `#uncommand`, `#xuncommand`, `#untranslate`, `#xuntranslate` | Remove the latest rule with the same pattern. |
| `#error`, `#warning`, `#stdout` | Report an error, a warning, or print the text. |
| `#pragma` | Accepted and ignored. |

`#if` expressions support integers, `.T.`/`.F.`, `defined(NAME)`, `!`/`.NOT.`, unary `+`/`-`, `*` `/` `%` `+` `-`,
comparisons (`=` `==` `!=` `<>` `#` `<` `<=` `>` `>=`), `.AND.` and `.OR.`, on 64-bit integers. Defines are expanded
first; names that remain are 0. Overflow, division by zero, a number outside the 64-bit range, or more than 256
nested operators make the expression invalid.

Include files nest up to 64 levels.

## Rules

A rule is `#command <pattern> => <result>`; the pattern must start with a literal token.

- `#command` matches a whole statement; `#translate` matches anywhere inside a statement.
- Keywords compare case-insensitively. `#command`/`#translate` accept dBASE abbreviations: at least four letters of a
  pattern word longer than four. The `#x...` variants require the full word. The same rule applies to pattern
  literals, restricted-marker words and stop tokens.
- `!=` and `<>` are the same token; `#` is a different one.
- The latest defined rule is tried first.

### Match markers

| Marker | Matches |
|---|---|
| `<x>` | One expression. |
| `<x,...>` | A comma-separated list of expressions; empty elements are allowed. |
| `<x: w1, w2>` | One of the listed words (each may be several tokens). `&` in the list accepts a macro (`&var`, `&var.`, `&( expr )`). An empty word, from a leading or doubled comma, always matches; a trailing comma adds none. |
| `<*x*>` | The rest of the statement; at least one token. |
| `<(x)>` | An extended expression: a parenthesized expression, or a run of adjacent tokens with no blanks between them (`customer.dbf`, `&cFile`). |
| `<!x!>` | One identifier. |
| `[ ... ]` | An optional clause. Consecutive optional clauses match in any order and any number of times. |

An expression ends at a comma or closing bracket at nesting level 0, where two operands meet with no operator between
them (`a b`, `x TO`), or at the token that follows the marker in the pattern. Inside an optional clause, the token that
follows the run of optional clauses is the stop token. A marker outside optional clauses may match only once.

### Result markers

| Marker | Produces |
|---|---|
| `<x>` | The matched tokens. |
| `#<x>` | The matched text as one string; `""` when nothing matched. |
| `<"x">` | Each comma-separated element as a string. A macro element becomes its variable (`&cVar` gives `cVar`) and `&( expr )` gives `( expr )`. |
| `<(x)>` | Like `<"x">`, and an element that starts with a string or with `(` is kept as is. |
| `<{x}>` | Each comma-separated element as a codeblock `{|| element }`; existing codeblocks are kept. |
| `<.x.>` | `.T.` when the marker matched, `.F.` otherwise. |
| `<-x->` | Nothing (the marker is matched and dropped). |
| `[ ... ]` | Repeated once per match of the markers it contains directly, never when none matched. A marker that matched once supplies its value to every repetition. |

The element rules apply to any marker, not only to lists: a match is split at its top-level commas.

A result may contain `;`, which splits it into several statements.

### Rescanning

Each statement is processed in this order: expand defines, apply translates from left to right, apply a command; then
again, until nothing changes. A statement that expands to nothing is dropped. A rewrite that produces the same tokens
counts as no change.

Everything one source statement (one `;`-separated statement of a line) expands into, including the statements a `;`
in a result splits off, shares one set of limits: 4096 rewrites, 65536 tokens per statement, and 64 levels of
splitting. Exceeding a limit reports one error for the source statement (circular rules) and drops the rest of its
expansion. A rule whose result starts with a word its own pattern accepts (`#command SHOW <x> => Show(<x>)`) is
circular, as in Clipper.

## std.ch

`include/std.ch` is processed before every source file. `-u` compiles without it, `-u<file>` uses another file (a
path relative to the current directory, or a name found in the include directories). It
lists only commands whose runtime support exists and grows with the runtime:

| Command | Becomes |
|---|---|
| `DO WHILE <exp>` | `while <exp>` |
| `END <x>`, `ENDDO <*x*>`, `ENDIF <*x*>`, `ENDCASE <*x*>` | `end`, `enddo`, `endif`, `endcase` |
| `ENDFOR [<*x*>]`, `NEXT <v> [TO <x>] [STEP <s>]` | `next` |
| `STORE <v> TO <v1> [, <vN>]` | `<v1> := [ <vN> := ] <v>` |
| `? [<list,...>]`, `?? [<list,...>]` | `QOut( <list> )`, `QQOut( <list> )` |
| `SET DECIMALS TO [<x>]` | `Set( _SET_DECIMALS, <x> )` (0 without a value) |
| `SET EXACT ON \| OFF \| &var`, `SET EXACT (<x>)` | `Set( _SET_EXACT, ... )` |
| `SET DATE [TO] AMERICAN \| ANSI \| BRITISH \| FRENCH \| GERMAN \| ITALIAN \| JAPANESE \| USA` | `Set( _SET_DATEFORMAT, ... )`, the picture with `yyyy` or `yy` as `SET CENTURY` says (`_DFSET`) |
| `SET DATE FORMAT [TO] <f>` | `Set( _SET_DATEFORMAT, <f> )` |
| `SET CENTURY ON \| OFF \| &var`, `SET CENTURY (<x>)` | `__SetCentury( ... )` |
| `SET EPOCH TO <y>` | `Set( _SET_EPOCH, <y> )` |
| `QUIT`, `CANCEL` | `__Quit()` |

`std.ch` includes `include/set.ch`, which defines the `_SET_*` numbers for `Set()`. It contains only `#define` lines
and C comments, so the runtime includes the same file and the numbers are defined once.

## Inspecting the output

`xbc -E file.prg` prints the preprocessed statements, one per line; `-p` also writes them to `file.ppo` and continues
compiling. Strings are written with a delimiter they do not contain; a string that contains `"`, `'` and `]` is written
as a concatenation.
