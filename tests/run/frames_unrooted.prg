// An element read whose one use is the operator right after it lives out of the frame; its runtime calls take it
// through slots the collector sees: an object read from an array and compared (and negated) through operators of its
// class that collect and allocate meanwhile, the object intact after; an element read that fails, whose error block
// collects and gives a value in its place; elements compared with NIL in a leaf (no frame) and in loops. And a loop
// that makes calls clears, where it is entered, the slot of the result of a call made before it that it does not use:
// the object it holds is destroyed by the collections the loop's calls make (a direct call that returns a number, which
// clears no slot of its own).
STATIC s_nGone := 0

PROCEDURE Main()
   LOCAL a := { TEq():New( 5 ), TEq():New( 7 ) }, i, n := 0, x, b, lMade

   ? "operator:", a[ 1 ] == 5, a[ 2 ] == 5, a[ 1 ] != 5, ( -a[ 2 ] ):n, a[ 1 ]:n, a[ 2 ]:nAsked
   FOR i := 1 TO 200
      IF a[ i % 2 + 1 ] == 7
         n++
      ENDIF
   NEXT
   ? "in a loop:", n, a[ 1 ]:nAsked + a[ 2 ]:nAsked, a[ 2 ]:n

   b := ErrorBlock( {| e | __GcCollect(), Replicate( "x", 100 ) } )
   x := { 1, 2 }
   ? "substituted:", Len( x[ "k" ] + "y" ), x[ 2 ] + 1
   ErrorBlock( b )

   ? "leaf:", Leaf( { NIL } ), Leaf( { 1 } ), Leaf( { "s" } )

   s_nGone := 0
   lMade := TGone():New() != NIL // the send's result: its slot would keep it past the comparison, which calls nothing
   n := 0
   FOR i := 1 TO 3
      n += Collected() // a direct call that returns a number: no slot of its own is cleared for it
   NEXT
   ? "made, gone in the loop:", lMade, n
RETURN

STATIC FUNCTION Leaf( a )
   IF a[ 1 ] == NIL
      RETURN "nil"
   ENDIF
RETURN ValType( a[ 1 ] )

// Collects, then tells how many objects were destroyed.
STATIC FUNCTION Collected()
   __GcCollect()
RETURN s_nGone

PROCEDURE Gone()
   s_nGone++
RETURN

CLASS TEq
   VAR n
   VAR nAsked INIT 0
   METHOD New( n ) INLINE ::n := n, Self
   METHOD Eq( x ) OPERATOR "=="
   METHOD Ne( x ) OPERATOR "!="
   OPERATOR "-" UNARY INLINE __GcCollect(), TEq():New( -::n )
ENDCLASS

// Each comparison collects (moving what it can) and allocates before it answers.
METHOD Eq( x ) CLASS TEq
   LOCAL c := Replicate( "z", 1000 )
   __GcCollect()
   ::nAsked++
RETURN ::n == x .AND. Len( c ) == 1000

METHOD Ne( x ) CLASS TEq
   __GcCollect()
   ::nAsked++
RETURN ::n != x

CLASS TGone
   VAR nId INIT 42
   DESTRUCTOR Bye()
ENDCLASS

METHOD PROCEDURE Bye CLASS TGone
   Gone()
RETURN
