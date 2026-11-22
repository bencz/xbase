// STATICs reached directly (never passed by reference nor a FOR EACH variable in the module) next to ones that may
// hold a cell (one passed by reference in another function, one a FOR EACH variable): read and written in loops, by a
// callee between a read and its use, in codeblocks evaluated later, initialized once, at file and function level.
STATIC s_nDirect := 0, s_aDirect, s_nByRef := 10, s_xEach := "start"

PROCEDURE Main()
   LOCAL i, x, b, a := { 1, 2, 3 }
   STATIC s_nLocal := 100

   FOR i := 1 TO 1000
      s_nDirect += i
      s_nLocal -= 1
   NEXT
   ? "loops:", s_nDirect, s_nLocal

   s_nDirect := 5
   x := s_nDirect + Bump()        // read before the call: 5 + 6
   ? "callee between:", x, s_nDirect
   ? "in a list:", s_nDirect, Bump(), s_nDirect

   b := {| n | s_nDirect := s_nDirect * n }
   Eval( b, 3 )
   ? "block:", s_nDirect, Eval( {|| s_nDirect + s_nLocal } )

   s_aDirect := { 0, 0 }
   FOR i := 1 TO 10
      s_aDirect[ 1 ] += i
      s_aDirect[ 2 ] := s_aDirect[ 1 ] * 2
   NEXT
   ? "array in a static:", s_aDirect[ 1 ], s_aDirect[ 2 ]

   ? "by reference:", s_nByRef, Twice( @s_nByRef ), s_nByRef
   FOR i := 1 TO 3
      s_nByRef++
   NEXT
   ? "after:", s_nByRef

   FOR EACH s_xEach IN a
      s_xEach *= 10
   NEXT
   ? "each:", s_xEach, a[ 1 ], a[ 2 ], a[ 3 ]
   ? "counter:", Counter(), Counter(), Counter()
   RETURN

STATIC FUNCTION Bump()
   s_nDirect++
   RETURN s_nDirect

STATIC FUNCTION Twice( n )
   n *= 2
   RETURN n

STATIC FUNCTION Counter()
   STATIC s_n := 0
   s_n++
   RETURN s_n
