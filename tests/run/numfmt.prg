// Number display: a sign only with a non-zero digit, '*' when the value does
// not fit or is not finite, 16 significant digits.
PROCEDURE Main()
   LOCAL x := -1, big := 10000000000, huge := 1000000000000000000, v := 1.0, i
   ? x / 1000, 0.0 * x
   ? big * big * 100000
   FOR i := 1 TO 20
      v *= huge
   NEXT
   ? v
   ? 123456789012345678 * 1.0
   ? 0.5, -0.5 * 1, 2.25 * 1.0
   RETURN
