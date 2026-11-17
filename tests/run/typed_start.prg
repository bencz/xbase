// The program's start is its first function when it has no public MAIN, here a STATIC one: its recursive calls with
// numbers go to its typed copy, and it stays the start (it is entered with no argument: NIL). A function reached only
// through a function reference, a codeblock and an INIT procedure after its calls went to typed copies stays too, and
// a function called with more argument types than it gets copies keeps the rest generic.
STATIC PROCEDURE Start( n )
   IF n == NIL
      ? "start", ValType( n )
      Start( 3 )
      ? "ref", Eval( @Twice(), 4 ), Eval( {| x | Twice( x ) }, 5 ), Twice( 6 )
      ? "kinds", Kinds( 1, .T. ), Kinds( .F., 2 ), Kinds( SToD( "20261116" ), 1 ), Kinds( 1, 1 ), Kinds( .T., .T. ), ;
         Kinds( 2, SToD( "20261117" ) ), Kinds( "a", 1 ), Kinds( NIL, 3 )
      RETURN
   ENDIF
   ?? "", n
   IF n > 0
      Start( n - 1 )
   ENDIF
RETURN

STATIC FUNCTION Twice( n )
RETURN n * 2

// Too large to inline: each call goes to a copy, up to the copies a function gets, and the rest to the function.
STATIC FUNCTION Kinds( a, b )
   LOCAL c := "", k, x
   FOR k := 1 TO 2
      x := iif( k == 1, a, b )
      DO CASE
      CASE ValType( x ) == "N"
         c += iif( x > 0, "N", "n" )
      CASE ValType( x ) == "L"
         c += iif( x, "L", "l" )
      CASE ValType( x ) == "D"
         c += iif( Empty( x ), "d", "D" )
      OTHERWISE
         c += ValType( x )
      ENDCASE
   NEXT
RETURN c

INIT PROCEDURE Early()
   ? "init", Twice( 1 )
RETURN

FUNCTION Other()
   ? "wrong start"
RETURN NIL
