// `x := NIL` lets go of what a local or a parameter held: the next collection frees it (its destructor runs), though
// the optimizer drops the store itself when x is not read after (a dead store) and the value lived in a frame slot (or
// several: a call's result that is the same array, a temporary of an expression) that nothing else takes. Checked by
// the live objects __GcCollect() counts, each case in a function of its own, at -O0 and -O2 (run0.*): straight on; an
// array also returned by the call that filled it (AFill()); inside a loop (versioned and inlined at -O2: the AEval()
// body's arguments, kept for its copies, are let go too once it is left); an array of tasks after a TaskWhenAll( a
// ):Wait() (its temporaries) with a send in a branch not taken; and an object whose destructor runs when it is let go,
// before the function returns; a parameter that received a value only the call held (a direct call lets go of the
// argument's slot once it read the value: the callee holds it alone). (The argument is a value of the caller's, not a
// LOCAL: at -O0 a LOCAL keeps what it holds while its function runs.) And what must stay: values live across the
// release keep their slots (a NIL in the middle of an expression, in nested loops that assign again, in a RECOVER, a
// LOCAL a codeblock captures and a parameter, each passed to a function called directly and read after, an object
// passed to a function (inlined at -O2: its call's arguments hold it while its body runs) and as Self, used first only,
// while another parameter is let go and a collection runs, in an inlined function that reads PCount() after it, in an
// ASYNC function that awaits after it, in a generator that yields after it), each checked by its result.
PROCEDURE Main()
   ? "straight:", Straight()
   ? "filled:", Filled()
   ? "in a loop:", InLoop()
   ? "tasks waited for:", Tasks( "x" )
   ? "destructor:", Destroyed()
   ? "parameter:", Param( Filled2( 50000 ) ), ParamCalled( Filled2( 50000 ) )
   ? "mid-expression:", MidExpression( .T. ), MidExpression( .F. )
   ? "passed, read after:", PassedKept()
   ? "kept while the call runs:", Running( Made( { .F. } ), { 1 } ), Made( { .F. } ):Run( { 1 } )
   ? "nested loops:", NestedLoops()
   ? "recover:", Recovered()
   ? "inlined with PCount():", Counted( { 1, 2 }, 5 ), Counted( { 1 } )
   ? "async:", Awaiting():Result
   ? "generator:", Yielded()
RETURN

STATIC FUNCTION Straight()
   LOCAL a := Array( 50000 ), n0, n1
   AEval( a, {| x, i | a[ i ] := { i } } ) // 50000 objects more
   n0 := __GcCollect()
   a := NIL
   n1 := __GcCollect()
RETURN n0 - n1 >= 50000

STATIC FUNCTION Filled()
   LOCAL a := Array( 1000 ), n0, n1
   AFill( a, { 1 } )
   AEval( a, {| x, i | a[ i ] := Space( 10 ) + Str( i ) } )
   n0 := __GcCollect()
   a := NIL
   n1 := __GcCollect()
RETURN n0 - n1 >= 1000

STATIC FUNCTION InLoop()
   LOCAL k, a, nLeft := 0, n0
   FOR k := 1 TO 3
      a := Array( 20000 )
      AEval( a, {| x, i | a[ i ] := { i } } )
      n0 := __GcCollect()
      a := NIL
      IF n0 - __GcCollect() >= 20000
         ++nLeft
      ENDIF
   NEXT
RETURN nLeft == 3

STATIC FUNCTION Tasks( cMode )
   LOCAL a := Array( 2000 ), k, n0, n1, oSource := CancellationTokenSource()
   FOR k := 1 TO 2000
      IF cMode == "never"
         a[ k ] := TaskRun( {|| oSource:Token } )
      ELSE
         a[ k ] := TaskRun( {|| k } )
      ENDIF
   NEXT
   TaskWhenAll( a ):Wait()
   n0 := __GcCollect()
   a := NIL
   n1 := __GcCollect()
RETURN n0 - n1 >= 2000

STATIC FUNCTION Destroyed()
   LOCAL o := Kept():New(), aLog := { .F. }
   o:aLog := aLog
   o := NIL
   __GcCollect()
RETURN aLog[ 1 ]

CLASS Kept
   VAR aLog
   VAR aBig INIT Array( 1000 )
   METHOD New() INLINE Self
   METHOD Run( b )
   DESTRUCTOR Done
ENDCLASS

// Self, used first only, while another parameter is let go and a collection runs (a send its caller cannot resolve:
// the object came from a macro).
METHOD Run( b ) CLASS Kept
   LOCAL aLog := ::aLog
   b := NIL
   __GcCollect()
RETURN IIf( aLog[ 1 ], "destroyed", "kept" )

METHOD PROCEDURE Done CLASS Kept
   ::aLog[ 1 ] := .T.
RETURN

// NIL in the middle of a longer expression, its temporaries live across it.
STATIC FUNCTION MidExpression( lRelease )
   LOCAL a := { 10, 20 }, b := { 1, 2, 3 }
RETURN Len( b ) + IIf( lRelease, ( a := NIL, __GcCollect(), Len( b ) ), a[ 2 ] ) + b[ 3 ] * 100

// Arrays assigned again after their NIL, and an outer one used throughout.
STATIC FUNCTION NestedLoops()
   LOCAL i, j, a, aOuter := { 0 }, nSum := 0
   FOR i := 1 TO 3
      a := { i, i * 2 }
      FOR j := 1 TO 2
         nSum += a[ j ] + aOuter[ 1 ]
         a := NIL
         __GcCollect()
         a := { i, i * 2 }
      NEXT
      aOuter[ 1 ] += i
      a := NIL
   NEXT
RETURN nSum + aOuter[ 1 ] * 1000

STATIC FUNCTION Recovered()
   LOCAL a := Array( 100 ), aKeep := { "kept" }, cOut := ""
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      a[ 101 ] := 1
   RECOVER
      a := NIL
      __GcCollect()
      cOut := aKeep[ 1 ]
   END SEQUENCE
RETURN cOut + IIf( a == NIL, " released", " not" )

// Inlined at -O2: PCount() after the NIL reads the arguments the body received.
STATIC FUNCTION Counted( a, n )
   a := NIL
   __GcCollect()
RETURN PCount() * 10 + IIf( n == NIL, 0, n )

STATIC ASYNC FUNCTION Awaiting()
   LOCAL a := Array( 1000 ), aKeep := { 7 }, x
   a := NIL
   x := AWAIT TaskRun( {|| 35 } )
   __GcCollect()
RETURN x + aKeep[ 1 ]

STATIC FUNCTION Yielded()
   LOCAL g := Gen(), cOut := "", x
   FOR EACH x IN g
      cOut += x
   NEXT
RETURN cOut

STATIC FUNCTION Gen()
   LOCAL a := Array( 1000 ), aKeep := { "y" }
   YIELD "x"
   a := NIL
   __GcCollect()
   YIELD aKeep[ 1 ]
   YIELD IIf( a == NIL, "z", "?" )
RETURN

// An array of n arrays of their own.
STATIC FUNCTION Filled2( n )
   LOCAL a := Array( n )
   AEval( a, {| x, i | a[ i ] := { i } } )
RETURN a

// Inlined at -O2 (the argument a value of the caller's that nothing else keeps).
STATIC FUNCTION Param( a )
   LOCAL n0, nLen
   n0 := __GcCollect()
   nLen := Len( a )
   a := NIL
RETURN n0 - __GcCollect() >= 50000 .AND. nLen == 50000

// Called at -O2 too (a function with BEGIN SEQUENCE is not inlined): the caller's slot and the parameter's own,
// moved into the variable that stands for it, both let go.
STATIC FUNCTION ParamCalled( a )
   LOCAL n0, nLen
   BEGIN SEQUENCE
      n0 := __GcCollect()
      nLen := Len( a )
      a := NIL
   END SEQUENCE
RETURN n0 - __GcCollect() >= 50000 .AND. nLen == 50000

// h, the first slot, is read through its cell (a codeblock captures it): the load is served in place, and the
// variable is not the call's to let go.
STATIC FUNCTION PassedKept()
   LOCAL h := { 1, 2 }, b := {|| h }
   Size( h )
RETURN Len( h ) + Len( Eval( b ) ) * 10 + ParamKept( { 1, 2, 3 } ) * 100

// a, moved into the LOCAL that stands for it, passed on and read after.
STATIC FUNCTION ParamKept( a )
   Size( a )
RETURN Len( a )

// Called directly at -O2 too (BEGIN SEQUENCE: not inlined).
STATIC FUNCTION Size( x )
   LOCAL n
   BEGIN SEQUENCE
      n := Len( x )
   END SEQUENCE
RETURN n

// A parameter used first only, while another is let go and a collection runs: an object is not destroyed while a
// function it was passed to still runs.
STATIC FUNCTION Running( o, b )
   LOCAL aLog := o:aLog
   b := NIL
   __GcCollect()
RETURN IIf( aLog[ 1 ], "destroyed", "kept" )

// An object only its result holds, of a class the caller cannot know (BEGIN SEQUENCE: never inlined).
STATIC FUNCTION Made( aLog )
   LOCAL o
   BEGIN SEQUENCE
      o := &( "Kept():New()" )
      o:aLog := aLog
   END SEQUENCE
RETURN o
