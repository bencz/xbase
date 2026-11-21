// FOR EACH over a string's bytes, at -O0 and -O2 (run0.*), the move of the loop's reference done in place (the shared
// one-byte string): bytes past 127 and NUL, the empty string, DESCEND, a value assigned to the loop variable (the
// string stays as it was), the variable captured by a codeblock (its cell) and a STATIC one, two loops over one
// string nested, a string of 1000 bytes, the variable's value after the loop.
STATIC s_x := "before"

PROCEDURE Main()
   LOCAL x := "kept", c := "", s := "A" + Chr( 0 ) + Chr( 200 ) + Chr( 255 ) + "z", y, b, n := 0, cLong
   FOR EACH x IN s
      c += Str( Asc( x ), 4 )
   NEXT
   ? "bytes:", c, "after:", x
   c := ""
   FOR EACH x IN ""
      c += "?"
   NEXT
   ? "empty:", "[" + c + "]"
   c := ""
   FOR EACH x IN "abc" DESCEND
      c += x
   NEXT
   ? "descend:", c
   s := "xyz"
   c := ""
   FOR EACH x IN s
      x := Upper( x )
      c += x
   NEXT
   ? "assigned:", c, s
   c := ""
   b := {|| y }
   FOR EACH y IN "cell"
      c += Eval( b )
   NEXT
   ? "captured:", c
   c := ""
   FOR EACH s_x IN "static"
      c += s_x + "."
   NEXT
   ? "static:", c, s_x
   c := ""
   FOR EACH x IN "ab"
      FOR EACH y IN "ab"
         c += x + y + " "
      NEXT
   NEXT
   ? "nested:", c
   cLong := Replicate( "0123456789", 100 )
   FOR EACH x IN cLong
      n += Asc( x ) - 48
   NEXT
   ? "long:", n
RETURN
