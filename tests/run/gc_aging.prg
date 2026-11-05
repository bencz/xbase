// Young objects age at their first young collection and are promoted at the second. Old objects that still refer to
// young ones after a young collection stay recorded until those are promoted: an old array (small, and large with
// cards) holding an aged object with no further stores, and an object promoted while its child is still young.
PROCEDURE Main()
   LOCAL small := Array( 10 ), big := Array( 3000 ), holder, k
   __GcCollect()                                   // small and big are old
   small[ 3 ] := "small" + Str( 1 )
   big[ 2500 ] := "big" + Str( 2 )
   holder := { "holder" + Str( 3 ) }
   FOR k := 1 TO 4                                 // young collections with no further stores into small and big
      __GcCollect( .T. )
      IF k == 1
         AAdd( holder, { "child" + Str( 4 ) } )   // holder ages at the first collection; its child is new
      ENDIF
      Churn()
   NEXT
   ? small[ 3 ], big[ 2500 ], holder[ 1 ], holder[ 2 ][ 1 ]
   RETURN

PROCEDURE Churn()
   LOCAL i, a := {}
   FOR i := 1 TO 20000
      AAdd( a, "garbage" + Str( i ) )
   NEXT
   RETURN
