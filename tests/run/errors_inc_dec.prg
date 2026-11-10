// ++ and -- (also the step of a FOR without STEP) on what is not a number or a date fail as the operators themselves:
// argument errors 1086 (++) and 1087 (--), with the operand as their only argument, which the error block may
// substitute; += 1 and a FOR with a STEP fail as + (1081, both operands). On numbers and dates they add or subtract
// 1, an integer overflowing into a double.
PROCEDURE Main()
   LOCAL x := "a", d := SToD( "20240229" ), n := 5, i, a := { "a" }, o := ErrorNew(), k := 0
   ErrorBlock( {| e | Show( e ) } )
   x++
   ? "x", x
   x := "a"
   ? "pre", ++x
   x := NIL
   x--
   ? "dec", x
   d++
   ? "date", d, d--, d
   n++
   ? "n", n, --n
   a[ 1 ]++
   ? "elem", a[ 1 ]
   o:cargo := "c"
   o:cargo--
   ? "send", o:cargo
   x := "s"
   x += 1
   ? "add", x
   FOR i := "a" TO "c"
      ? "loop", i
      IF ++k > 3
         EXIT
      ENDIF
   NEXT
   k := 0
   FOR i := "a" TO "c" STEP 1
      ? "loopstep", i
      IF ++k > 3
         EXIT
      ENDIF
   NEXT
   n := 9223372036854775807
   n++
   ? "big", n > 9223372036854775806, ValType( n )
RETURN

STATIC FUNCTION Show( e )
   LOCAL a := e:args, s := "", k
   FOR k := 1 TO Len( a )
      s += " " + ValType( a[ k ] ) + ":" + IIF( ValType( a[ k ] ) == "C", a[ k ], IIF( a[ k ] == NIL, "NIL", ;
         LTrim( Str( a[ k ] ) ) ) )
   NEXT
   ? "err", e:subCode, e:operation, e:description, Len( a ), s, e:canSubstitute
RETURN "z"
