// LOCALs as SSA values (mem2reg): assigned in one branch only or never, swapped inside loops (values crossing on the
// loop's back edge), updated before EXIT and LOOP, in nested loops, assigned inside IIF and expressions.
PROCEDURE Main()
   LOCAL a := 1, b := 2, t, i, j, n, s := "", never, odd, k := 0, x, y
   FOR i := 1 TO 5
      t := a
      a := b
      b := t
   NEXT
   ? "swap", a, b
   a := 1
   b := 2
   FOR i := 1 TO 4
      a := a + b
      b := a - b
   NEXT
   ? "rotate", a, b
   FOR i := 1 TO 6
      IF i % 2 == 0
         odd := i
      ENDIF
   NEXT
   ? "one branch", odd, ValType( never )
   n := 0
   DO WHILE .T.
      n++
      IF n == 3
         LOOP
      ENDIF
      IF n > 5
         EXIT
      ENDIF
      s += Str( n, 1 )
   ENDDO
   ? "exit loop", n, s
   n := 0
   FOR i := 1 TO 3
      FOR j := 1 TO i
         n += i * j
      NEXT
   NEXT
   ? "nested", n, i, j
   x := IIf( k == 0, ( k := 5, k * 2 ), ( k := 7, k ) )
   ? "iif", x, k
   y := ( k := k + 1 ) + ( k := k * 2 )
   ? "expr", y, k
   a := 0
   WHILE ( a := a + 1 ) < 4
   ENDDO
   ? "cond assign", a
   RETURN
