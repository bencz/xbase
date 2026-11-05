// DO ... WITH passes a variable written alone by reference, other arguments by value.
PROCEDURE Main()
   LOCAL x := 1, y := 2, a := { 1 }
   DO SetTo WITH x, 5
   ? "var", x
   DO SetTo WITH ( y ), 5
   ? "paren", y
   DO SetTo WITH a[ 1 ], 5
   ? "elem", a[ 1 ]
   DO SetTo WITH x + 0, 7
   ? "expr", x
   DO SetTo WITH @y, 9
   ? "@", y
   RETURN

PROCEDURE SetTo( v, n )
   v := n
   RETURN
