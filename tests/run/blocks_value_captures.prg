// Codeblocks and the variables they capture: by value where nothing assigns the variable after its declaration (a
// cell of the block's own), shared where something does, as Clipper shares them. Each case gives what sharing gives:
// a parameter passed by reference (the block sees the caller's later assignment), a LOCAL assigned after the block is
// made, in another block, by a FOR loop, by PValue(); blocks made in a loop and kept; a function returning a block,
// inlined; nested blocks; a method's Self; a generator's and an ASYNC function's parameter; WITH OBJECT.
PROCEDURE Main()
   LOCAL x := 1, b, a := {}, i, o, s := "", gen, t
   b := ByRef( @x )
   x := 5
   ? "by reference:", Eval( b )
   ? "assigned after:", AssignedAfter(), "in another block:", InAnotherBlock(), "by FOR:", ByFor()
   ? "by PValue:", ByPValue( 1 ), "not assigned:", NotAssigned( 3 )
   FOR i := 1 TO 5
      AAdd( a, Mk( i * 10 ) )
   NEXT
   FOR i := 1 TO Len( a )
      s += Str( Eval( a[ i ], i ), 4 )
   NEXT
   ? "kept:", s
   s := 0
   FOR i := 1 TO 1000
      s += Eval( Mk( i % 7 ), i )
   NEXT
   ? "inlined:", s
   ? "nested:", Eval( Eval( Nested( 4 ) ), 5 )
   o := TPoint():New( 3, 4 )
   ? "self:", Eval( o:Getter() ), Eval( o:Scaled( 2 ) )
   s := ""
   FOR EACH t IN Gen( 7 )
      s += Str( Eval( t ), 3 )
   NEXT
   ? "generator:", s
   ? "async:", Eval( AWAIT Later( 9 ) )
   ? "with object:", WithObject()
   RETURN

STATIC FUNCTION ByRef( n )
   RETURN {|| n }

STATIC FUNCTION AssignedAfter()
   LOCAL n := 1, b := {|| n }
   n := 2
   RETURN Eval( b )

STATIC FUNCTION InAnotherBlock()
   LOCAL n := 1, b := {|| n }, c := {|| n := 3 }
   Eval( c )
   RETURN Eval( b )

STATIC FUNCTION ByFor()
   LOCAL i, a := {}
   FOR i := 1 TO 3
      AAdd( a, {|| i } )
   NEXT
   RETURN Eval( a[ 1 ] ) + Eval( a[ 3 ] )

STATIC FUNCTION ByPValue( n )
   LOCAL b := {|| n }
   PValue( 1, 7 )
   RETURN Eval( b )

STATIC FUNCTION NotAssigned( n )
   LOCAL k := n * 2, b := {| x | x + k + n }
   RETURN Eval( b, 1 )

STATIC FUNCTION Mk( k )
   RETURN {| x | x + k }

STATIC FUNCTION Nested( n )
   LOCAL m := n + 1
   RETURN {|| {| x | x + n + m } }

STATIC FUNCTION Gen( n )
   LOCAL k
   FOR k := 1 TO 3
      YIELD {|| n + k }
   NEXT
   RETURN

ASYNC FUNCTION Later( n )
   AWAIT TaskYield()
   RETURN {|| n * 2 }

STATIC FUNCTION WithObject()
   LOCAL a := {}, o
   FOR EACH o IN { TPoint():New( 1, 2 ), TPoint():New( 5, 6 ) }
      WITH OBJECT o
         AAdd( a, {|| :x } )
      END
   NEXT
   RETURN Str( Eval( a[ 1 ] ), 2 ) + Str( Eval( a[ 2 ] ), 2 )

CLASS TPoint
   VAR x
   VAR y
   METHOD Init( x, y )
   METHOD Getter() INLINE {|| ::x + ::y }
   METHOD Scaled( k ) INLINE {|| ( ::x + ::y ) * k }
ENDCLASS

METHOD Init( x, y ) CLASS TPoint
   ::x := x
   ::y := y
   RETURN Self
