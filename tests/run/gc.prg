// Many collections run while the program allocates: live data must survive
// them, garbage must be reclaimed.
PROCEDURE Main()
   LOCAL keep := {}, i, s, a, c := "x", nLive, bKeep, cBig := ""
   FOR i := 1 TO 2000000
      s := c + c
      a := { s, { i }, {|| i } }
      IF i % 10000 == 0
         AAdd( keep, a )
      ENDIF
   NEXT
   FOR i := 1 TO 2000
      cBig += "0123456789"            // strings past the small-object size
   NEXT
   bKeep := MakeBlock( 42 )
   nLive := __GcCollect()
   ? Len( keep ), keep[1][2][1], keep[200][2][1], keep[200][1], Len( cBig )
   ? Eval( bKeep ), Eval( keep[200][3] ), nLive < 2000
   RETURN

FUNCTION MakeBlock( n )
   LOCAL aData := { n, "kept by a block" }
   RETURN {|| aData[1] }
