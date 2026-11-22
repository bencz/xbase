// An element read at a constant position of an array literal that nothing changes or lets out is the value it was
// made with (folded: codeblocks then run inline, and an array that inlined bodies only held goes). The elements are
// computed once, in order, when the literal is made; a position out of bounds still raises the bound error; a row of
// an array of arrays is the same array; an array written, passed to a function that is not inlined or read as an
// argument (PCount()) stays one.
STATIC s_nCalls := 0

PROCEDURE Main()
   LOCAL oErr
   ? "elements:", Elements()
   ? "sum:", Sums()
   ? "row:", RowShared()
   ? "written:", Written()
   ? "counted:", Counted( { 5, 6 } ), Dot( { 1, 2, 3 }, { 4, 5, 6 } )
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      ? { 7, 8 }[ 3 ]
   RECOVER USING oErr
      ? "bound:", oErr:subCode, oErr:description
   END SEQUENCE
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      ? OutOfBounds()
   RECOVER USING oErr
      ? "zero:", oErr:subCode
   END SEQUENCE
   ? "blocks:", Blocks()
   ? "changed by a handler:", Handled( "x" )
RETURN

// An index that is no number: its error's handler receives the array (its arguments), changes it and substitutes a
// value; the reads after see the change.
STATIC FUNCTION Handled( x )
   LOCAL a := { 1, 2 }, bOld := ErrorBlock( {| e | e:args[ 1 ][ 1 ] := 99, 0 } ), r
   r := Str( a[ x ] ) + Str( a[ 1 ] )
   ErrorBlock( bOld )
RETURN r

STATIC FUNCTION Elements()
   LOCAL a := { Next(), Next() * 10, {|| s_nCalls * 100 } }
RETURN Str( a[ 1 ] ) + Str( a[ 2 ] ) + Str( Eval( a[ 3 ] ) ) + Str( s_nCalls )

STATIC FUNCTION Sums()
   LOCAL i, n := 0
   FOR i := 1 TO 1000
      n += Sum3( { i, i * 2, i * 3 } )
   NEXT
RETURN n

STATIC FUNCTION RowShared()
   LOCAL r := { { 1, 2 }, "x" }, x := r[ 1 ]
   x[ 1 ] := 99
RETURN Str( r[ 1 ][ 1 ] ) + iif( x == r[ 1 ], " same", " other" )

STATIC FUNCTION Written()
   LOCAL b := { 1, 2, 3 }
   b[ 2 ] := 20
RETURN Str( b[ 1 ] ) + Str( b[ 2 ] ) + Str( b[ 3 ] )

STATIC FUNCTION OutOfBounds()
   LOCAL a := { 7, 8 }
RETURN a[ 0 ]

STATIC FUNCTION Next()
   s_nCalls++
RETURN s_nCalls

STATIC FUNCTION Sum3( v )
RETURN v[ 1 ] + v[ 2 ] + v[ 3 ]

STATIC FUNCTION Dot( u, v )
RETURN u[ 1 ] * v[ 1 ] + u[ 2 ] * v[ 2 ] + u[ 3 ] * v[ 3 ]

STATIC FUNCTION Counted( v )
RETURN PCount() * 100 + v[ 1 ] + Len( v )

STATIC FUNCTION Blocks()
   LOCAL k := 3, aB := { {|| 7 }, {| x, y | x * 2 + y }, {| x | x + k } }, i, n := 0
   FOR i := 1 TO 10
      n += Eval( aB[ 1 ] ) + Eval( aB[ 2 ], i, 1 ) + Eval( aB[ 3 ], i )
   NEXT
RETURN n
