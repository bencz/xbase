// A pipeline of four ASYNC stages joined by bounded channels (capacity 256): a source sends 200,000 records as text
// ("id;value", the values from a fixed seed), a parser splits them into { id, value }, a stage maps each value to its
// square modulo 1,000,003 and keeps the even ones, and a sink sums them. Each stage awaits its sends and receives
// (AWAIT FOR EACH over its input) and closes its output when its input ends. The sums must be the same whatever the
// scheduling.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), c1 := Channel( 256 ), c2 := Channel( 256 ), c3 := Channel( 256 ), aSink, aTasks
   aTasks := { Source( c1, 200000 ), Parser( c1, c2 ), Square( c2, c3 ), Sink( c3 ) }
   aSink := AWAIT aTasks[ 4 ]
   AWAIT TaskWhenAll( aTasks )
   ? "pipeline_tasks", ElapsedMs() - t, ;
     IIf( aSink[ 1 ] == 49893566082 .AND. aSink[ 2 ] == 100073 .AND. aSink[ 3 ] == 10032092994, "ok", "WRONG" )
RETURN

ASYNC FUNCTION Source( cOut, n )
   LOCAL i, x := 4321
   FOR i := 1 TO n
      x := x * 16807 % 2147483647
      AWAIT cOut:SendAsync( LTrim( Str( i ) ) + ";" + LTrim( Str( x % 100000, 10, 0 ) ) )
   NEXT
   cOut:Close()
RETURN NIL

ASYNC FUNCTION Parser( cIn, cOut )
   LOCAL c, n
   AWAIT FOR EACH c IN cIn
      n := At( ";", c )
      AWAIT cOut:SendAsync( { Val( Left( c, n - 1 ) ), Val( SubStr( c, n + 1 ) ) } )
   NEXT
   cOut:Close()
RETURN NIL

ASYNC FUNCTION Square( cIn, cOut )
   LOCAL a, y
   AWAIT FOR EACH a IN cIn
      y := a[ 2 ] * a[ 2 ] % 1000003
      IF y % 2 == 0
         AWAIT cOut:SendAsync( { a[ 1 ], y } )
      ENDIF
   NEXT
   cOut:Close()
RETURN NIL

// { the sum of the values, their count, the sum of their ids }
ASYNC FUNCTION Sink( cIn )
   LOCAL a, nSum := 0, nCount := 0, nIds := 0
   AWAIT FOR EACH a IN cIn
      nSum += a[ 2 ]
      nCount++
      nIds += a[ 1 ]
   NEXT
RETURN { nSum, nCount, nIds }
