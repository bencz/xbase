// merge-reads: an instance variable read three times in one expression is read once, guarded once as a number; the
// same computed position of a table read twice is computed and read once; a read kept whole (v), then the same read
// taken as a number twice after a guarded element: the second read keeps its own guard (the first one was not let
// through as a number), and merges with the third; a read after a method call or an assignment of it stays.
CLASS Pt
   VAR x INIT 3
   VAR y INIT 4
   METHOD Bump() INLINE ::x += 10
ENDCLASS

FUNCTION Sums( n )
   LOCAL o := Pt():New(), t := { 1, 2, 3 }, i, s := 0, v, d, c := ""
   FOR i := 1 TO n
      s += o:x * o:x + o:y * o:x
      s += t[ i % 3 + 1 ] * t[ i % 3 + 1 ]
      v := o:x
      d := t[ i % 3 + 1 ]
      s += d * d - d + o:x * o:x
      c += ValType( v )
      o:Bump()
      s += o:x
      o:x := i
      s += o:x
   NEXT
RETURN s
