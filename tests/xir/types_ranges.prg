// Integer ranges (dumped with --dump-xir-types): + - * of integers whose ranges cannot overflow are integers with no
// guard; those that may get a guard against the integers alone (subst.check guard 4), the rest continuing in the
// generic copy. A FOR counter is bounded by its limit, up and down, with a constant or a variable step of a known
// sign; a comparison bounds its operands on the branch it is known on, and its negation on the other; Int( x / k ) of
// an integer is an integer. Each function stands alone (nothing calls it, so nothing inlines it).
FUNCTION Sum()
   LOCAL i, s := 0
   FOR i := 1 TO 1000
      s := s + i * 3
   NEXT
RETURN s

FUNCTION Down()
   LOCAL i, s := 0
   FOR i := 100 TO -100 STEP -2
      s := s + ( i - 1 ) * 1000
   NEXT
RETURN s

FUNCTION Stepped()
   LOCAL i, k := 3, s := 0
   FOR i := 1 TO 500 STEP k
      s := s + i * i
   NEXT
RETURN s

FUNCTION Squares()
   LOCAL i := 2, j, c := 0
   DO WHILE i * i <= 2000000
      FOR j := i * i TO 2000000 STEP i
         c++
      NEXT
      i++
   ENDDO
RETURN c

FUNCTION Bounded()
   LOCAL k, x, n, s := 0
   FOR k := -2 TO Val( "2" )
      x := k * 9999999999 + 5
      IF x > -1000
         IF x < 1000
            n := x * 1000000000000000
         ELSE
            n := x * 1000000000000000
         ENDIF
      ELSE
         n := x * 1000000000000000
      ENDIF
      s := s + n
   NEXT
RETURN s

FUNCTION Quotients()
   LOCAL i, s := 0
   FOR i := -1000 TO 1000
      s := s + Int( i / 16 ) * 7
   NEXT
RETURN s
