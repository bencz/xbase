// The integer fast paths of +, -, *, the comparisons and FOR, and their way out to the runtime operators: overflow
// into doubles, mixed integer and double operands, strings and NIL, comparisons kept as values, FOR with negative
// and double steps.
PROCEDURE Main()
   LOCAL big := 9223372036854775807, small := -9223372036854775807, x, y, i, n, lt, s
   x := big + 1
   y := small - 10
   ? "overflow", ValType( x ), x > 9000000000000000000, ValType( y ), y < -9000000000000000000
   x := 3037000500 * 3037000500
   ? "mul overflow", ValType( x ), x > 9000000000000000000, ValType( 3 * 4 ), 3 * 4
   ? "mixed", 1 < 1.5, 2 >= 2.0, 3 == 3.0, 1 + 0.5, 7 - 2.25, 4 * 0.5
   ? "strings", "a" < "b", "b" == "b", "x" + "y"
   ? "nil", NIL == NIL, 1 == NIL, NIL != 0
   lt := 5 < 7
   n := 7 > 5
   ? "kept", lt, n, ValType( lt )
   s := 0
   FOR i := 10 TO 1 STEP -3
      s += i
   NEXT
   ? "down", s, i
   s := 0
   FOR i := 1 TO 2 STEP 0.5
      s += i
   NEXT
   ? "double step", s, i
   s := 0
   FOR i := 1 TO 5
      IF i % 2 == 1 .AND. i != 3
         s += i * i
      ENDIF
   NEXT
   ? "cond", s
   ? "neg", -5 + 3, 0 - 7, -2 * -3, -2 < -1, 5 <= -5
   RETURN
