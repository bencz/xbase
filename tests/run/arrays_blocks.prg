// Arrays, codeblocks and captured variables.
PROCEDURE Main()
   LOCAL a := { 1, "two", { 3, 4 } }, b, n := 10, bAdd, bCounter, i, s := "Hello" + " " + "World"
   LOCAL m[2, 3]
   ? Len( a ), a[1], a[2], a[3][2], a[3, 1], ValType( a ), ValType( a[3] )
   a[1] := 100
   a[3][2] += 5
   a[1]++
   ? a[1], a[3][2], Len( m ), Len( m[1] ), ValType( m[2, 3] )
   bAdd := {| x, y | x + y + n }
   ? Eval( bAdd, 1, 2 )
   n := 20
   ? Eval( bAdd, 1, 2 ), ValType( bAdd )
   bCounter := MakeCounter()
   ? Eval( bCounter ), Eval( bCounter ), Eval( bCounter )
   b := {}
   FOR i := 1 TO 5
      AAdd( b, i * i )
   NEXT
   ? Len( b ), b[5], ATail( b ), ASize( b, 2 ) == b, Len( b )
   ? Eval( {| x | Eval( {| y | y * n }, x ) + n }, 2 )
   ? s, Len( AClone( a ) ), AClone( a )[3] == a[3]
   RETURN

FUNCTION MakeCounter()
   LOCAL nCount := 0
   RETURN {|| ++nCount }
