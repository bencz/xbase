// LOCALs holding an array of constants their function only reads (t[i], Len( t )) are read as tables of constant data,
// with each call's own array made only where a read fails: numbers, logicals and NIL read and counted; an index that is
// no number, whose error block changes the array it receives (e:args) and substitutes a value: the call's reads after
// it see the change, the next call a fresh table; an error block that resizes it (Len() of the call); an index out of
// bounds, which no value substitutes, left by a BREAK to the call's own BEGIN SEQUENCE: its RECOVER and what follows
// see the change; a first read that fails in a function that calls nothing; recursion, an error in the inner call only;
// a function inlined in a loop, failing in some iterations; folded quotients (no table); constant reads; a generator's
// table. And LOCALs that are no tables (assigned, an element assigned, passed on, captured, returned) give what they
// give.
STATIC s_nErrors := 0

PROCEDURE Main()
   LOCAL i, n := 0, bOld, aSeen

   FOR i := 1 TO 24
      n += DaysIn( ( i - 1 ) % 12 + 1 )
   NEXT
   ? "days:", n, DaysIn( 2 ), Count(), Flags( 1 ), Flags( 2 ), Flags( 3 ), Half( 2 )
   SET DECIMALS TO 4
   ? "run-time decimals:", Quotients( 1 ), Quotients( 2 ), Quotients( 3 ), "constant reads:", Ends( 5 )
   SET DECIMALS TO 2

   bOld := ErrorBlock( {| e | s_nErrors++, ChangeArgs( e ), IIf( e:canSubstitute, -1, Break( e ) ) } )
   ? "no number:", Changed( "x" ), Changed( 1 ), "errors:", s_nErrors
   ? "resized:", Resized(), Count()
   ? "first read:", Leaf( "k" ), Leaf( 2 )
   ? "recursion:", Nested( 3 )
   ? "inlined, failing:", Looped()
   ErrorBlock( bOld )
   ? "out of bounds:", Bounded( 13 ), Bounded( 2 ), BoundedHere()

   n := 0
   FOR i := 1 TO 1000
      n += DaysIn( i % 12 + 1 )
   NEXT
   aSeen := {}
   FOR EACH i IN Numbers()
      AAdd( aSeen, i )
   NEXT
   ? "inlined:", n, "generator:", Len( aSeen ), aSeen[ 1 ], aSeen[ Len( aSeen ) ]
   ? "no tables:", Written(), ElementSet(), PassedOn(), Captured(), Returned()[ 2 ]
RETURN

STATIC FUNCTION DaysIn( m )
   LOCAL aDays := { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
RETURN aDays[ m ]

STATIC FUNCTION Count()
   LOCAL a := { 1, 2, 3, 4, 5 }
RETURN Len( a )

// Folded quotients and powers whose decimals are SET DECIMALS' when they run: no table.
STATIC FUNCTION Quotients( k )
   LOCAL a := { 1 / 3, 2 ** 2, 10 % 3 }
RETURN a[ k ]

// Reads at constant positions and Len() of a table no read of which can fail and go on: its items.
STATIC FUNCTION Ends( k )
   LOCAL a := { 10, 20, 30 }
RETURN a[ 1 ] + a[ 3 ] + Len( a ) + k

STATIC FUNCTION Flags( k )
   LOCAL a := { .T., NIL, .F. }
RETURN a[ k ]

STATIC FUNCTION Half( k )
   LOCAL a := { 0.5, 1.25, -2 }
RETURN a[ k ]

// The error block's: an element of the array it receives changed, or the array resized (an index "resize").
STATIC PROCEDURE ChangeArgs( e )
   IF Len( e:args ) >= 1 .AND. ValType( e:args[ 1 ] ) == "A"
      IF ValType( e:args[ 2 ] ) == "C" .AND. e:args[ 2 ] == "resize"
         ASize( e:args[ 1 ], 20 )
      ELSE
         e:args[ 1 ][ 1 ] := 1000
      ENDIF
   ENDIF
RETURN

// A read with an index that is no number (k), then the first element: 1000 in the call that failed, 10 in the next.
STATIC FUNCTION Changed( k )
   LOCAL a := { 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120 }
   LOCAL x := a[ k ]
RETURN LTrim( Str( x ) ) + "/" + LTrim( Str( a[ 1 ] ) ) + "/" + LTrim( Str( Len( a ) ) )

STATIC FUNCTION Resized()
   LOCAL a := { 1, 2, 3 }
   LOCAL x := a[ "resize" ]
RETURN LTrim( Str( x ) ) + "/" + LTrim( Str( Len( a ) ) ) + "/" + ValType( a[ 20 ] )

STATIC FUNCTION Leaf( k )
   LOCAL a := { 7, 8 }
RETURN a[ k ] + a[ 1 ]

STATIC FUNCTION Nested( d )
   LOCAL a := { 5, 6, 7 }, c
   IF d == 0
      RETURN LTrim( Str( a[ "x" ] ) ) + ":" + LTrim( Str( a[ 1 ] ) )
   ENDIF
   c := Nested( d - 1 )
RETURN c + "," + LTrim( Str( a[ 1 ] ) )

// A small function called in a loop (inlined at -O2) whose read fails in some iterations only: each iteration is a call
// of its own, which sees the table whole.
STATIC FUNCTION Looped()
   LOCAL i, c := ""
   FOR i := 1 TO 4
      c += Pick( IIf( i % 2 == 1, "x", 2 ) ) + " "
   NEXT
RETURN c

STATIC FUNCTION Pick( k )
   LOCAL a := { 3, 4 }
   LOCAL x := a[ k ]
RETURN LTrim( Str( x ) ) + "/" + LTrim( Str( a[ 1 ] ) )

// An index out of bounds (k > 4) in the call's own BEGIN SEQUENCE, whose error block changes the array and BREAKs.
STATIC FUNCTION Bounded( k )
   LOCAL a := { 1, 2, 3, 4 }, x := 0, c := ""
   BEGIN SEQUENCE WITH {| e | ChangeArgs( e ), Break( e ) }
      x := a[ k ]
   RECOVER
      c := "recovered "
   END
RETURN c + LTrim( Str( x ) ) + "/" + LTrim( Str( a[ 1 ] ) )

// The same with a position known to be a number (a constant): the read after RECOVER sees the change too.
STATIC FUNCTION BoundedHere()
   LOCAL a := { 1, 2, 3, 4 }, x := 0, c := ""
   BEGIN SEQUENCE WITH {| e | ChangeArgs( e ), Break( e ) }
      x := a[ 5 ]
   RECOVER
      c := "recovered "
   END
RETURN c + LTrim( Str( x ) ) + "/" + LTrim( Str( a[ 1 ] ) ) + "/" + LTrim( Str( Len( a ) ) )

STATIC FUNCTION Numbers()
   LOCAL a := { 2, 4, 6, 8 }, i
   FOR i := 1 TO Len( a )
      YIELD a[ i ]
   NEXT
RETURN

STATIC FUNCTION Written()
   LOCAL a := { 1, 2 }
   a := { 3, 4 }
RETURN a[ 1 ]

STATIC FUNCTION ElementSet()
   LOCAL a := { 1, 2 }
   a[ 1 ] := 5
RETURN a[ 1 ] + a[ 2 ]

STATIC FUNCTION PassedOn()
   LOCAL a := { 1, 2, 3 }
RETURN Len( AClone( a ) ) + AScan( a, 3 )

STATIC FUNCTION Captured()
   LOCAL a := { 1, 2 }, b := {|| a[ 2 ] }
RETURN Eval( b )

STATIC FUNCTION Returned()
   LOCAL a := { 1, 2 }
RETURN a
