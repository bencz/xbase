# Databases

The work areas and the database drivers (RDDs, Clipper's replaceable database drivers): tables, memos, indexes, locking,
and the commands and functions over them. Done: work areas, the driver interface, `DBF` tables with `DBT` and `FPT`
memos, `NTX` and `CDX` orders (indexes), relations, copies to and from tables and text files, sharing and locks, the
compiler's field references and the commands over them. The rest of this document marked *(planned)* is the plan the
work follows.

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
  `REPLACE`, `LOCATE`, `COUNT`, `SUM`, `AVERAGE`) share one scope walk: from the top, unless `WHILE`, `NEXT n` or
  `REST` start it at the current record or `RECORD n` names the one record; `WHILE` stops at the first record that
  fails it, `NEXT n` on the last record it took (none for `n` below 1). `AVERAGE` counts the records in the memory
  variable `__Avg`, as Clipper's does (no record: the averages are 0, the zero divisor's default).
- A table opened `READONLY` takes no write: `FieldPut`, assignments to fields, `dbDelete`, `dbRecall`, `dbAppend`,
  `__dbPack` and `__dbZap` are `DBFNTX/1025` `Write not allowed` (generic code `EG_READONLY`), and nothing changes.
- `field.c`: the field references of compiled code (below) and `__AreaEval( xArea, bBlock )`, which evaluates
  `alias->( expr )`.
- `dbf.c`: the `DBF` driver, with its memos in `dbt.c` ([Files](#files)).
- `order.c`, `index.h`, `ntx.c`, `ordcmd.c`: orders, the interface of their files' formats, NTX files and the
  functions ([Orders](#orders)).
- `relation.c`: relations between areas and their functions ([Relations](#relations)).
- `transfer.c`, `transfer.prg`, `text.c`: the copies between files and the text file drivers ([Copies between
  files](#copies-between-files)).

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
`ordcmd.c` has the functions. An area has a list of orders open and a controlling one (`xb_area.order`, 0 for the
record order).

An order is a tag of an order file, a bag (`xb_order_bag`, shared by the area's orders in it and closed with the last of
them), in its driver's format (`xb_rdd.index`, `index.h`): NTX (`ntx.c`, one order per file, named by the file) or CDX
(`cdx.c`, the named tags of one file). The format has the keys (their length for a value, a value's key bytes, a key's
value again, their comparison in the order's order, and whether its file stores a descending order the other way round),
the files (open with their tags, build a tag from sorted entries, remove a tag, flush, close, and a hold of the file's
lock across several operations) and each tag's cursor (first, last, next, prev, seek, find, the counts and the n-th
entry, the entry's key, an entry replaced by another); every cursor operation works in the order's own order, whatever
the file stores. A record's change evaluates every order's new key first, then moves the entries that changed bag by
bag, each bag's lock held once for all its orders (an order whose new key could not be had stops the change for it and
the orders after it; an error of the file is raised once the lock is let go, so that its handler may do anything,
closing the area included).

- `INDEX ON <key> TO <file> [FOR <cond>] [ALL] [WHILE <cond>] [NEXT <n>] [RECORD <n>] [REST] [EVAL <block>]
  [EVERY <n>] [UNIQUE] [DESCENDING] [ADDITIVE] [USECURRENT]` (`ordCondSet()` then `ordCreate()`; Clipper's
  `dbCreateIndex()` too) evaluates the key of the records it takes (the condition's only), builds the file and opens it
  as the controlling order; then the first record. The other orders stay open while it is built and close after it
  unless `ADDITIVE`. Which records (`collect` in `order.c`): with no scope clause, or `ALL`, every record, deleted and
  filtered ones included, in record order. `RECORD n` takes that record; `NEXT n`, `REST` and `WHILE` start at the
  current record (`ordCondSet()`'s 7th argument, which the command gives as `RecNo()`), `USECURRENT` at the
  controlling order's first record in its scope; those walk the controlling order (the record order without one)
  raw: deleted and filtered records are taken, and a record the order has no entry for ends the walk. `WHILE` ends it
  at the first record it rejects, `NEXT` after n records. The `EVAL` block runs after the first record taken and then
  after every `EVERY` more (not after `NEXT`'s last one), with the area current; anything but `.T.` ends the build,
  which keeps the records taken so far. Records appended while the build runs are not taken; in record order, blocks
  that move the record do not change which records are taken. A record's key is the one it has when taken: a block
  that changes records the build has taken leaves their entries as they were. The order is not in the area's list
  while it is built (blocks that run meanwhile do not see it). The key's type and
  length come from the first record's value (the blank record's when there is none): a string's length (at most 256),
  a number's `Str()` length and decimals, 8 for a date, 1 for a logical; another type is `DBFNTX/1026` `Data type
  error`. `UNIQUE` (default `SET UNIQUE`) keeps the first record of each key. `ordCondSet( [cFor], [bFor], [lAll],
  [bWhile], [bEval], [nEvery], [nStart], [nNext], [nRecord], [lRest], [lDescend], [lCompound], [lAdditive],
  [lCurrent] )` keeps these settings (`xb_order_conditions`) for the next order made in the area; `lCompound` is
  NTX's one order per file.
- Files of tags (CDX): `INDEX ON <key> TAG <name> [TO <file>]` (`ordCreate( [cFile], cTag, ... )`) adds tag
  `<name>` to the file, replacing a tag of that name (its order closed, the new one in its place in the list); without
  `TO` the file is the table's production file, its name with `.cdx` in its directory. `INDEX ON <key> TO <file>`
  without `TAG` makes the file anew (its orders open in the area closed first) with one tag named as the file. A tag
  left alone in its file by the replacement makes the file anew. A production file made in an area that did not have
  it open sets the flag of byte 28 of the table's header (bit `0x01`); `USE` opens the production file with the table
  when it exists (first in the list, no order controlling), and `ordListClear()`, `CLOSE INDEXES`, `SET INDEX TO` and
  `INDEX ON` without `ADDITIVE` keep its orders open. An order added to a file the area has open comes after that
  file's other orders. `ordDestroy( xOrder, [cFile] )` and `DELETE TAG <name> [IN <file>] [, ...]` take a tag out of
  its file and close its order; a file left without tags (an NTX file always) is erased, and the production file's
  erasure clears the header's flag. `ordCount( [cFile] )` counts the area's orders (in the file). `REINDEX` makes
  each CDX file anew with its tags in their order (the first one makes the file, the others are added), each as it
  was described (its conditions, the partial flag cleared); when one fails, the orders after it in its file close
  with it (the file holds them no more; the reference keeps them open on tags its file no longer has). A tag is
  partial when built with `NEXT`, `RECORD`, `REST` or `USECURRENT`. A tag is added to an existing file only when it is
  a CDX file this reader takes; an empty file is made anew, another one is an error (the reference makes any file it
  cannot read anew, losing what it holds).
- `SET INDEX TO <files>` (`ordListClear()` and `ordListAdd()`), `USE ... INDEX <files>` (`dbSetIndex()`): an order
  opened when none is controlling becomes the controlling one, and the first record current; a file open already is
  not opened again; one that cannot be is `DBFNTX/1003` `Open error`, which may be retried. The key's type is the
  current record's value's; the key and condition are compiled from their text.
- `SET ORDER TO <n>` (`ordSetFocus()`, `dbSetOrder()`: both give the name before) chooses the controlling order and
  leaves the record where it is (0: the record order); `ordListClear()`, `CLOSE INDEXES` close the orders. `REINDEX
  [EVAL <block>] [EVERY <n>]` (`ordListRebuild()`, `dbReindex()`), `PACK` and `ZAP` build every order again from all
  the records (`REINDEX`'s `EVAL` as `INDEX ON`'s, for each order); then the first record. When an order's key or the
  `EVAL` block fails meanwhile, that order is closed and the ones after it stay as they were.
- Scopes: each order has a top and a bottom bound (`xb_order_scope`). `ordScope( nScope, [xValue] )` gives the
  controlling order's bound before (0 top, 1 bottom; `NIL` for none) and sets it to `xValue`, `NIL` clearing it;
  `nScope` 2 sets both (no value: clears both) and gives `NIL`. `SET SCOPETOP TO [x]`, `SET SCOPEBOTTOM TO [x]`,
  `SET SCOPE TO [x [, y]]` (one value: both bounds), `SET SCOPE TO , y` are `std.ch` rules over it. A bound compares
  as a prefix, as `SEEK` does; a value of another type than the key's leaves the bound off; a codeblock is evaluated
  again before each navigation (one that fails leaves the bound as it was). Setting a scope does not move the record.
  `GO TOP`, `GO BOTTOM`, `SKIP` and `SEEK` stay within it: `SKIP` past it, or from a record outside it, is the end
  (back: `Bof()` on its first record); a `SEEK` of a key outside it finds nothing, and with `SOFTSEEK` stops on the
  scope's first record when the key comes before the scope, at the end when after it.
- Positions in the scope, the filter and `SET DELETED` not counted: `ordKeyCount( [xOrder] )` the entries,
  `ordKeyNo( [xOrder] )` the current record's position (0 when it has no entry there), `ordKeyGoto( nKeyNo )` goes to
  the record of entry n of the controlling order (then forward to the first that shows; past the last: the end; below 1:
  `.F.`), `ordKeyVal()` the current record's key as a value (a number with the key's width and decimals; a CDX key's,
  which keeps none, with `SET DECIMALS`; a date, a logical; `NIL` when it has no entry). `ordSkipUnique( [nDirection] )`
  goes to the next key's first record, or back to the previous key's last one (back from the first key: the first record
  with `Bof()`). With no order they are `LastRec()`, `RecNo()`, `GO n`, `NIL` and `SKIP n`.
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
  is now. While an order's key, condition, `WHILE`, `EVAL` or scope block runs, the area's orders may not change:
  making, opening, closing them or `ordCondSet()` there is `DBCMD/1050` `Operation not supported` (closing the area
  stops what the block was for).
- Names: an order's name (`ordName()`, `ordSetFocus()`) is its tag's name, or its file's without directories and
  extension: upper case, its first 10 characters, without trailing blanks (`XB_INDEX_NAME_MAX`), and so are the names
  functions take; `ordBagName()` is the file's name as given. `IndexKey()`, `ordKey()`, `ordFor()`, `IndexOrd()`,
  `ordNumber()`, `ordIsUnique()`, `ordDescend()`, `ordBagExt()` and `IndexExt()` (`.ntx`, `.cdx`) tell the rest; an
  order that is not there gives the empty value. An order named 0 is the controlling one for `ordName()`, `ordKey()`,
  `ordFor()`, `ordBagName()` and `IndexKey()`, none for `ordIsUnique()`, `ordDescend()` and `ordSetFocus()`, as in the
  reference.
- Keys (`xb_order_value_key`), as Clipper's NTX makes them: a string's bytes (blanks after it, or cut); a number's
  `Str( n, len, dec )` with blanks and the sign as `0` and, when negative, each digit d as `0x5C - d` (negative numbers
  first, larger ones first); a date's `DToS()` (blanks for the empty date); `T` or `F`. Keys compare as bytes, a
  descending order's the other way round, equal keys by record number (a build that walks another order, with
  `USECURRENT`, `REST`, ..., keeps among equal keys the order the walk reaches them in, as the reference does, and
  `UNIQUE` the first of them). CDX keys: a string's bytes as NTX's; a number (and a date's day number) as a
  big-endian double, its sign bit set when not negative and every bit turned when negative (minus zero is zero); `T`
  or `F`; a descending tag is stored ascending and walked from its end; equal keys by record number always.

Where xbase does not follow the reference (checked by `tests/run/rdd_index_stress.prg` and the cross runs of
[Tests](#tests)): equal keys stay in record order through every change; the reference puts a key it adds after the
keys equal to it (its order of equal keys then depends on the history of changes). Both read the other's files: an
entry looked for by key and record that is not where record order puts it is looked for among its key's entries
(`tests/run/rdd_index_seek.prg` keeps such a file). A key, condition, `EVAL` or scope block that closes the area, or
would change its orders, is handled as above (`rdd_index_blocks.prg`, `rdd_index_eval.prg`); the reference fails or
reads freed memory then. `ordKeyCount()` with a codeblock bound counts for the block's value now; the reference may
give a count it made for an earlier value (`rdd_index_scope.prg`). `REINDEX EVAL` takes the pending `ordCondSet()`
`EVAL` and `EVERY` and clears the pending settings; the reference keeps them for the next order made. The order
rules above are the same for every format: the reference's CDX driver differs from its own NTX one in some of them
(it counts `EVAL` calls and walks `REST` otherwise, its `ordKeyCount()` and `ordKeyNo()` leave out filtered and
deleted records, its `ordKeyGoto( 0 )` gives `.T.`), and xbase keeps NTX's rules there.

## Relations

`SET RELATION TO <expr> INTO <alias> [SCOPED] [, ...] [ADDITIVE]` (`dbSetRelation( xArea, bExpr, [cExpr], [lScoped] )`,
`ordSetRelation( xArea, bKey, [cKey] )` for a scoped one) makes the area's child follow it; without `ADDITIVE` the
area's relations go first (`dbClearRelation()`, `SET RELATION TO`). An area holds its relations in the order they were
set (`xb_area.relations`, `xb_relation`: the child's number, the block and its text, roots of the collector);
`dbRelation( [n] )` gives relation n's text and `dbRSelect( [n] )` its child's number (0 or no `n`: the first; `""` and
0 for none).

- Where the child goes: at the parent's end, to its own end; else the expression is evaluated with the parent current
  and its value sought in the child's controlling order, without soft seek (deleted and filtered records skipped,
  `Found()` telling), within a scope of that value (both bounds) for a scoped relation; a child without an order goes
  to the record of that number (`Found()` whether it is there; a value that is not a number: the end).
- The child moves lazily, as Clipper's drivers move it: a change of the parent's record (each step of a navigation
  included) only marks the child's move pending (`xb_area.relPending`, `relParent`), and its children's; the child
  makes it (`xb_area_settle`, the parent's own pending move first) before anything reads or changes its record: its
  position (`RecNo()`, `Eof()`, `Bof()`, `Found()`), fields, deleted mark, locks, filter, a `SKIP`, a scope walk, an
  order build, `ordKeyNo()`, `ordKeyVal()`, `ordSkipUnique()` (`xb_area_record`, `xb_db_record`, field references).
  Functions about the area itself (`Used()`, `Alias()`, `SET ORDER`, `IndexOrd()`, ...) do not make it, so a move
  pending when the controlling order changes seeks in the new one. The expression runs once per move, with the
  parent's record as it is then (a key written in the parent before the child is read moves the child there).
  `APPEND` in the parent does not move the children.
- A move of the child that does not start from its record (`GO`, `GO TOP`, `GO BOTTOM`, `SEEK`, `APPEND`) drops the
  pending move. A scoped relation's pending move is made first, there and wherever the child's scope is read or bounds
  a navigation (`ordScope()`, `ordKeyCount()`; `xb_area_settle_scoped`).
- Ending a relation (`dbClearRelation()`, closing the parent) makes the move it left pending and clears a scoped
  child's scope. Closing the child removes the relations into it (`xb_area.parents` counts them); while an area closes
  (`closing`), no relation is set from or into it.
- What an expression does to the relations it stands on is checked, not trusted: a relation is found again by its id
  after each evaluation (one an expression removed moves nothing more), the expression's own moves of its parent make
  no second move of the child (`settling`), and a relation is freed only where the children's pending moves that name
  it are dropped.
- Errors: no table in the current area, `DBCMD/2001`; an alias that names no area, `BASE/1002`, which may be retried;
  no area at the number, one closing, or no block, `DBCMD/1004`; the area itself, or a child whose relations lead back
  to it, `DBCMD/1013` (the reference refuses only the area itself; a longer cycle crashes it once the parent moves).
- The expression is evaluated with the parent current (`xb_area_eval`, as order keys are), and the area current before
  comes back also after an error in it (the reference leaves the parent current then). A relation set without text
  gives `""` to `dbRelation()` (the reference gives NIL).

## Drivers

A driver is a table of operations on the raw table (`xb_rdd` in `rdd.h`; an open failure names the file that failed,
the table or a file of its own): check a structure (the types, lengths and sizes it can make; `dbCreate` checks the
names), create, open, close, the record count, go to a record (the blank one after the last outside the table; the
record's bytes are read when first used: `load`, which `get` and `put` do themselves), flush (the record, and on
`COMMIT` the header), append, read and write a field, a field's bytes, the record's fields' bytes and their
replacement at once (`record_bytes`, `put_record_bytes`: a copy between tables of the same fields), the deletion flag,
pack, zap, and the header's sizes and date, the production-file flag of the header (`set_production`), and reading the
current record again in shared mode before an order is placed from it (`refresh`). Operations that fail give the
`FError()` code of the failure; the work area raises the error. A field's value is read from and written into the bytes
a table stores by one rule for every driver (`xb_field_decode`, `xb_field_encode` in `field.c`; memos are the
driver's). `RddSetDefault()` and `USE ... VIA` choose the driver:

- `DBFNTX` (the default, as in Clipper; `DBF` is the same): new tables get `DBT` memos, orders are `NTX`, shared tables
  are locked by Clipper's scheme.
- `DBFCDX`: the same `DBF` code (`xb_rdd_dbfcdx`); new tables get `FPT` memos, shared tables are locked by FoxPro's
  scheme. Its orders are `CDX` files.
- `SDF` and `DELIM` (`text.c`): text files, for the copies between files ([Text files](#text-files)).

A driver tells its tables' extension (`ext`, `dbTableExt()`'s), its memo format (`memo`, rdd/memo.h), its order files'
format (`index`, rdd/index.h, whose `ext` is `ordBagExt()`'s) and its lock scheme (`lockScheme`). A table's memo format
comes from its version byte, whatever driver opens it (`0x83` DBT, `0xF5` FPT; a table of plain fields, `0x03`, its
driver's), so either driver reads and writes the other's tables. `DBFNTX()`, `DBFCDX()`, `SDF()` and `DELIM()` are
functions only so that `REQUEST DBFCDX` links as in Clipper programs.

*(Planned)*: `SET DB ENGINE TO <name>` (xbase's own command, over `RddSetDefault()`); later, as libraries in `libs/`,
SQL engines (SQLite, PostgreSQL), among them an SQLite driver that behaves as `DBF` does (the row id as `RecNo()`,
navigation in the order of an index, `Deleted()` a column). The language's other commands come with them: `SORT`,
`TOTAL`, `UPDATE`, `JOIN`, `LIST`, `DISPLAY`.

## Copies between files

`COPY TO`, `APPEND FROM` (`... SDF`, `... DELIMITED [WITH ...]`, `VIA <driver>`), `COPY STRUCTURE [FIELDS ...]`,
`COPY STRUCTURE EXTENDED` and `CREATE <file> [FROM <file>]` are `std.ch` rules over functions written in xBase
(`transfer.prg`: `__dbCopy`, `__dbApp`, `__dbSDF`, `__dbDelim`, `__dbCopyStruct`, `__dbCopyXStruct`, `__dbCreate`),
which use three primitives (`transfer.c`): `__dbUsed( cOperation )` (`DBCMD/2001` for the command without a table),
`__dbTransOpen( cFile, [cDriver], lCreate, [aStruct], [xDelimiter] )`, which opens the other file in the lowest free
area, without an alias (`xb_area_open_with`; the current area stays current), and `__dbTrans( nTarget, aFields, [bFor],
[bWhile], [nNext], [nRecord], [lRest] )`, which copies the records of the current area's scope.

- `COPY TO` creates the file (the driver's extension added to a name without one; one there is emptied) with the
  current area's fields the `FIELDS` list names, in its order (every field without it; names in any letter case, after
  `alias->`, `FIELD->` or `_FIELD->`; unknown names and names given again left out), copies the records of the scope
  (as `dbEval`'s: `SET DELETED` and the filter apply, the starting record of `NEXT`, `REST`, `WHILE` and `RECORD` taken
  as it is) and closes it; the area stays where the scope left it.
- `APPEND FROM` opens the file (a table shared and read-only; a text file with the current area's fields the list
  names) and appends the records of its scope, whose blocks run with the file's area current (they read its fields),
  to the current area, the fields of the same names; the area stays current, on the last record appended.
- A record is copied whole, its bytes as they are (blanks and stars kept), when both have the same fields in the same
  places and no memo; else field by field (`copy_fields`): a field of the same kind and size whose bytes are a value as
  a table stores it is copied as it is, others are read and written as values (a blank number becomes 0, a blank logical
  `.F.`, as in the reference; a value of another type is `DBFNTX/1020`, which the copy goes on after when defaulted; too
  wide, stars and `1021`). A deleted record's copy is deleted; one whose copy failed (an error not defaulted) stays,
  deleted, and the copy ends with the error, the other file closed first.
- From a text file, a field the line leaves blank stays blank and stars stay stars; others are read as the table stores
  values: a date that is not one is blank, a logical is `T` or `F`, a number is written in the field's form (the
  reference copies the text's bytes into the table as they are, unchecked, when the structures match).
- The source is read in order (`xb_area.snapshot`: a shared table's records read ahead and counted once, as under the
  file lock), and its records past the count it had when the copy started are not copied: `APPEND FROM` the table
  itself copies the records it had (the reference goes on without end).
- `COPY STRUCTURE` creates an empty table of the chosen fields (no table, or no field chosen: `DBCMD/1014`; the
  reference creates a table of no fields); `COPY STRUCTURE EXTENDED` a table of `FIELD_NAME C10`, `FIELD_TYPE C1`,
  `FIELD_LEN N3`, `FIELD_DEC N3` with a record for each field (a character field longer than 255 bytes keeps its
  length's low byte as `FIELD_LEN` and the high byte as `FIELD_DEC`); `CREATE <file>` creates and opens such a table,
  `CREATE <file> FROM <file>` a table of the structure the records of the second describe, in the current area or with
  `NEW` a new one, under `ALIAS`.
- `DELIMITED WITH` takes an expression or a name (`BLANK`, `TAB`, `PIPE`), and clauses may follow it; Clipper's rule,
  tried after it, takes the rest of the command as the delimiter's text. An unknown driver is `DBCMD/1015` (the
  reference's `COPY TO` uses the default driver, its `APPEND FROM` crashes). `CODEPAGE` is accepted and not used.

### Text files

`SDF` and `DELIM` keep no structure: the copy gives them the table's fields (`open_with`), checked as a `DBF` table's
(`xb_dbf_check`). Without one a text file cannot be opened: `USE ... VIA "SDF"` is an `Open error` (`FError()` 5), and
`dbCreate( ..., "SDF" )` creates an empty file, which it then cannot open. A record is a line; a text
file is created and written in record order, or read: it is read once to find where its lines start (8 bytes a line
kept), then a line is read, through a window of 64 KB, when its record is used.

- Lines end at the system's newline when written (`xb_platform_eol`: LF, CR LF on Windows; the reference's default),
  and a Ctrl-Z (`0x1A`) ends the file. Read, a line ends at CR or LF, the other one right after it belonging to the
  same end; a Ctrl-Z is dropped wherever it is; an empty line is a blank record, the end of the file after a line's
  end none.
- `SDF`: the fields' bytes side by side as a table stores them (a memo has none); a line shorter than the record is
  blank to the end, a longer one cut.
- `DELIM`: character fields without trailing blanks between delimiters, numbers without leading blanks (a blank one
  `0` with its decimals), dates' digits (an empty date nothing), logicals `T` or `F`, memos left out with their
  separators; nothing is escaped. The delimiter and separator are `"` and `,`; `WITH BLANK`, `TAB` and `PIPE` give no
  delimiter and a space, tab or bar; another text its first character as the delimiter; an array `{ cDelimiter,
  cSeparator }` each element's first character. Read, blanks before a field are skipped (so runs of blanks separate a
  `BLANK` line's fields); a field that starts with the delimiter ends at the next one, what follows up to the
  separator dropped; a character field takes its text cut to its length, a number its value (stars when it does not
  fit), a date eight digits of a date, a logical `T` when the text starts with `T` or `Y`; the fields a line does not
  reach stay blank. The reference drops the separator after a first field that writes nothing (an empty date first),
  which shifts that line's columns; xbase writes every separator.

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
  fields and a version byte of another memo format (dBASE IV, ...) is `DBFNTX/1012` `Corruption detected`.
- **FPT** (`fpt.c`, FoxPro memos as the reference writes them, with the free list of Clipper's FlexFile): a 1024-byte
  header (the first free block and the block size, 64 for new files, big-endian; the FlexFile mark at `0x200`, then,
  little-endian, the byte offsets of the free list's two pages and a counter of changes). A memo starts a block: its
  type (1: text; others, such as arrays, read as `NIL`) and length, big-endian, then its bytes, and `0xAF` in the last
  byte of its last block unless it fills it. Each write is one operation: the old memo's blocks are freed (back to the
  end of the file, or into the free list joined with their neighbours), the new memo takes the smallest free area it
  fits, or the end; then the free list's pages (1018 bytes each, made when the list first has an area, dropped when
  it empties at the end of the file; at most 126 areas, the smallest dropped past that), the changed free areas'
  headers (type 1001) and the header are written, and the file is cut when it ends sooner. A memo field is 10
  characters (the block number in digits) or, when asked for exactly 4, the number in 4 bytes (little-endian; a blank
  record holds zeros there). PACK copies the memos kept, each at the next block, without `0xAF`, and gives the file a
  fresh header; ZAP leaves a fresh header. Bytes 8-15 hold a writer's signature, which the reference fills with its
  name; xbase leaves them zero. A file without the FlexFile mark (FoxPro's, its header 512 bytes) keeps no list: what
  a write frees serves that write only; PACK and ZAP keep it so. PACK goes in the steps of DBT's, its first free block
  on disk past every copy at each step, so that a failure leaves the copies where no new memo goes. Where xbase does
  better than the reference: a memo whose length goes past the file is a Read error (the reference raises Corruption
  detected, or runs out of memory for a large length); a record's memo block that is not a memo of the file (in the
  header, past its end, a free area, a length past the end) is not freed when the field is written again, where the
  reference frees it, so that a damaged record never hands live blocks to a new memo; areas of the free list that are
  not areas of the file are left out of it; a FlexFile file does not grow past 4 GB, which its list's 32-bit offsets
  reach. The memo interface (`memo.h`: create, open, read, write, zap, PACK's steps) is the same for DBT and FPT.
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
- **CDX** (`cdx.c`, FoxPro's compound index as the reference writes it): 512-byte pages, every pointer a byte offset.
  A tag has a 1024-byte header (its root's offset, the key's length, its options: unique, partial, FOR condition,
  compact and compound always; `DESCENDING`; the key's and the condition's texts); the header at offset 0 is the
  directory's, a tag whose keys are the other tags' names (10 bytes, blanks after) and whose record numbers are their
  headers' offsets; it alone holds the `RCHB` mark, the free list's head (offset 4, little-endian) and a change counter
  (offset 8, big-endian). Tags are numbered in the order of their headers. A page: its kind (root, leaf), entry count
  and left and right neighbours on its level (every level linked). A branch holds whole entries (the key, then the
  record number and the child's offset, big-endian), each the last entry of its child's tree, at most
  `(512 - 12) / (key + 8)`; bytes past its last entry are left as the steps that changed it left them. A leaf holds its
  free bytes, the masks and bit widths of a descriptor's three fields and its byte length (3 to 8), one descriptor per
  entry (the record number, the bytes it repeats of the entry before it, the trailing blanks or zeros it leaves out,
  little-endian) and the rest of each key packed from the page's end down; its record numbers' width is the tag's
  default, widened a byte at a time as they need. Freed pages hold the next free page's offset; pages are taken from
  that list first, else from the end of the file (a tag's header always from the end, zeros written at once).
  - The build sorts the keys (then by record) and fills pages in one pass, every leaf as wide as the table's record
    count needs, a new page taken before the parent entry of the page it ends; the header is written last.
  - Adding and removing an entry recompute its and its neighbour's compression; the levels above keep their entries
    of the child up to date and balance it with its neighbours: a leaf that overflows spills into new pages or in front
    of its right neighbour, leaves with a page's room to spare between them merge (a page left empty unlinked and
    freed), branches share their entries evenly over as many pages as they need, and a full root moves its entries to
    a new page and stays where it is over it (a tag's header never changes). Removing a tag frees its pages, children
    before their branch, then its header's two halves (in a shared file they are left).
  - The free list's head is not read when a file is opened exclusive (pages freed by earlier sessions are not taken
    again), and is written when it changed; shared, it is read with the counter under every lock.

  The files are the reference's bytes after the same operations (`rdd_cdx_format.prg`: random changes of thousands of
  records, exclusive and shared, against its checksums; outside the tests, runs of tens of thousands of changes with
  keys of 1 to 240 bytes compared file by file). The pages kept in memory (512 per file, found by a hash of their
  offsets, made as pages come) are decoded when first read only as far as their header; their entries when a key is
  needed; a changed leaf's bytes are made from its entries once, when they are written. A tag that leaves its file
  (replaced, removed, its file made anew) finds no page any more, even if an order still names it.

Every format is checked against the reference's files: what one writes, the other reads, and after the same operations
the files are the same bytes (but the date of the last update). `tests/run/rdd_format.prg`, `rdd_memo_format.prg` and
`rdd_index_format.prg` and `rdd_cdx_format.prg` keep files the reference wrote and check both ways.

## Performance

- Navigation reads no record: `GO`, `SKIP` and `SEEK` set the record number, and the driver reads the record when a
  field, its deletion flag or a key is first used (`load`). In order, a `SKIP` moves the order's cursor (the path from
  the root to the entry, kept from one call to the next while the order does not change) to the next entry.
- Each open order keeps 128 pages (a hash from page to slot, the page used last checked first); pages changed are
  written on `COMMIT`, at closing, or when the cache needs their slot.
- A page is searched by halves over its offsets; a key compares with `memcmp`.
- Counting (`ordKeyCount()`, `ordKeyNo()`) and `ordKeyGoto()` read the pages' entry counts, never one entry at a
  time: the pages where the bounds fall (`ordKeyNo()`: the current entry's path) are searched, the subtrees between
  them counted whole (`count`, `count_before`, `select` of the format). An NTX page does not keep the size of
  the tree under it, so the cost is the pages between the bounds (`ordKeyGoto( n )`: those before entry n), each read
  once; a count that would read more pages than the file has is a broken file (a loop), `DBFNTX/1010`.
- Files are read and written at a position in one call (`pread`/`pwrite`, `ReadFile`/`WriteFile` with an offset;
  `xb_platform_file_read_at`), the last record with the end mark after it in one write. Records read in order come
  64 KB at a time (a shared table's under the file lock, or while a copy reads it: `snapshot`).
- An exclusive table keeps the records written at its end behind (`behind`, up to 64 KB of them) and writes them in
  one call when the chunk is full, before the file is read where they are or past them, and on `COMMIT`, `PACK`,
  `ZAP` and closing; reading one of them takes it from there. Appending costs a write a chunk instead of one a record;
  an error writing them is raised by the operation that wrote the chunk.
- Copies between files copy a record's bytes whole when the fields are the same, else a field's bytes when they are a
  value in the stored form; values are read and written only for the other fields ([Copies between
  files](#copies-between-files)).
- The build: the keys in one array (a key that is a character field alone is copied from the record, no block run),
  one sort, one pass of writes. Keeping orders up to date costs the keys' evaluation before and after a change, and
  the entries only of keys that changed.
- `bench/index.prg` times a build over 200,000 records, 100,000 SEEKs, a SKIP through and 20,000 appends with the
  order open. Measured once against the reference on one machine, the same program: the build and SEEK about as
  fast, SKIP and appends with an order open faster. Copies (`COPY TO`, `APPEND FROM` of tables, SDF and DELIMITED
  files, 200,000 records) take a third or less of the reference's time.

## Sharing, locks and concurrency

Tables are shared between programs, and between the areas (and, when they come, the tasks and threads) of one program,
with one set of rules: two
work areas on the same table behave the same whether they are in one task, in two tasks on different threads, or in
two programs. The file protocol is the reference's, so xbase and Clipper/Harbour programs share tables correctly; what
happens inside the process is xbase's own design, made for threads, tasks and `async`/`await`
([architecture.md](architecture.md#asyncawait)).

### The file protocol

Taken from the locks and writes the reference makes (observed with the system's lock table and a trace of its calls),
and checked by programs of both sharing tables, memos and indexes at once (each locking, changing, appending, reading
and keeping an index up to date while the other does); the Windows side by two xbase programs run under Wine.
Offsets are bytes of the file; POSIX takes them with `fcntl` locks, Windows with `LockFileEx` on the same bytes.

- `EXCLUSIVE` and `SHARED` opening (table, memo, index): on POSIX an exclusive or shared `flock`, not waiting; on
  Windows the share mode at opening (none, or read and write). Refused: `NetErr()`.
- Record lock (`RLock()`, `dbRLock( n )`): a write lock on byte 1,000,000,000 + n, not waiting.
- File lock (`FLock()`): a write lock on bytes 1,000,000,001 to 2,000,000,000, not waiting.
- Append (`APPEND BLANK`): a write lock on byte 1,000,000,000 (waiting) while the record count grows and the header
  is written; the new record is locked.
- Memo block added (DBT): a write lock on byte 0 of the memo file (waiting) while the block and the next free block
  are written. FPT: the same lock around every memo write, the header and the free list read again under it; a read
  lock on it while the header is read at opening.
- `DBFCDX` locks a table by FoxPro's scheme instead: record r's lock at byte `0x40000000 + (r-1) * record length +
  header length`, the file lock on `0x40000001` to `0x7FFFFFFD`, the append lock on `0x40000000`; while the table's
  production order file is open in the area, record r's at `0x7FFFFFFE - r`, the file lock on `0x77FFFFFF` to
  `0x7FFFFFFD`, the append lock on `0x7FFFFFFE` (programs that disagree on whether it is open lock different bytes,
  as the reference's do). A lock is released at both places, so that one taken before the production file opened or
  closed goes too. The header's flag of a production file is written under the append lock in a shared table.
- Index read and change (CDX): a read or write lock on byte `0x7FFFFFFE` of the file (waiting), then the free list's
  head and the change counter (8 bytes at offset 4) read: either changed, the pages kept are stale and the end of the
  file is found again. A change is written before the write lock goes, with the head and the counter plus one. The
  tags themselves are read at opening: a tag another program adds, removes or makes anew meanwhile is not seen until
  the file is opened again (as with the reference).
- Index read (NTX): a read lock on byte 1,000,000,000 of the index (waiting); the header is read, and a counter
  (bytes 2-3) other than the last one seen means pages kept from before are stale.
- Index change (NTX): a write lock on the same byte (waiting); the pages are written, then the header with the counter
  plus one. A changed key's old entry goes and the new one comes under one lock.
- An order's cursor is placed from the current record's key as it is now (the record read again, unless it has
  changes not yet written), so `SKIP` and `ordKeyNo()` follow a key another program changed.

- Records are read without locks. In shared mode the record count is the file's size (the header's may lag), and a
  record is read from the file each time it becomes current, and again when its lock (or the file lock) is had: a
  change is made to the record as it is then, never to what was read before (no update of another program is lost).
  Records read ahead serve only under the file lock, and go when it does.
- The shared append writes the new record with the end mark `0x1A` after it (under the append lock, so it is always
  the file's end); the reference leaves the mark out of shared tables.
- Writing a record in shared mode needs its lock or the file lock: otherwise `DBFNTX/1022` `Lock required`. A changed
  record is written when the record changes, is unlocked, or at `COMMIT`; its index entries then change under the
  index write lock.
- `RLock()` releases the area's other record locks first, `dbRLock( n )` adds one (`dbRLock()` with no record is
  `RLock()`); a shared `READONLY` table's locks are always had (it writes nothing); `dbRUnlock( [n] )`, `dbUnlock()`,
  `UNLOCK [ALL]`, `dbRLockList()`. `APPEND BLANK` in shared mode releases the record locks and locks the new record;
  when the append lock cannot be had it sets `NetErr()`. `USE` that cannot have the sharing it asks for sets
  `NetErr()` (`DBFNTX/1001` `Open error` for `dbUseArea`). `SET EXCLUSIVE` (on by default, as in Clipper) chooses
  when `USE` says neither `SHARED` nor `EXCLUSIVE`.
- In exclusive mode nothing of this costs anything: no locks, the record count kept in memory, pages and records
  read ahead and written lazily, as now.

### Inside the process

POSIX record locks belong to the process, not to a file handle: two handles of one process never block each other,
and closing any handle of a file drops every lock the process has on it. Windows locks belong to a handle. Neither is
what a program with several areas, tasks or threads needs, so the runtime arbitrates inside the process itself:

- **Shared files** (`core/shared_file.c`, `xb_shared`): a process-wide registry, keyed by the file's identity (device
  and inode; volume serial and file index on Windows, found from the path before anything is opened), so that two
  paths of one file are one entry. Each user has its own handle. On POSIX systems a handle is never closed while the
  file has users (closing it would drop their locks): one its user gave up is kept for the next user of the same
  access, and all of them close with the last user; on Windows, where locks belong to a handle, a user's handle closes
  with it. The entry knows each user's sharing (exclusive or shared): a second exclusive opening, or an exclusive one
  while others use the file, is refused inside the process, before a handle is opened, exactly as another process
  would be refused by the OS.
- **Lock manager**: each shared file keeps the locks its users hold (range, shared or exclusive, owner: the user,
  that is the area's opening). A request is checked first against the other users in the process, then taken from the
  OS only for bytes the process does not hold yet; a release gives back to the OS only the bytes no other user still
  holds (a write lock still held by another user under a released read lock stays a write lock). So areas, tasks and
  threads lock each other out with the same results as programs do, and closing one area never drops another's locks.
  On Windows each user's locks are its handle's, taken as they are asked.
- **Waiting**: record and file locks never wait (Clipper's `RLock()` and `FLock()` answer at once). The transient locks
  (append, memo, index) wait for another process: the OS lock is tried again after pauses growing to 63 ms, the
  registry's mutex released in between, so a wait never blocks the process's other users. With one thread, a transient
  lock held inside the process can only be the caller's own, which the rule below rules out: such a conflict is an
  error, never a wait.
  *(With threads)*: a holder inside the process is waited for on a condition of the shared file; a thread that waits
  is parked for the collector (a safepoint: a collection never waits for it), and an `async` task that waits gives its
  thread back (below).
- **No xBase code under a transient lock**: keys, conditions, filters and other blocks are evaluated before or after
  an index or memo lock, never while it is held, so a block can never wait for a lock its own thread holds. Record and
  file locks are the program's, held across its code as in Clipper; a task asking for a record another task of the
  same program holds gets `.F.`, as from another program.
- **Index pages**: in shared mode each index operation takes the index's read (or write) lock, reads the header and
  drops the pages it kept when the change counter moved; the cursor keeps a copy of its entry (key and record) and is
  placed on it again then. Changes are written before the write lock is released, the counter one higher; a change
  that fails half way leaves nothing to write later (the pages are read again). *(Planned)*:
  one page cache per index file in the process, read by many areas at once and changed by one at a time (a
  reader-writer lock), instead of one per open order.
- **Records**: each area keeps its current record's bytes and its own changes until they are written; nothing of a
  record is shared between areas, so no record lock is needed to read.
- **Where**: the lock list of an area and the commands in `area.c` and `dbcmd.c` (`xb_area_lock`, `xb_area_unlock`,
  `xb_area_append`, `xb_area_record_writable`, `xb_area_exclusive`); the driver's `lock` operation and its shared reads,
  appends and header in `dbf.c`; the memo lock in `dbt.c`; the index lock (`begin`, `finish`) in `ntx.c` and `cdx.c`.
- **Cost**: an exclusive table takes no lock and makes no extra call (the index benchmark is unchanged). Shared, each
  index operation is a lock, a read of the header's first 12 bytes and an unlock, each navigation a look at the table's
  size and the current key read from its record, each record read from the file; measured once on one machine, the index
  benchmark's work in shared mode (with a lock per change) took less time than the reference's.

### Tasks, threads and async/await

- *(Planned with tasks)*: a work area belongs to one task context (`ctx->areas`, already so), like its `SET` values; a
  task that resumes on another thread takes its areas with it. An area is never used by two tasks at once, so it needs
  no lock of its own. Handing an area from one task to another is explicit (planned: a function of xbase's own, the area
  closed in the first task and open in the second with its position and locks).
- What the runtime shares between threads is only the shared-file registry and the files' lock tables (one mutex) and,
  when they come, the shared page caches (reader-writer locks); no other global state in the database code.
- Disk I/O and waits for locks are synchronous calls; under `async`/`await` they run on the blocking pool, so a task
  awaiting a table operation frees its thread. Planned awaitable forms (for example, waiting for a record lock with a
  timeout and a `CancellationToken`) are xbase extensions on top of the same lock manager.
- The platform layer gains byte-range locks (`fcntl` / `LockFileEx`), the sharing lock (`flock` / share modes) and file
  identities; with threads, a condition variable and a reader-writer lock beside `xb_mutex`.

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
  the trees checked page by page (fill, order, every page reachable once, the free list). CDX files were also kept
  at once by programs of both, shared, each appending and changing keys while the others did, then walked by both
  (every tag's entries in order, one per record, each the record's key).
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
3. Done: `NTX` indexes: `INDEX ON` with its clauses, `SET INDEX`, `SET ORDER`, `SEEK`, `REINDEX`, `SET SOFTSEEK`,
   `SET UNIQUE`, conditions, scopes, key positions, `ordSkipUnique()`.
4. Done: sharing and locks: the shared-file registry, the lock manager, the file protocol (tables, memos, NTX), `SET
   EXCLUSIVE`, `RLock()`, `FLock()`, `dbRLock()`, `dbRUnlock()`, `dbRLockList()`, `UNLOCK`, `NetErr()`. With tasks and
   threads: waiting inside the process, shared page caches, handing areas between tasks.
5. `DBFCDX`: done: the driver, `FPT` memos, FoxPro's lock scheme, the order operations as an interface of the driver
   (`index.h`, NTX behind it), `CDX` indexes (tags, the production file, `ordDestroy()`, sharing).
6. Done: relations (`SET RELATION`), `AVERAGE`, `COPY`, `APPEND FROM`, `SDF`, `DELIM`, `CREATE`. Next: `SORT`,
   `TOTAL`, `UPDATE`, `JOIN`; `LIST` and `DISPLAY` with the console's redirections (`SET ALTERNATE`, `SET PRINTER`).
7. `SET DB ENGINE`; then SQL engines as libraries, and queries.
