// FOR EACH variables are references to the elements: direct writes to the array, RETURN inside the loop,
// codeblocks created in the loop, STATIC loop variables read by other functions.
STATIC s_v := "sv"
PROCEDURE Main()
   LOCAL a := { 1, 2, 3 }, s := "abc", c, v, b, bl := {}, i := 0, w := "orig", m
   FOR EACH c IN s
      c := c + "!"
      ?? c, ""
   NEXT
   ? s, c
   FOR EACH v IN a
      i++
      b := { || v }
      a[ i ] := v * 10
      ?? Eval( b ), v, ""
      AAdd( bl, { | x | v := x } )
   NEXT
   ? a[1], a[2], a[3]
   Eval( bl[ 1 ], 77 )
   ? a[1], v
   FOR EACH s_v IN a
      ?? ReadS(), ""
   NEXT
   ? s_v
   ? Ret1( a ), s_v
   a := { 1, 2, 3 }
   SetFirst( a )
   ? a[1]
   i := 0
   FOR EACH v IN a
      i++
      a[ i ] := v * 100
   NEXT
   ? a[1], a[2], a[3]
   FOR EACH v IN a
      Bump( a )
   NEXT
   ? a[1], a[2], a[3]
   FOR EACH w IN a
      w := 7
      ?? a[1], ""
      EXIT
   NEXT
   ? w, a[1]
   m := {}
   FOR EACH v IN { 1, 2, 3 }
      AAdd( m, { || v } )
   NEXT
   ? Eval( m[1] ), Eval( m[2] ), Eval( m[3] ), v
   a := { 1, 2 }
   FOR EACH v IN a
      v := AAdd( a, v + 10 )
      IF Len( a ) > 40
         EXIT
      ENDIF
   NEXT
   ? Len( a ), a[1], a[2], a[3]
RETURN
FUNCTION ReadS()
RETURN s_v
FUNCTION Ret1( a )
   FOR EACH s_v IN a
      IF s_v == 20
         RETURN s_v
      ENDIF
   NEXT
RETURN 0
PROCEDURE SetFirst( a )
   LOCAL v
   FOR EACH v IN a
      v := 99
      RETURN
   NEXT
RETURN
PROCEDURE Bump( a )
   LOCAL k
   FOR k := 1 TO Len( a )
      a[ k ]++
   NEXT
RETURN
