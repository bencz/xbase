// Memory reached in several ways inside loops (the alias information the compiler gives LLVM must hold): elements
// and the variables holding their arrays, rows shared by arrays of arrays, arrays growing while a loop reads them,
// variables passed by reference, STATICs and captured variables changed through others, FOR EACH references.
STATIC s_total := 0

PROCEDURE Main()
   LOCAL a := { 1, 2, 3 }, row := { 10, 20 }, m, i, k := 0, b, x, n := 0
   m := { row, row, { 5, 6 } }
   FOR i := 1 TO 3
      m[ i, 1 ] += i
      k += m[ 1, 1 ] + row[ 1 ]
   NEXT
   ? k, row[ 1 ], m[ 3, 1 ]
   FOR i := 1 TO 6
      IF i % 2 == 0
         AAdd( a, a[ i - 1 ] * 10 )
      ENDIF
      k += a[ Len( a ) ]
   NEXT
   ? k, Len( a ), a[ Len( a ) ]
   Bump( @k, a )
   Bump( @a[ 2 ], a )
   ? k, a[ 2 ]
   b := {| v | s_total += v, n += v }
   FOR i := 1 TO 4
      Eval( b, i )
      AddStatic( @s_total, i )
   NEXT
   ? s_total, n
   FOR EACH x IN a
      x := x * 2
      a[ 1 ] += 1
   NEXT
   ? a[ 1 ], a[ 2 ], a[ 3 ]
   RETURN

PROCEDURE Bump( v, arr )
   LOCAL i
   FOR i := 1 TO Len( arr )
      v += arr[ i ]
      arr[ 1 ] := v % 7
   NEXT
   RETURN

PROCEDURE AddStatic( v, i )
   v += i * 100
   RETURN
