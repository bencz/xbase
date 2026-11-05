FUNCTION Main()
   ? "fib(20) =", Fib( 20 )
   ? "fact(20) =", Fact( 20 )
   ? "fact(21) =", Fact( 21 )   // overflows int64 -> becomes a double
   ? Greet( "xbase" )
   ? Greet()
   ? Missing( 1 )
   DO Show WITH "via DO", 42
   RETURN NIL

FUNCTION Fib( n )
   IF n < 2
      RETURN n
   ENDIF
   RETURN Fib( n - 1 ) + Fib( n - 2 )

FUNCTION Fact( n )
   LOCAL r := 1
   DO WHILE n > 1
      r *= n--
   ENDDO
   RETURN r

FUNCTION Greet( cName )
   IF cName == NIL
      cName := "anonymous"
   ENDIF
   RETURN "Hello, " + cName + "!"

FUNCTION Missing( a, b )
   RETURN ValType( a ) + ValType( b )

PROCEDURE Show( a, b )
   ? a, b
   RETURN
