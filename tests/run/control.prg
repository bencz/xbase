PROCEDURE Main()
   LOCAL i, n := 0, s := ""

   FOR i := 1 TO 10
      IF i % 2 == 0
         LOOP
      ENDIF
      n += i
   NEXT
   ? "odd sum", n          // 1+3+5+7+9

   FOR i := 10 TO 1 STEP -3
      s += Str2( i )
   NEXT i
   ? s

   i := 0
   DO WHILE .T.
      i++
      IF i >= 5
         EXIT
      ENDIF
   ENDDO
   ? "i =", i

   IF i < 3
      ? "small"
   ELSEIF i < 10
      ? "medium"
   ELSE
      ? "large"
   ENDIF

   ? IIf( i == 5 .AND. .NOT. i > 6, "and/not ok", "and/not BAD" )
   ? IIf( i == 1 .OR. i == 5, "or ok", "or BAD" )
   RETURN

STATIC FUNCTION Str2( n )
   IF n == 10
      RETURN "10,"
   ELSEIF n == 7
      RETURN "7,"
   ELSEIF n == 4
      RETURN "4,"
   ENDIF
   RETURN "1"
