// An error block that substitutes a value for a failed operation inside loops the compiler types: the value is of
// the operation's own type (an integer) but outside the range its operands would give, so whatever computes with it
// (a product that then overflows into a double, a comparison, a FOR limit) must see it as it is. Operations: & with a
// constant, unary minus, + - *, Int() of a quotient, a comparison whose bound would apply on its branch, a FOR's limit
// and step; the substituted values are large positive and negative integers and a double.
PROCEDURE Main()
   LOCAL bOld := ErrorBlock( {|| 1099511627776 } )
   ? "bitand:", BitMask()
   ? "minus:", Minus()
   ? "arithmetic:", Arith()
   ? "quotient:", Quotient()
   ? "compared:", Compared()
   ErrorBlock( {|| -4611686018427387904 } )
   ? "negative:", BitMask(), Arith()
   ErrorBlock( {|| 2.5 } )
   ? "double:", BitMask(), Minus(), Arith()
   ErrorBlock( {|| 3 } )
   ? "limit:", Limit()
   ErrorBlock( bOld )
RETURN

STATIC FUNCTION BitMask()
   LOCAL i, x, y, s := ""
   FOR i := 1 TO 3
      x := iif( i == 2, "a", i )
      y := x & 255
      s += LTrim( Str( ( y * 1000000000000000 + 1 ) / 1000000000000000, 25, 3 ) ) + " "
   NEXT
RETURN s

STATIC FUNCTION Minus()
   LOCAL i, x, y, s := ""
   FOR i := 1 TO 3
      x := iif( i == 2, "a", i )
      y := -x
      s += LTrim( Str( ( y * 1000000000000000 - 1 ) / 1000000000000000, 25, 3 ) ) + " "
   NEXT
RETURN s

STATIC FUNCTION Arith()
   LOCAL i, x, y, s := ""
   FOR i := 1 TO 3
      x := iif( i == 2, "a", i )
      y := ( x + 1 ) * 2 - 3
      s += LTrim( Str( ( y * 1000000000000000 ) / 1000000000000000, 25, 3 ) ) + " "
   NEXT
RETURN s

STATIC FUNCTION Quotient()
   LOCAL i, x, y, s := ""
   FOR i := 1 TO 3
      x := iif( i == 2, "a", i * 64 )
      y := Int( x / 16 )
      s += LTrim( Str( ( y * 1000000000000000 ) / 1000000000000000, 25, 3 ) ) + " "
   NEXT
RETURN s

STATIC FUNCTION Compared()
   LOCAL i, x, n, s := ""
   FOR i := 1 TO 3
      x := iif( i == 2, "a", i ) & 255
      IF x < 1000
         n := x * 1000000000000000
      ELSE
         n := x * 1000000000000000
      ENDIF
      s += LTrim( Str( n / 1000000000000000, 25, 3 ) ) + " "
   NEXT
RETURN s

// The limit "x" + 1 fails and becomes 3: the loop runs to it.
STATIC FUNCTION Limit()
   LOCAL i, n := 0, lim := "x"
   FOR i := 1 TO lim + 1
      n := n + i * 1000000000000000000
   NEXT
RETURN Str( n, 25, 0 ) + Str( i, 4 )
