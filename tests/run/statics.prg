STATIC s_count := 10, s_name := "file", s_list := { 1, "two", { 3 } }
STATIC s_nil, s_block := { | n | n * s_count }, s_today := { || Date() }

PROCEDURE Main()
   STATIC first := .T.
   LOCAL i
   ? s_count, s_name, Len( s_list ), s_list[ 3 ][ 1 ], s_nil
   FOR i := 1 TO 3
      ? Counter(), Bump()
   NEXT
   ? s_count
   ? first
   first := .F.
   ? Again()
   ? Eval( { || s_count + 1 } )
   ? Eval( Maker() ), Eval( Maker() )
   AAdd( s_list, 4 )
   ? Len( s_list ), Eval( s_block, 2 ), ValType( Eval( s_today ) )
   RETURN

FUNCTION Counter()
   STATIC n := 0
   n++
   RETURN n

FUNCTION Bump()
   s_count += 5
   RETURN s_count

FUNCTION Again()
   STATIC first := "own"
   RETURN first

FUNCTION Maker()
   STATIC k := 100
   k++
   RETURN { || k }
