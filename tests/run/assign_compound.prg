// Assignments evaluate their operands once, left to right: x op= v and x := x op v read x before evaluating v (also
// for elements: a, then i, then the element, then v), as in Clipper.
STATIC s := 1

PROCEDURE Main()
   LOCAL x := 1, y := 2, a := { 1 }, b, e

   ? "x + (x := 5)", x + ( x := 5 )
   x := 1
   x := x + ( x := 5 )
   ? "x := x + (x := 5)", x
   x := 1
   y := x + ( x := 5 )
   ? "y := x + (x := 5)", y
   x := 1
   x := ( x := 5 ) + x
   ? "x := (x := 5) + x", x
   x := 1
   x += ( x := 5 )
   ? "x += (x := 5)", x
   x := 1
   x := x * ( x := 5 )
   ? "*", x
   x := 1
   x := x - ( x := 5 )
   ? "-", x
   x := 10
   x := x / ( x := 5 )
   ? "/", x
   x := 10
   x := x % ( x := 3 )
   ? "%", x
   x := 2
   x := x ^ ( x := 3 )
   ? "^", x
   x := 2
   x := x ** ( x := 3 )
   ? "**", x
   x := "a"
   x := x + ( x := "b" )
   ? "string", x

   x := 1
   x := x + y + ( x := 5 )
   ? "x + y + v", x
   x := 1
   x := x + 1 + ( x := 5 )
   ? "x + 1 + v", x
   x := 1
   x := ( x ) + ( x := 5 )
   ? "(x) + v", x
   x := 1
   x := ( x + ( x := 5 ) )
   ? "(x + v)", x
   x := 1
   y := 3
   x := x + ( y := 5 ) * ( x := 5 )
   ? "x + v * w", x
   x := 1
   x := x - - ( x := 5 )
   ? "x - -v", x
   x := 1
   x = x + ( x := 5 )
   ? "x = x + v", x
   x := 1
   y := x := x + ( x := 5 )
   ? "chain", x, y
   x := 1
   ? "in an expression", x := x + ( x := 5 )
   x := 1
   x := x + ( x := 5 ) + 100
   ? "x + v + 100", x

   s := s + ( s := 5 )
   ? "STATIC", s
   x := 1
   b := {|| x := x + ( x := 5 ) }
   Eval( b )
   ? "captured", x
   x := 1
   b := {|| x += ( x := 5 ) }
   Eval( b )
   ? "captured +=", x
   a := { 1, 2 }
   FOR EACH e IN a
      e := e + ( e := 5 )
   NEXT
   ? "FOR EACH", a[ 1 ], a[ 2 ]

   a := { 1 }
   a[ 1 ] := a[ 1 ] + ( a[ 1 ] := 5 )
   ? "a[1] := a[1] + v", a[ 1 ]
   a := { 1 }
   a[ 1 ] += ( a[ 1 ] := 5 )
   ? "a[1] += v", a[ 1 ]
   a := { 1, 2 }
   x := 1
   a[ x ] += ( x := 2 )
   ? "a[x] += (x := 2)", a[ 1 ], a[ 2 ]
   a := { 10 }
   x := 1
   ? "a[1]++", a[ 1 ]++, a[ 1 ]
   RETURN
