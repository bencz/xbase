// A stack that went deep gives its pages back once it is shallow again: on a thread of 16 MB of stack, a recursion
// the stack error ends, recovered by BEGIN SEQUENCE, leaves the resident memory about where it was before (the pages
// it touched, more than 10 MB, given back), and a second one reaches the same depth (the stack grows again as before);
// on the program's thread a recursion of some 40 MB that returns gives them back at the next sequence that recovers,
// and so do a codeblock made from text and a function forwarding `...`, recursing past the mark (no stack error
// there: it is only the mark), each after a while (a thread gives its pages back at most every 100 ms). Where the
// system does not tell the resident memory, the comparisons hold as such.
MEMVAR bMacro, nDeep

PROCEDURE Main()
   LOCAL a := {}
   ThreadStart( {|| Overflows( a ) } ):Join()
   ? "thread: grew", a[ 1 ], "given back", a[ 2 ], "same depth again", a[ 3 ] == a[ 4 ], a[ 3 ] > 1000
   ? "program:", Returns()
   ? "macro:", Macro()
   ? "forwarded:", Forwarded()
RETURN

STATIC PROCEDURE Overflows( a )
   LOCAL nBefore := Resident(), nAt := 0, nDepth1, nDepth2, nAfter
   nDepth1 := Depth( .T., @nAt )
   nAfter := Resident()
   nDepth2 := Depth( .F. )
   AAdd( a, nAt == 0 .OR. nAt - nBefore > 10 * 1048576 )
   AAdd( a, nAfter == 0 .OR. nAfter - nBefore < 4 * 1048576 )
   AAdd( a, nDepth1 )
   AAdd( a, nDepth2 )
RETURN

// How deep Recurse() went before the stack error, which the handler takes (with the resident memory then, lMeasure).
STATIC FUNCTION Depth( lMeasure, nAt )
   LOCAL nLevel := 0, e
   BEGIN SEQUENCE WITH {| e | iif( lMeasure, nAt := Resident(), NIL ), Break( e ) }
      Recurse( @nLevel )
   RECOVER USING e
   END SEQUENCE
RETURN nLevel

STATIC FUNCTION Recurse( nLevel )
   nLevel++
RETURN Recurse( @nLevel ) + 1

STATIC FUNCTION Returns()
   LOCAL nBefore := Resident(), nAfter
   PRIVATE nDeep := 0
   Deep( 1 )
   Settle()
   nAfter := Resident()
RETURN Grown( nBefore, nDeep, nAfter )

STATIC FUNCTION Deep( n )
   IF n == 120000
      nDeep := Resident()
      RETURN 0
   ENDIF
RETURN Deep( n + 1 ) + 1

STATIC FUNCTION Macro()
   LOCAL nBefore := Resident(), n, nAfter
   PRIVATE nDeep := 0
   PRIVATE bMacro := &( "{| n | iif( n == 0, ( nDeep := __GcMemory()[ 3 ], 0 ), Eval( bMacro, n - 1 ) + 1 ) }" )
   n := Eval( bMacro, 20000 )
   Settle()
   nAfter := Resident()
RETURN LTrim( Str( n ) ) + " " + Grown( nBefore, nDeep, nAfter )

STATIC FUNCTION Forwarded()
   LOCAL nBefore := Resident(), n, nAfter
   PRIVATE nDeep := 0
   n := Forward( 60000, "a", "b" )
   Settle()
   nAfter := Resident()
RETURN LTrim( Str( n ) ) + " " + Grown( nBefore, nDeep, nAfter )

STATIC FUNCTION Forward( n, ... )
   IF n == 0
      nDeep := Resident()
      RETURN PCount()
   ENDIF
RETURN Forward( n - 1, ... ) + 1

// Past the 100 ms since the thread last gave pages back, a sequence that recovers (where a stack gives them back).
STATIC PROCEDURE Settle()
   LOCAL nStart := ElapsedMs()
   DO WHILE ElapsedMs() - nStart < 150
   ENDDO
   BEGIN SEQUENCE
      Break( NIL )
   END SEQUENCE
RETURN

// Whether the stack grew more than 8 MB, and gave it back (within 4 MB of where it began).
STATIC FUNCTION Grown( nBefore, nDeep, nAfter )
RETURN "grew " + iif( nDeep == 0 .OR. nDeep - nBefore > 8 * 1048576, ".T.", ".F." ) + " given back " + ;
   iif( nAfter == 0 .OR. nAfter - nBefore < 4 * 1048576, ".T.", ".F." )

STATIC FUNCTION Resident()
RETURN __GcMemory()[ 3 ]
