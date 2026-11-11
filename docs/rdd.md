# Databases (planned)

The design of the work areas and the database drivers (RDDs, Clipper's replaceable database drivers): tables, memos,
indexes, locking, and the commands and functions over them. Nothing of it exists yet; this document is the plan the work
follows, and becomes the description of the code as parts of it are done.

Goals: Clipper's behavior, and files that Clipper programs and the reference read and write at the same time as xbase
programs (the same bytes on disk, the same locks), on every platform and byte order; drivers that can be added without
touching the rest (SQL engines later), each as a library when it needs one.

## Layers

1. **Work areas** (`runtime/src/rdd/`, portable C): per task, as the `SET`s, a table of work areas by number, their
   aliases, the current one, and for each the driver and its state. Every database function of the language works
   through it: `dbUseArea`, `dbCloseArea`, `dbSelectArea`, `Select`, `Alias`, `Used`, navigation (`dbGoTo`, `dbGoTop`,
   `dbGoBottom`, `dbSkip`, `Bof`, `Eof`, `RecNo`, `LastRec`, `Found`), fields (`FieldGet`, `FieldPut`, `FieldPos`,
   `FieldName`, `FCount`, `dbStruct`), changes (`dbAppend`, `dbDelete`, `dbRecall`, `Deleted`, `dbCommit`, `__dbPack`,
   `__dbZap`), orders (`ordCreate`, `ordListAdd`, `ordSetFocus`, `dbSeek`, `ordKey`, scopes), filters, relations,
   locks (`RLock`, `FLock`, `dbUnlock`, `NetErr`), `dbEval`, `dbCreate`, `Header`, `RecSize`, `LUpdate`.
2. **The driver interface**: a table of operations (open, create, close, go to, skip, read and write a field, append,
   delete, lock, flush, and the order operations), which a driver fills, inheriting what it does not change from the
   driver it extends. Drivers register by name; `RddSetDefault()` and `USE ... VIA` choose them. `SET DB ENGINE TO
   <name>` (xbase's own command, over `RddSetDefault()`) is the friendly form.
3. **Drivers**: `DBF` (the table), `DBFNTX` (`DBF`, `DBT` memos and `NTX` indexes; the default, as in Clipper),
   `DBFCDX` (`DBF`, `FPT` memos and `CDX` compound indexes), `SDF` and `DELIM` (text files, for `COPY TO` and
   `APPEND FROM`). Later, as libraries in `libs/`: SQL engines (SQLite, PostgreSQL), among them an SQLite driver that
   behaves as `DBF` does (the row id as `RecNo()`, navigation in the order of an index, `Deleted()` a column).
4. **The language**: the commands of `std.ch` over the functions (`USE`, `SELECT`, `APPEND BLANK`, `REPLACE`, `DELETE`,
   `RECALL`, `PACK`, `ZAP`, `GO`, `SKIP`, `SEEK`, `LOCATE`, `CONTINUE`, `INDEX ON`, `SET INDEX`, `SET ORDER`,
   `REINDEX`, `SET FILTER`, `SET RELATION`, `SET DELETED`, `SET EXCLUSIVE`, `SET SOFTSEEK`, `SET UNIQUE`, `COPY`,
   `APPEND FROM`, `SORT`, `TOTAL`, `COUNT`, `SUM`, `AVERAGE`, `UPDATE`, `JOIN`, `LIST`, `DISPLAY`, ...), and in the
   compiler the field references: `alias->field`, `FIELD->name`, `(alias)->( expr )`, `FIELD` declarations, and
   undeclared names, which are a field of the current work area before a memory variable (Clipper's rule). A field
   reference finds the field by name once per work area and keeps its position in a cache of the reference, as sends
   keep their member.

## Files

Every binary field of a file is read and written through the byte-order helpers (`core/bytes.h`): DBF, DBT and NTX
are little-endian; FPT headers and CDX keys and some of their fields are big-endian. No structure is ever copied to or
from a file whole, so every host, little- or big-endian, 32- or 64-bit, writes the same bytes.

- **DBF**: the header (version, date of the last update, record count, header and record lengths, flags, the codepage
  byte), field descriptors (name, type, length, decimals), records with their deletion flag, the end mark. Field types:
  Clipper's `C` (up to 64 KB with the length's high byte in the decimals), `N`, `D`, `L`, `M`; then the common
  extended ones (`I`, `B`, `Y`, `T`, `@`, `+`, `V`, `Q`) as the reference reads them.
- **DBT** (dBASE III memos): 512-byte blocks, each memo ended by two `0x1A` bytes.
- **FPT** (FoxPro memos): the block size in the header, each block with its type and length; big-endian.
- **NTX**: 1024-byte pages, the key expression, size and decimals, the unique flag and the condition; keys as the
  ordering of their values demands (numbers in a form that sorts as text, dates as `DToS()`).
- **CDX**: 512-byte pages; a compound index of tags, each a B-tree whose leaf pages keep keys compressed (the record
  number, duplicate and trailing counts packed in bits); numbers and dates as big-endian sortable doubles.

Every format is checked against the reference's files: what one writes, the other reads, and after the same operations
the files are the same bytes (but the date of the last update).

## Sharing and locks

Programs share tables as Clipper's do: `SHARED` and `EXCLUSIVE` opening, record and file locks through the platform
layer (byte-range locks on POSIX systems and on Windows, at the offsets of each driver's lock scheme, the same as the
reference's, so programs of both lock each other out correctly), the record count and the header read again when the
table may have changed, `NetErr()` after a failed `USE` or `APPEND BLANK`, and `COMMIT`. The lock offsets are taken
from the reference's behavior (observed in the locks it sets), never from its code.

## Text and codepages

A table's bytes are kept as they are: the functions read them as `SET CODEPAGE` says. A codepage per table (the
header's codepage byte, a `USE ... CODEPAGE` clause) that converts on reading and writing comes later.

## Queries

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

## Order of the work

1. Work areas, the driver interface, `DBF` without memos; the compiler's field references; `USE`, navigation, fields,
   `APPEND BLANK`, `REPLACE`, `DELETE`, `RECALL`, `PACK`, `ZAP`, `dbCreate`, `dbStruct`, `SET DELETED`, `LOCATE`,
   `SET FILTER`, `COUNT`, `SUM`; `dbstruct.ch`.
2. `DBT` memos.
3. `NTX` indexes: `INDEX ON`, `SET INDEX`, `SET ORDER`, `SEEK`, `REINDEX`, `SET SOFTSEEK`, `SET UNIQUE`, conditions,
   scopes.
4. Sharing and locks.
5. `FPT` memos and `CDX` indexes (`DBFCDX`).
6. Relations, `COPY`, `APPEND FROM`, `SDF`, `DELIM`, `SORT`, `TOTAL`, `UPDATE`, `JOIN`.
7. `SET DB ENGINE`; then SQL engines as libraries, and queries.
