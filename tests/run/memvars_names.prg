// Names of memory variables met only while the program runs (made by macros, Type(), MemVarBlock(), RESTORE) are
// held while something uses them and freed after, their numbers given to other names: 20000 PRIVATEs of names made
// from data, created in a function and released at its return; Type() and macro reads of names no variable has; macro
// assignments that create PRIVATEs; PUBLICs made and released by name; RESTORE of a file with new names; tasks and
// threads that start with such a PRIVATE visible; ASYNC functions that see it and create their own; MemVarBlock()
// blocks of such a name: after each, the names in use come back to what they were (__MemNamesInUse(), once this
// thread let go of the names it keeps for its next macros; the task workers keep 32 at most each). A number freed
// and given to another name never shows that name's variable under the old one: Type() of the old name is "U", a
// block of the old name finds no variable, a macro compiled for the old name before still reaches the old name only.
// CLEAR MEMORY lets every name go. The file starts with "memvars_names", deleted at the end.
MEMVAR pKept

PROCEDURE Main()
   LOCAL nBase, i, c, x, b, bOld, aT, t
   nBase := __MemNamesInUse()
   __MemNamesInUse( .T. )
   nBase := __MemNamesInUse()
   ? "start", nBase > 0

   FOR i := 1 TO 20000
      x := Make( "dyn" + LTrim( Str( i ) ), i )
   NEXT
   ? "privates made and released:", x, Held( nBase )

   FOR i := 1 TO 20000
      x := Type( "zq" + LTrim( Str( i ) ) ) + Type( "zq" + LTrim( Str( i ) ) + " + 1" )
   NEXT
   ? "type of unknown names:", x, Held( nBase )

   FOR i := 1 TO 5000
      x := MacroAssign( "ma" + LTrim( Str( i ) ), i )
   NEXT
   ? "macro assignments:", x, Held( nBase )

   FOR i := 1 TO 3000
      c := "pub" + LTrim( Str( i ) )
      PUBLIC &c
      &c := i
      x := &c
      RELEASE &c
   NEXT
   ? "publics:", x, Type( "pub3000" ), Held( nBase )

   x := 0
   FOR i := 1 TO 300
      x += Shared( "sh" + LTrim( Str( i ) ), i )
   NEXT
   ? "tasks, threads, async:", x, Held( nBase ) <= 32 * 16 // the task workers keep theirs

   bOld := Block( "blk1", 5 )
   __MemNamesInUse( .T. ) // blk1's number freed now, for the next new name
   ? "block outlives its variable:", ValType( bOld ), Held( nBase )
   ? "number given to another name:", WhileOther( bOld ), Held( nBase )
   bOld := NIL
   ? "block dropped:", Held( nBase )

   b := &( "{|| oldname }" )
   ? "macro block of a name before:", Eval( Catch( b ) ), Reused( b ), Held( nBase )
   b := NIL

   PRIVATE pKept := 1
   FOR i := 1 TO 50
      c := "sv" + LTrim( Str( i ) )
      PRIVATE &c := i
   NEXT
   SAVE ALL LIKE sv* TO memvars_names
   x := Restored()
   ? "restore:", x, __MemNamesInUse() - nBase > 49
   CLEAR MEMORY
   ? "cleared:", Held( nBase ), Type( "pKept" ), Type( "sv7" )
   FErase( "memvars_names.mem" )
RETURN

// The names in use more than at the start, this thread's cache of names let go first.
STATIC FUNCTION Held( nBase )
RETURN __MemNamesInUse( .T. ) - nBase

STATIC FUNCTION Make( c, i )
   PRIVATE &c := i
RETURN &c + 1

STATIC FUNCTION MacroAssign( c, i )
   &c := i * 2
RETURN &( c + " + 1" )

STATIC FUNCTION Shared( c, i )
   LOCAL t, n := 0
   PRIVATE &c := i
   t := TaskRun( {|| &c } )
   n += t:Result
   n += ThreadStart( {|| &c } ):Join()
   n += Async( c ):Result
   n += &c // the async function's own PRIVATE did not touch this one
RETURN n

STATIC ASYNC FUNCTION Async( c )
   LOCAL n := Value( c )
   AWAIT TaskDelay( 0 )
RETURN n + Own( c ) - 1000

STATIC FUNCTION Value( c )
RETURN &c

// A PRIVATE of its own, in the environment of the ASYNC function calling it, which shares its caller's bindings.
STATIC FUNCTION Own( c )
   LOCAL d := c + "x"
   PRIVATE &d := 1000, &c := -1
RETURN &d

// A MemVarBlock() of a variable that ends at the return.
STATIC FUNCTION Block( c, i )
   PRIVATE &c := i
RETURN MemVarBlock( c )

// While a variable of another name has the number the block's name had (the last freed is the first given again).
STATIC FUNCTION WhileOther( bOld )
   LOCAL c := "other1"
   PRIVATE &c := 77
RETURN Type( "blk1" ) + " " + Type( "other1" ) + " " + Eval( Catch( bOld ) ) + " " + LTrim( Str( &c ) )

// A block that turns an error into its description.
STATIC FUNCTION Catch( b )
RETURN {|| Try( b ) }

STATIC FUNCTION Try( b )
   LOCAL x
   BEGIN SEQUENCE WITH {| e | Break( e:description ) }
      x := Eval( b )
   RECOVER USING x
   END SEQUENCE
RETURN x

// The block, compiled for a name with no variable, after a variable of another name took the number it may have had.
STATIC FUNCTION Reused( b )
   PRIVATE newname := "new"
RETURN Try( b )

STATIC FUNCTION Restored()
   LOCAL n := 0, i
   RESTORE FROM memvars_names ADDITIVE
   FOR i := 1 TO 50
      n += &( "sv" + LTrim( Str( i ) ) )
   NEXT
RETURN n
