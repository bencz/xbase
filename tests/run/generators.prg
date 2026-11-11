// Generators: a function (or method) with YIELD returns an enumerator, a GENERATOR object whose MoveNext runs the
// body up to the next YIELD and whose Current is that element. FOR EACH goes through it (also DESCEND, which gathers
// the elements first), and so does a DO WHILE on MoveNext; one generator feeds another; nested loops, FOR and FOR EACH
// state, LOCALs and codeblocks that capture them survive between steps; a recursive method generator (a tree in
// order); an endless one left by EXIT; an error in a step ends it and reaches the caller's handler; arguments are
// taken by value; and many steps in a row.
PROCEDURE Main()
   Part1()
   Part2()
RETURN

STATIC PROCEDURE Part1()
   LOCAL x, g, c := ""
   FOR EACH x IN Range( 1, 5 )
      c += Str( x, 2 )
   NEXT
   ? "range:", c
   g := Squares( 4 )
   ? ValType( g ), g:ClassName()
   DO WHILE g:MoveNext()
      ?? " " + LTrim( Str( g:Current ) )
   ENDDO
   ? "after end:", g:MoveNext(), g:Current
   c := ""
   FOR EACH x IN Words( "alpha beta  gamma" )
      c += "[" + x + "]"
   NEXT
   ? "words:", c
   c := ""
   FOR EACH x IN Evens( Range( 1, 10 ) )
      c += Str( x, 3 )
   NEXT
   ? "chained:", c
   c := ""
   FOR EACH x IN Range( 1, 3 ) DESCEND
      c += Str( x, 2 )
   NEXT
   ? "descend:", c
   c := ""
   FOR EACH x IN Nested()
      c += x + " "
   NEXT
   ? "nested:", c
RETURN

STATIC PROCEDURE Part2()
   LOCAL x, c := "", t, e, n, blocks := {}
   t := TTree():New( 4, TTree():New( 2, TTree():New( 1 ), TTree():New( 3 ) ), TTree():New( 6, , TTree():New( 7 ) ) )
   FOR EACH x IN t:InOrder()
      c += Str( x, 2 )
   NEXT
   ? "tree:", c
   c := ""
   FOR EACH x IN Fib()
      IF x > 100
         EXIT
      ENDIF
      c += LTrim( Str( x ) ) + " "
   NEXT
   ? "infinite, exited:", c
   FOR EACH x IN Makers()
      AAdd( blocks, x )
   NEXT
   ? "blocks:", Eval( blocks[ 1 ] ), Eval( blocks[ 2 ] ), Eval( blocks[ 3 ] )
   ErrorBlock( {| o | Break( o ) } )
   c := ""
   BEGIN SEQUENCE
      FOR EACH x IN Failing()
         c += Str( x, 2 )
      NEXT
   RECOVER USING e
      c += " error " + e:operation
   END SEQUENCE
   ? "error:", c
   n := 7
   c := ""
   FOR EACH x IN ByValue( @n )
      c += Str( x, 3 )
   NEXT
   ? "args by value:", c, n
   n := 0
   FOR EACH x IN Range( 1, 300000 )
      n += x
   NEXT
   ? "sum:", n
RETURN

FUNCTION Range( a, b )
   LOCAL i
   FOR i := a TO b
      YIELD i
   NEXT
RETURN

FUNCTION Squares( n )
   LOCAL i := 0
   DO WHILE ++i <= n
      YIELD i * i
   ENDDO
RETURN

FUNCTION Words( s )
   LOCAL w := "", ch
   FOR EACH ch IN s
      IF ch == " "
         IF ! Empty( w )
            YIELD w
         ENDIF
         w := ""
      ELSE
         w += ch
      ENDIF
   NEXT
   IF ! Empty( w )
      YIELD w
   ENDIF
RETURN

FUNCTION Evens( e )
   LOCAL x
   FOR EACH x IN e
      IF x % 2 == 0
         YIELD x
      ENDIF
   NEXT
RETURN

FUNCTION Nested()
   LOCAL a, b
   FOR EACH a IN { "x", "y" }
      FOR EACH b IN Range( 1, 2 )
         YIELD a + LTrim( Str( b ) )
      NEXT
   NEXT
RETURN


FUNCTION Fib()
   LOCAL a := 0, b := 1, t
   DO WHILE .T.
      YIELD a
      t := a + b
      a := b
      b := t
   ENDDO
RETURN

FUNCTION Makers()
   LOCAL i
   FOR i := 1 TO 3
      YIELD {|| i * 10 }
   NEXT
RETURN

FUNCTION Failing()
   YIELD 1
   YIELD 1 + "x"
   YIELD 3
RETURN

FUNCTION ByValue( v )
   YIELD v
   v := 99
   YIELD v
RETURN

CLASS TTree
   VAR v, l, r
   METHOD New( v, l, r ) INLINE ::v := v, ::l := l, ::r := r, Self
   METHOD InOrder()
ENDCLASS

METHOD InOrder() CLASS TTree
   LOCAL x
   IF ::l != NIL
      FOR EACH x IN ::l:InOrder()
         YIELD x
      NEXT
   ENDIF
   YIELD ::v
   IF ::r != NIL
      FOR EACH x IN ::r:InOrder()
         YIELD x
      NEXT
   ENDIF
RETURN
