# Databases

The work areas and the database drivers (RDDs, Clipper's replaceable database drivers): tables, memos, indexes, locking,
and the commands and functions over them. The first two steps are done: work areas, the driver interface, `DBF` tables
with `DBT` memos, the compiler's field references and the commands over them. The rest of this document marked
*(planned)* is the plan the work follows.

Goals: Clipper's behavior, and files that Clipper programs and the reference read and write at the same time as xbase
programs (the same bytes on disk, the same locks), on every platform and byte order; drivers that can be added without
touching the rest (SQL engines later), each as a library when it needs one.

## Work areas

`runtime/src/rdd/` (portable C; `rdd.h` declares what the files share):

- `area.c`: each task's table of work areas (`xb_ctx.areas`, by number from 1, growing as needed; `xb_ctx.areaCurrent`
  starts at 1), aliases (upper case; any letter case finds them), and the navigation the language sees on top of a
  driver's raw records: records that are deleted (under `SET DELETED`, `Set( _SET_DELETED )`) or that the filter
  rejects are skipped; `BOF` and `EOF`; going to a record that is not there (0, past the last) leaves the blank record
  after the last current with both `Bof()` and `Eof()` true; a record that cannot be read leaves it there too. Any
  move, and `dbAppend`, makes `Found()` false. The filter, its text and the last `LOCATE`'s condition are roots of the
  collector (`xb_areas_mark`). Every table is written and closed when the program ends (`xb_program_run`), whatever
  ended it; an area is free before the error of a close that failed is raised.
- Code that runs blocks over an area (navigation through a filter, the scope walk, `LOCATE`, `CONTINUE`, `dbEval`)
  pins it (`xb_area_pin`): a block that closes the area, or closes it and opens another table there, ends the command
  where it is, and the area's memory lives until the command lets go of it.
- `dbcmd.c`: the database functions ([coverage.md](coverage.md) lists them), and `__dbPack`, `__dbZap`, `__dbLocate`,
  `__dbContinue`, which the commands use. On an area with no table, the functions that only tell something give a
  neutral value (`RecNo()` 0, `Eof()` and `Bof()` `.T.`, `FieldGet()` NIL, `Alias()` `""`, `FieldPut()` nothing); those
  that move or change are `DBCMD/2001` `Workarea not in use` (generic code `EG_NOTABLE`).
  `dbEval( bBlock, [bFor], [bWhile], [nNext], [nRecord], [lRest] )` and the commands with a scope (`DELETE`, `RECALL`,
  `REPLACE`, `LOCATE`, `COUNT`, `SUM`) share one scope walk: from the top, unless `WHILE`, `NEXT n` or `REST` start it
  at the current record or `RECORD n` names the one record; `WHILE` stops at the first record that fails it, `NEXT n`
  on the last record it took (none for `n` below 1).
- A table opened `READONLY` takes no write: `FieldPut`, assignments to fields, `dbDelete`, `dbRecall`, `dbAppend`,
  `__dbPack` and `__dbZap` are `DBFNTX/1025` `Write not allowed` (generic code `EG_READONLY`), and nothing changes.
- `field.c`: the field references of compiled code (below) and `__AreaEval( xArea, bBlock )`, which evaluates
  `alias->( expr )`.
- `dbf.c`: the `DBF` driver, with its memos in `dbt.c` ([Files](#files)).

Names: `dbUseArea` and `dbCreate` take the table's name with `.dbf` added when it has no extension, and its alias is
the file name without directories and extension unless one is given. Once the table is open (an `Open error` comes
first), the alias, trailing blanks dropped, must be a name (a letter or `_`, then letters, digits and `_`), else
`DBCMD/1010` `Illegal characters in alias` (`EG_BADALIAS`); one another area has, or a letter that names an area (`A`
to `K`, and `M`, the memory variables), is `DBCMD/1011` `Alias already in use` (`EG_DUPALIAS`); either closes the
table again. `USE ... NEW` and `dbUseArea( .T., ... )` select the lowest free area, which stays current even when the
table cannot be opened; `dbCreate( ..., lNewArea )` leaves the area current before current then. `SELECT`,
`dbSelectArea`, `Select( cName )` and `alias->` take a number (`SELECT 0`: the lowest free area) or a string read as
`xb_area_named` reads it: digits after blanks are a number (what follows them is ignored), else, trailing blanks aside,
one of the letters `A` to `K` is area 1 to 11, else an alias; any other name is `BASE/1002` `Alias does not exist`,
which may be retried (`Select()` gives 0).

Errors carry the subsystem of what raised them (`xb_error_of` in [runtime.md](runtime.md#errors-and-pending-requests)):
`DBCMD` for the functions, the driver's name (`DBFNTX`) for the table. A table that cannot be opened is `DBFNTX/1001`
`Open error` with the file name and its `FError()` code (its memo file, `DBFNTX/1002` with the memo file's name), which
may be retried (the default leaves the area free); writing a value of the wrong type to a field is `DBFNTX/1020` `Data
type error`, a number too wide for it `DBFNTX/1021` `Data width error` (the field gets stars, as the reference writes),
both with the value in `args` and a default that leaves the field as it is; an assignment of NIL is a wrong type, while
`FieldPut( n, NIL )` writes nothing. `dbCreate` with a structure that is not one (field names missing or repeated), or
one the driver cannot make, is `DBCMD/1014`.

Where xbase does not follow the reference (each checked by `tests/run/rdd_checks.prg`, `rdd_select.prg`, `rdd_memo.prg`,
`rdd_macro.prg` and `rdd_lifetime.prg`):

- `dbCreate` takes only the types the driver writes (`C`, `N`, `D`, `L`, `M`) and at least one field; the reference
  creates a table from an empty structure or one with a type it cannot use. A memo field is always 10 bytes, the
  block number as text (the reference also makes one of 4 bytes, a binary number, when asked).
- A memo's value stays `M` for `ValType()` in every copy ([runtime.md](runtime.md#values)); the reference makes it `C`
  when it is stored in a variable, an element or a hash, returned, or taken by `FOR EACH`, and keeps it elsewhere.
- PACK ends each memo it moves with two `0x1A`, as every memo is written (the reference writes one there), and a value
  of the wrong type for a memo field names the field, as for the other types (the reference names the `.dbt`).
- A `.dbt` shorter than its header's 4 bytes is an `Open error` (the reference opens it); one whose header gives a
  first free block below the end of the file gets new memos after the last one (the reference writes over those
  there); `DATA.DBT` serves `DATA.DBF` when there is no `DATA.dbt`.
- `__dbLocate` on an area with no table is `2001`, as every other function; the reference's subcode is `35`.
- The same table opened twice under its alias is `Alias already in use` (with locks, step 4, a table open
  exclusively will be an `Open error`, as in the reference).
- `dbSelectArea( "M" )` is `Alias does not exist`; the reference selects an area past the last.
- A table with a field of no bytes is an `Open error` (general failure, 31), as one with a date that is not 8 bytes
  is in both.
- `alias->( expr )` brings the work area current before back also when the expression fails; an alias that names no
  area is `BASE/1002` also when it is an expression's value (the reference evaluates `( expr )->( ... )` in the current
  area then); an alias expression is evaluated once in `+=` and `++` (the reference evaluates it twice in a macro).

## Field references

The compiler resolves names; the runtime finds fields by name when the code runs.

- `alias->name` is a field of the work area the alias names; `FIELD->name` (and `_FIELD->name`) one of the current
  area; `FIELD->alias->name` one of the alias's. `( expr )->name` takes the area from the expression's value (an
  alias, or a number from 1), and so does a macro before `->` (`&cAlias->name`, `&cAlias.->name`, `&( expr )->name`:
  the text is the alias, not compiled). `M->name` and `MEMVAR->name` are memory variables.
- `alias->( expr, ... )` (also `( expr )->( ... )` and `&cAlias->( ... )`) evaluates the expressions with that area
  current and gives the last one's value: the compiler makes it `__AreaEval( alias, {|| expr, ... } )`.
- `FIELD name, ... [IN alias]` declares fields as `MEMVAR` declares memory variables: before the first function for
  the whole file, or in a function; the names are then fields of the alias's area (the current one without `IN`).
- An undeclared name (no `LOCAL`, `STATIC`, parameter, `MEMVAR` or `FIELD`) is read as a field of the current work
  area when it has one, else as a memory variable (Clipper's rule); assigning it assigns the memory variable.
- A reference that finds no field is `BASE/1003` `Variable does not exist` (the field's name), one whose alias names
  no area `BASE/1002` (the alias); both may be retried. A field written while the area is at its end (`Eof()`) is
  ignored.

Compiled code calls `xb_field_get`, `xb_field_set` and `xb_mv_field` ([runtime.md](runtime.md#work-areas)), each
reference with a cache of its own (`xb_field_cache`): the field's position in the table it found it in, which a table
opened later (each one has a different `generation`) makes it look up again. The macro compiler reads the same forms
([runtime.md](runtime.md#macro-operator)).

## Drivers

A driver is a table of operations on the raw table (`xb_rdd` in `rdd.h`; an open failure names the file that failed,
the table or a file of its own): check a structure (the types, lengths and
sizes it can make; `dbCreate` checks the names), create, open, close, the record count, go
to a record (the blank one after the last outside the table), flush (the record, and on `COMMIT` the header), append,
read and write a field, the deletion flag, pack, zap, and the header's sizes and date. Operations that fail give the
`FError()` code of the failure; the work area raises the error. The names `DBF` and `DBFNTX` (the default, as in
Clipper; `RddSetDefault()` and `USE ... VIA` choose) both give the `DBF` driver with `DBT` memos, whose name is
`DBFNTX` until it has indexes.

*(Planned)*: the order operations and locks in the table; drivers that inherit what they do not change from the
driver they extend; `DBFNTX` (`DBF`, `DBT` memos and `NTX` indexes), `DBFCDX` (`DBF`, `FPT` memos and `CDX` compound
indexes), `SDF` and `DELIM` (text files, for `COPY TO` and `APPEND FROM`); `SET DB ENGINE TO <name>` (xbase's own
command, over `RddSetDefault()`); later, as libraries in `libs/`, SQL engines (SQLite, PostgreSQL), among them an
SQLite driver that behaves as `DBF` does (the row id as `RecNo()`, navigation in the order of an index, `Deleted()` a
column). The language's other commands come with them: `SEEK`, `INDEX ON`, `SET INDEX`, `SET ORDER`, `REINDEX`,
`SET RELATION`, `SET EXCLUSIVE`, `SET SOFTSEEK`, `SET UNIQUE`, `COPY`, `APPEND FROM`, `SORT`, `TOTAL`, `AVERAGE`,
`UPDATE`, `JOIN`, `LIST`, `DISPLAY`.

## Files

Every binary field of a file is read and written through the byte-order helpers (`core/bytes.h`): DBF, DBT and NTX
are little-endian; FPT headers and CDX keys and some of their fields are big-endian. No structure is ever copied to or
from a file whole, so every host, little- or big-endian, 32- or 64-bit, writes the same bytes.
- **DBF** (`dbf.c`): the header (version `0x03`, date of the last update as year since 1900, month and day, record
  count, header and record lengths, flags, the codepage byte), field descriptors (name, type, length, decimals; a
  terminator `0x0D` and a `0x00`), records with their deletion flag (`*` deleted, a blank otherwise), the end mark
  `0x1A`. A table whose descriptors cannot be read (an unknown type, a field of no bytes, a date that is not 8 bytes,
  fields past the record's length) is an `Open error` with code 31 (general failure). Field types: Clipper's `C` (up to
  64 KB, a length past 255 with its high byte in the decimals), `N` (read and written as `Val()` and `Str()` do,
  `xb_num_parse` and `xb_num_to_field`; stars when the number is too wide), `D` (`YYYYMMDD`, blanks for the empty date),
  `L` (`T` or `F`; `T`, `t`, `Y`, `y` read as true) and `M` (10 bytes: the number of the memo's first block in the
  `.dbt`, right-aligned; blanks for none). A table with memo fields has version `0x83` and a `.dbt` of the same name.
  The record is written when another becomes current or the table closes, the header (record count, date) on `COMMIT`
  and at closing when the table changed. *(Planned)*: the common extended types (`I`, `B`, `Y`, `T`, `@`, `+`, `V`, `Q`)
  as the reference reads them.
- **DBT** (`dbt.c`, dBASE III memos): 512-byte blocks; block 0 is the header, the number of the first free block in
  its first 4 bytes and zeros after; each memo starts a block, its text followed by two `0x1A` (reading stops at the
  first `0x1A`, so a memo cannot hold one), and the file ends after the last memo (the next one starts at the next
  block, zeros between). A value is never rewritten in place: writing a memo field writes a new memo after the last
  (an empty value is none), as the reference does; the first free block is never taken below the end of the file,
  whatever the header says (the reference then writes over the memos there). PACK gives the records it keeps their
  memos at the start, in record order, dropping the others, in steps that each leave every record on a whole memo, so
  that a failure (a full disk) loses nothing: copies of the memos after every older one, the records moved to the
  front with their copies' blocks, the copies moved to block 1 when they fit before themselves (else they stay), the
  records renumbered, the older memos cut. ZAP leaves the header only, its first free block 1. The memo file is the
  table's name with `.dbt` (also beside `DATA.DBF`, as the reference names it); opening finds `DATA.DBT` too when
  there is no `DATA.dbt`, as tables copied from systems that write names in upper case have it. A table with memo
  fields and another version byte (dBASE IV, FoxPro: memos of another format) is `DBFNTX/1012` `Corruption
  detected`.
- **FPT** (FoxPro memos): the block size in the header, each block with its type and length; big-endian.
- **NTX**: 1024-byte pages, the key expression, size and decimals, the unique flag and the condition; keys as the
  ordering of their values demands (numbers in a form that sorts as text, dates as `DToS()`).
- **CDX**: 512-byte pages; a compound index of tags, each a B-tree whose leaf pages keep keys compressed (the record
  number, duplicate and trailing counts packed in bits); numbers and dates as big-endian sortable doubles.

Every format is checked against the reference's files: what one writes, the other reads, and after the same operations
the files are the same bytes (but the date of the last update). `tests/run/rdd_format.prg` keeps a table the reference
wrote and checks both ways for `DBF`.

## Sharing and locks (planned)

Programs share tables as Clipper's do: `SHARED` and `EXCLUSIVE` opening, record and file locks through the platform
layer (byte-range locks on POSIX systems and on Windows, at the offsets of each driver's lock scheme, the same as the
reference's, so programs of both lock each other out correctly), the record count and the header read again when the
table may have changed, `NetErr()` after a failed `USE` or `APPEND BLANK`, and `COMMIT`. The lock offsets are taken
from the reference's behavior (observed in the locks it sets), never from its code.

## Text and codepages

A table's bytes are kept as they are: the functions read them as `SET CODEPAGE` says. A codepage per table (the
header's codepage byte, a `USE ... CODEPAGE` clause) that converts on reading and writing comes later.

## Queries (planned)

The driver interface is navigational, as Clipper's. A later step lets a driver run a query itself: an `Enumerable`
over a table (`Table( "CLIENTES" ):Where( {| r | r:CIDADE == "SP" } ):OrderBy( ... )`) goes record by record on any
driver, and an SQL driver may run it in the database when the compiler keeps the expression of the blocks given to a
query (as data a driver can translate), falling back to the record-by-record way for what it cannot translate.

## Tests

- Run tests create, change and read tables, memos and indexes (in a directory of their own, deleted at the end), with
  the expected results taken from the reference running the same programs.
- Cross tests: files the reference wrote (kept in the tests) read by xbase, and files xbase writes compared with the
  reference's bytes.
- Big-endian: the formats are cross-compiled for a big-endian target and, where an emulator is available, run there
  against the same files.
- SQL engines *(planned)*, each in three ways: as SQL alone (`CREATE TABLE`, `INSERT`, `SELECT` with conditions, joins
  and order, the results checked); as `DBF` (the same programs as the `DBF` tests, under `SET DB ENGINE`, with the
  same output); and mixed (a table made by SQL read with `USE`, navigation and fields, and one made with `dbCreate` and
  `REPLACE` read by `SELECT`), so that types, record numbers, deletion, blanks and `NULL`, widths and decimals agree
  both ways.

## Order of the work

1. Done: work areas, the driver interface, `DBF` without memos; the compiler's field references; `USE`, navigation,
   fields, `APPEND BLANK`, `REPLACE`, `DELETE`, `RECALL`, `PACK`, `ZAP`, `dbCreate`, `dbStruct`, `SET DELETED`,
   `LOCATE`, `SET FILTER`, `COUNT`, `SUM`; `dbstruct.ch`.
2. Done: `DBT` memos.
3. `NTX` indexes: `INDEX ON`, `SET INDEX`, `SET ORDER`, `SEEK`, `REINDEX`, `SET SOFTSEEK`, `SET UNIQUE`, conditions,
   scopes.
4. Sharing and locks.
5. `FPT` memos and `CDX` indexes (`DBFCDX`).
6. Relations, `COPY`, `APPEND FROM`, `SDF`, `DELIM`, `SORT`, `TOTAL`, `UPDATE`, `JOIN`.
7. `SET DB ENGINE`; then SQL engines as libraries, and queries.
