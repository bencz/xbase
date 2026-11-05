// Deep recursion: every level's frame is on the machine stack, linked into the context. Collections run at the
// deepest level, and every level then checks that its locals survived (Bad() counts the levels that lost them).
PROCEDURE Main()
   ? Depth( 20000, {} )
   ? Len( Build( 3000 ) )
   ? Depth( 200000, {} )
   RETURN

FUNCTION Depth( n, acc )
   LOCAL s := Replicate( "x", n % 50 ), a := { n, s, { n * 2 } }
   IF n == 0
      __GcCollect( .T. )
      __GcCollect()
      RETURN Len( acc )
   ENDIF
   IF n % 1000 == 0
      AAdd( acc, a )
   ENDIF
   RETURN Depth( n - 1, acc ) + Bad( a[ 1 ] != n .OR. a[ 2 ] != s .OR. Len( s ) != n % 50 .OR. a[ 3, 1 ] != n * 2 )

FUNCTION Build( n )
   LOCAL tail, mine := Chr( 65 + n % 26 )
   IF n == 0
      __GcCollect()
      RETURN ""
   ENDIF
   tail := Build( n - 1 )
   RETURN tail + IIf( mine == Chr( 65 + n % 26 ), mine, "?" )

FUNCTION Bad( lost )
   RETURN IIf( lost, 1, 0 )
