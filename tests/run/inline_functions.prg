// Calls of small functions of the module, which run where they are called, behave as calls: the call stack shows the
// function with its own lines (also to a function it calls and to an error), parameters take the arguments (NIL past
// the last, extra ones ignored, references through them), LOCALs start NIL on every call, a procedure gives NIL,
// nested and repeated calls, a RETURN from inside FOR EACH, and a BREAK out of one leave the stack as it was. An object
// a call made and dropped is gone once it returns, as its frame is. Recursive functions, those that read PCount() or
// create PRIVATEs stay calls.
STATIC s_nGone := 0

PROCEDURE Main()
   LOCAL i, n := 1, a := { 1, 2 }, e
   ? Twice( 21 ), Twice( "ab" ), Add3( 1, 2, 3 ), Add3( 1 ), Add3( 1, 2, 3, 4 )
   ? Where(), ProcName( 0 ), ProcLine( 0 )
   ? Outer( 5 )
   Bump( @n )
   Bump( @a[ 2 ] )
   ? n, a[ 2 ]
   FOR i := 1 TO 3
      ?? Fresh( i )
   NEXT
   ? ValType( Nothing() ), Fact( 5 ), Counted( 1, 2 ), Named()
   ? Sum( { 1, 2, 3, 4 } ), Sum( { 5 } )
   Drop()
   __GcCollect()
   ? "gone", s_nGone
   BEGIN SEQUENCE
      Breaks()
   RECOVER USING e
      ? "recovered", e, ProcName( 0 )
   END
   BEGIN SEQUENCE WITH {| o | Break( o ) }
      Fails( "a" )
   RECOVER USING e
      ? "error", e:description, e:operation, ProcName( 0 ), ProcLine( 0 )
   END
RETURN

FUNCTION Twice( x )
RETURN x + x

FUNCTION Add3( x, y, z )
RETURN ValType( x ) + ValType( y ) + ValType( z )

FUNCTION Where()
RETURN ProcName( 0 ) + "(" + LTrim( Str( ProcLine( 0 ) ) ) + ")<" + ProcName( 1 ) + "(" + ;
   LTrim( Str( ProcLine( 1 ) ) ) + ")"

FUNCTION Outer( n )
RETURN Inner( n ) + 1

FUNCTION Inner( n )
RETURN n * 10 + Len( ProcName( 1 ) + ProcName( 2 ) )

PROCEDURE Bump( x )
   x += 10
RETURN

FUNCTION Fresh( k )
   LOCAL t
   t := iif( t == NIL, 0, t ) + k
RETURN t

PROCEDURE Nothing()
RETURN

FUNCTION Fact( n )
RETURN iif( n <= 1, 1, n * Fact( n - 1 ) )

FUNCTION Counted( ... )
RETURN PCount()

FUNCTION Named()
   PRIVATE cName := "private"
RETURN Shown()

FUNCTION Shown()
RETURN M->cName

FUNCTION Sum( arr )
   LOCAL e, t := 0
   FOR EACH e IN arr
      IF e == 3
         RETURN t
      ENDIF
      t += e
   NEXT
RETURN -1

PROCEDURE Drop()
   Gone():New()
RETURN

PROCEDURE Breaks()
   Break( "out of " + ProcName( 0 ) )
RETURN

PROCEDURE Fails( x )
   x := x - 1
RETURN

CREATE CLASS Gone
   EXPORTED:
   METHOD New
   DESTRUCTOR Bye
ENDCLASS

METHOD New() CLASS Gone
RETURN Self

METHOD PROCEDURE Bye() CLASS Gone
   s_nGone++
RETURN
