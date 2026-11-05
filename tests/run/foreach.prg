PROCEDURE Main()
   LOCAL a := { 1, 2, 3 }, v := "before", s := "abc", c, b := { 10, 20 }, x, y, i := 0, m
   FOR EACH v IN a
      v := v * 10
   NEXT
   ? a[1], a[2], a[3], v
   FOR EACH c IN s
      ?? c, ""
      c := "z"
   NEXT
   ? c, s
   FOR EACH v IN a DESCEND
      ?? v, ""
      IF v == 20
         EXIT
      ENDIF
      v := -1
   NEXT
   ? v, a[1], a[2], a[3]
   FOR EACH x, y IN a, b
      ? x, y
   NEXT
   FOR EACH x, y IN a, b DESCEND
      ? x, y
   NEXT
   FOR EACH v IN a
      i++
      IF i == 1
         AAdd( a, 99 )
      ENDIF
   NEXT
   ? i, Len( a )
   FOR EACH v IN {}
      ? "never"
   NEXT
   FOR EACH v IN a
      IF v < 50
         LOOP
      ENDIF
      ? "big", v
   NEXT
   m := { { 1, 2 }, { 3, 4 } }
   FOR EACH x IN m
      FOR EACH y IN x
         y := y + 100
      NEXT
   NEXT
   ? m[1][1], m[1][2], m[2][1], m[2][2]
   i := 0
   FOR EACH v IN a
      i++
      IF i == 1
         ASize( a, 1 )
      ENDIF
   NEXT
   ? i, Len( a )
   ? Sum( { 1, 2, 3, 4 } )
   RETURN

FUNCTION Sum( arr )
   LOCAL e, t := 0
   FOR EACH e IN arr
      IF e == 3
         RETURN t
      ENDIF
      t += e
   NEXT
   RETURN -1
