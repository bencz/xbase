// A statement `target = value` assigns to a variable or an element, also written in parentheses; `=` inside the value
// compares (`x = y = 3` is `x := ( y = 3 )`); other statements that start like a target are expressions.
PROCEDURE Main()
   LOCAL a := { 1, 2 }, y := 0, x := 0, m := { { 1, 2 }, { 3, 4 } }
   a[1] = 5
   ? a[1]
   a[2] = y = 0
   ? a[2]
   x = y = 3
   ? x, y
   a[1] = a[1] + 1
   ? a[1]
   (x) = 9
   ? x
   m[2, 1] = "x"
   m[1][2] = "y"
   ? m[2][1], m[1, 2]
   a[1]++
   ? a[1]
   a[2] := 8
   ? a[2]
   ( Len( a ) )
   RETURN
