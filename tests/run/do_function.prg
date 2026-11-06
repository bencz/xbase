// Do(): a function by name (letter case aside, spaces kept), from this module, another one or the
// runtime, a codeblock, references passed on, Do on the call stack, and its errors.
PROCEDURE Main()
   LOCAL x := 1, e
   ? Do( "twice", 21 ), Do( "TWICE", 2 ), Do( {| a, b | a + b }, 3, 4 ), Do( "Len", "abc" )
   Do( "inc", @x )
   ? x
   ? Do( "Stack" ), Do( "fromother", "x" )
   ErrorBlock( {| err | Break( err ) } )
   BEGIN SEQUENCE
      Do( "nosuch" )
   RECOVER USING e
      ? e:subSystem, e:subCode, e:genCode, e:description, e:operation, e:canSubstitute, Len( e:args )
   END
   BEGIN SEQUENCE
      Do( " twice ", 1 )
   RECOVER USING e
      ? e:subCode, e:description, "[" + e:operation + "]"
   END
   BEGIN SEQUENCE
      Do( 5 )
   RECOVER USING e
      ? e:subCode, e:description, e:operation, e:canSubstitute
   END
   BEGIN SEQUENCE
      Do()
   RECOVER USING e
      ? e:subCode, e:description, e:operation
   END
RETURN

FUNCTION Twice( n )
RETURN n * 2

PROCEDURE Inc( n )
   n++
RETURN

FUNCTION Stack()
RETURN ProcName( 0 ) + " " + ProcName( 1 ) + " " + ProcName( 2 )
