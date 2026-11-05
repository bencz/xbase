// Runtime operator semantics. Operands are variables so that no compile-time
// folding is involved: `/` and `%` always give doubles with SET DECIMALS.
PROCEDURE Main()
   LOCAL a := "abc", b := "ab", n10 := 10, n4 := 4, n5 := 5, n7 := 7, n3 := 3, n2 := 2, n6 := 6
   ? n10 / n4, n10 / n5, n7 % n3, n2 ^ n10, -n2 ^ n2, n6 / -1
   ? 1.5 + 1.25, 1.5 * 1.25
   ? a = b, b = a, a == b, a != b, "" = a
   ? "b" $ a, "z" $ a, a < "abd", a >= b
   ? "Hello " - "World", ( a + "   " ) - "defghijklmnopqrstuvwxyz"
   ? Len( a + b ), a + b
   ? 1 + 2 * 3 - n4 / n2
   RETURN
