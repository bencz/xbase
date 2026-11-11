# Databases

The work areas and the database drivers (RDDs, Clipper's replaceable database drivers): tables, memos, indexes, locking,
and the commands and functions over them. The first three steps are done: work areas, the driver interface, `DBF` tables
with `DBT` memos and `NTX` orders (indexes), the compiler's field references and the commands over them. The rest of
this document marked *(planned)* is the plan the work follows.

Goals: Clipper's behavior, and files that Clipper programs and the reference read and write at the same time as xbase
programs (the same bytes on disk, the same locks), on every platform and byte order; drivers that can be added without
touching the rest (SQL engines later), each as a library when it needs one.

## Work areas

`runtime/src/rdd/` (portable C; `rdd.h` declares what the files share):

- `area.c`: each task's table of work areas (`xb_ctx.areas`, by number from 1, growing as needed; `xb_ctx.areaCurrent`
  starts at 1), aliases (upper case; any letter case finds them), and the navigation the language sees on top of a
  driver's raw records: records that are deleted (under `SET DELETED`, `Set( _SET_DELETED )`) or that the filter rejects
  are skipped; `BOF` and `EOF`; going to a record that is not there (0, past the last) leaves the blank record after the
  last current with both `Bof()` and `Eof()` true; a record that cannot be read leaves it there too. The order of
  navigation is the controlling order's when there is one ([Orders](#orders)), the record order otherwise. Any move, and
  `dbAppend`, makes `Found()` false. The filter, its text and the last `LOCATE`'s condition are roots of the collector
  (`xb_areas_mark`). Every table is written and closed when the program ends (`xb_program_run`), whatever ended it; an
  area is free before the error of a close that failed is raised.
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
- `order.c`, `ntx.c`, `ordcmd.c`: orders, their files and their functions ([Orders](#orders)).

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

## Orders

An order (an index) keeps the records in the order of a key: `order.c` makes keys and keeps the entries up to date,
`ntx.c` is the file ([Files](#files)), `ordcmd.c` the functions. An area has a list of orders open and a controlling one
(`xb_area.order`, 0 for the record order).

- `INDEX ON <key> TO <file> [FOR <cond>] [UNIQUE] [DESCENDING] [ADDITIVE]` (`ordCondSet()` then `ordCreate()`;
  Clipper's `dbCreateIndex()` too) evaluates the key of every record, deleted and filtered ones included, in record
  order (the condition's records only), builds the file and opens it as the controlling order, the other orders closed
  unless `ADDITIVE`; then the first record. The key's type and length come from the first record's value (the blank
  record's in an empty table): a string's length (at most 256), a number's `Str()` length and decimals, 8 for a date, 1
  for a logical; another type is `DBFNTX/1026` `Data type error`. `UNIQUE` (default `SET UNIQUE`) keeps the first
  record of each key. Of `ordCondSet()`'s settings the condition (1st and 2nd arguments), `DESCENDING` (11th) and
  `ADDITIVE` (13th) are used.
- `SET INDEX TO <files>` (`ordListClear()` and `ordListAdd()`), `USE ... INDEX <files>` (`dbSetIndex()`): an order
  opened when none is controlling becomes the controlling one, and the first record current; a file open already is
  not opened again; one that cannot be is `DBFNTX/1003` `Open error`, which may be retried. The key's type is the
  current record's value's; the key and condition are compiled from their text.
- `SET ORDER TO <n>` (`ordSetFocus()`, `dbSetOrder()`: both give the name before) chooses the controlling order and
  leaves the record where it is (0: the record order); `ordListClear()`, `CLOSE INDEXES` close the orders. `REINDEX`
  (`ordListRebuild()`, `dbReindex()`), `PACK` and `ZAP` build every order again; then the first record. When an
  order's key fails meanwhile, that order is closed and the ones after it stay as they were.
- `SEEK <value> [SOFTSEEK] [LAST]` (`dbSeek( xKey, [lSoft], [lLast] )`, `FIND`): the first record whose key starts
  with the value's key (a string is compared for its own length: a partial key; `SET EXACT` does not change that), the
  last one with `LAST`, skipping records that do not show (`SET DELETED`, the filter); `Found()` tells whether it is
  there. Not found: the end (`Eof()`), or with `SET SOFTSEEK` the first record after where it would be (with `LAST`,
  the last one before it; none: the end). A value of another type than the key's finds nothing; no value is
  `DBCMD/1001`, no controlling order `DBFNTX/1201` `Workarea not indexed`.
- A changed record's entries move when it is written (`xb_area_hot` takes the keys before the first change of a record,
  `xb_area_cold` compares them with the keys after, before the record is written, before any navigation, on `COMMIT`
  and at closing): a key that did not change costs nothing more than its evaluation; an appended record enters with its
  keys; a record that stops meeting a condition leaves that order. `SKIP` after a change goes on from where the record
  is now. While an order's key or condition block runs, the area's orders may not change: making, opening, closing
  them there is `DBCMD/1050` `Operation not supported` (closing the area stops what the block was for).
- Names: an order's name (`ordName()`, `ordSetFocus()`) is its file's name without directories and extension, upper
  case, its first 10 characters (NTX's names), and so are the names functions take; `ordBagName()` is the file's name
  as given. `IndexKey()`, `ordKey()`, `ordFor()`, `IndexOrd()`, `ordNumber()`, `ordIsUnique()`, `ordDescend()`,
  `ordBagExt()` and `IndexExt()` (`.ntx`) tell the rest; an order that is not there gives the empty value. An order
  named 0 is the controlling one for `ordName()`, `ordKey()`, `ordFor()`, `ordBagName()` and `IndexKey()`, none for
  `ordIsUnique()`, `ordDescend()` and `ordSetFocus()`, as in the reference.
- Keys (`xb_order_value_key`), as Clipper's NTX makes them: a string's bytes (blanks after it, or cut); a number's
  `Str( n, len, dec )` with blanks and the sign as `0` and, when negative, each digit d as `0x5C - d` (negative numbers
  first, larger ones first); a date's `DToS()` (blanks for the empty date); `T` or `F`. Keys compare as bytes, a
  descending order's the other way round, equal keys by record number.

Where xbase does not follow the reference (checked by `tests/run/rdd_index_stress.prg` and the cross runs of
[Tests](#tests)): equal keys stay in record order through every change; the reference puts a key it adds after the
keys equal to it (its order of equal keys then depends on the history of changes). Both read the other's files: an
entry looked for by key and record that is not where record order puts it is looked for among its key's entries
(`tests/run/rdd_index_seek.prg` keeps such a file). A key or condition block that closes the area, or would change its
orders, is handled as above (`rdd_index_blocks.prg`); the reference fails or reads freed memory then.

## Drivers

A driver is a table of operations on the raw table (`xb_rdd` in `rdd.h`; an open failure names the file that failed,
the table or a file of its own): check a structure (the types, lengths and sizes it can make; `dbCreate` checks the
names), create, open, close, the record count, go to a record (the blank one after the last outside the table; the
record's bytes are read when first used: `load`, which `get` and `put` do themselves), flush (the record, and on
`COMMIT` the header), append, read and write a field, a field's bytes, the deletion flag, pack, zap, and the header's
sizes and date. Operations that fail give the
`FError()` code of the failure; the work area raises the error. The names `DBF` and `DBFNTX` (the default, as in
Clipper; `RddSetDefault()` and `USE ... VIA` choose) both give the `DBF` driver with `DBT` memos and `NTX` orders,
named `DBFNTX`.

*(Planned)*: locks in the table, and the order operations in it when a second index format comes; drivers that inherit
what they do not change from the driver they extend; `DBFCDX` (`DBF`, `FPT` memos and `CDX` compound indexes), `SDF` and
`DELIM` (text files, for `COPY TO` and `APPEND FROM`); `SET DB ENGINE TO <name>` (xbase's own command, over
`RddSetDefault()`); later, as libraries in `libs/`, SQL engines (SQLite, PostgreSQL), among them an SQLite driver that
behaves as `DBF` does (the row id as `RecNo()`, navigation in the order of an index, `Deleted()` a column). The
language's other commands come with them: `SET RELATION`, `SET EXCLUSIVE`, `SET SCOPE`, `COPY`, `APPEND FROM`, `SORT`,
`TOTAL`, `AVERAGE`, `UPDATE`, `JOIN`, `LIST`, `DISPLAY`.

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
- **NTX** (`ntx.c`): 1024-byte pages. Page 0 is the header: a signature (6, 7 with a condition), a counter of
  changes, the root's and the first free page's offsets, an entry's size (the key's and 8), the key's length and
  decimals, the entries a page holds (`floor(1022 / (entry + 2)) - 1`, made even) and half of that, the key's text,
  the `UNIQUE` and `DESCENDING` flags, the condition's text. A page: its entry count, then an offset per slot (the
  entries in order, then the free slots), the slots (each a page offset, a record number, a key); the entry after the
  last holds only the page of the keys after all others. A B-tree: the entries of a branch page are entries too, the
  pages under it before them. A free page has count 0 and the next free page in its only entry; new pages come from
  that list first.
  - The build (`INDEX ON`, `REINDEX`) sorts the keys (a merge sort by an 8-byte prefix as a number, then the rest of
    the key, then the record) and writes the pages bottom up in one pass, as the reference lays them out (its files and
    xbase's are the same bytes, but bytes of unused slots that the reference leaves from pages it wrote before): full
    pages, the entry after a full page going up a level; at the end, from the bottom, a last page with fewer than half
    the entries shares them with the page before it (the first one more; the second page's arrivals take its free
    slots turned by `F mod k`, as the reference's do).
  - Adding an entry moves 2-byte offsets in its page. A full page gives an entry, through the parent, to a sibling that
    has room; else it splits: in halves, but at the end of the last page of its level (keys coming in order) the left
    page keeps all it can, at the start of the first one the right page does, so pages filled in order stay full.
  - Removing an entry of a branch page puts the entry before it (the last of a leaf) in its place; a page left under
    half takes from a sibling with more than half, or joins it (the page freed); a root left empty gives way to its
    only page.
  - The counter of changes goes up once each time pages are written; the reference counts its changes otherwise
    (sharing, step 4, reads it).
- **CDX**: 512-byte pages; a compound index of tags, each a B-tree whose leaf pages keep keys compressed (the record
  number, duplicate and trailing counts packed in bits); numbers and dates as big-endian sortable doubles.

Every format is checked against the reference's files: what one writes, the other reads, and after the same operations
the files are the same bytes (but the date of the last update). `tests/run/rdd_format.prg`, `rdd_memo_format.prg` and
`rdd_index_format.prg` keep files the reference wrote and check both ways.

## Performance

- Navigation reads no record: `GO`, `SKIP` and `SEEK` set the record number, and the driver reads the record when a
  field, its deletion flag or a key is first used (`load`). In order, a `SKIP` moves the order's cursor (the path from
  the root to the entry, kept from one call to the next while the order does not change) to the next entry.
- Each open order keeps 128 pages (a hash from page to slot, the page used last checked first); pages changed are
  written on `COMMIT`, at closing, or when the cache needs their slot.
- A page is searched by halves over its offsets; a key compares with `memcmp`.
- Files are read and written at a position in one call (`pread`/`pwrite`, `ReadFile`/`WriteFile` with an offset;
  `xb_platform_file_read_at`), the last record with the end mark after it in one write. Records read in order come
  64 KB at a time.
- The build: the keys in one array (a key that is a character field alone is copied from the record, no block run),
  one sort, one pass of writes. Keeping orders up to date costs the keys' evaluation before and after a change, and
  the entries only of keys that changed.
- `bench/index.prg` times a build over 200,000 records, 100,000 SEEKs, a SKIP through and 20,000 appends with the
  order open. Measured once against the reference on one machine, the same program: the build and SEEK about as
  fast, SKIP and appends with an order open faster.

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
  reference's bytes. Orders were also kept by both in turn (each changing the other's files) and checked by the other,
  the trees checked page by page (fill, order, every page reachable once, the free list).
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
3. Done: `NTX` indexes: `INDEX ON`, `SET INDEX`, `SET ORDER`, `SEEK`, `REINDEX`, `SET SOFTSEEK`, `SET UNIQUE`,
   conditions. Next in this step: scopes (`ordScope()`, `SET SCOPE`), key positions (`ordKeyCount()`, `ordKeyNo()`,
   `ordKeyGoto()`, `ordKeyVal()`), `ordSkipUnique()`, and `INDEX ON`'s `WHILE`, `NEXT`, `EVAL` and `EVERY`.
4. Sharing and locks.
5. `FPT` memos and `CDX` indexes (`DBFCDX`).
6. Relations, `COPY`, `APPEND FROM`, `SDF`, `DELIM`, `SORT`, `TOTAL`, `UPDATE`, `JOIN`.
7. `SET DB ENGINE`; then SQL engines as libraries, and queries.
