// Evaluation order, FOR starts read by codeblocks, multi-bracket LOCAL arrays, AClone of shared and cyclic arrays.
PROCEDURE Main()
   LOCAL a := { 1, 2 }, old, F, i, x := 5, b, c, aLog := {}
   LOCAL m[2][3]
   F := {|| a := { 7, 8 }, 1 }
   old := a
   a[ Eval( F ) ] := 5
   ? old[1], a[1]
   a := { 1, 2 }
   old := a
   a[ Eval( F ) ]++
   ? old[1], a[1]
   a := { 0, 0, 0 }
   i := 1
   a[ i++ ] := i
   ? a[1], a[2], i
   a := { 0, 0, 0 }
   i := 1
   a[ i ] := i++
   ? a[1], a[2], i
   a := { 1, 2 }
   b := a
   a[1] := ( a := { 7, 8 } )
   ? ValType( b[1] ), ValType( a[1] )
   b := {|| x }
   FOR x := Eval( b ) TO 6
      AAdd( aLog, x )
   NEXT
   ? Len( aLog ), aLog[1], aLog[2], Len( m ), Len( m[2] )
   a := { 1 }
   AAdd( a, a )
   c := AClone( a )
   ? Len( c ), Len( c[2] ), c[2] == c, c[2][2] == c
   b := { 1, 2 }
   c := AClone( { b, b } )
   ? c[1] == c[2], c[1] == b
   RETURN
