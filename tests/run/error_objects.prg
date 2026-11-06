// Runtime errors as Error objects: the error block receives them, BREAK takes
// them to RECOVER, and a value the block returns replaces the result of an
// operation whose error allows it (canSubstitute); the default block gives 0
// for a division by zero. The last division BREAKs with no sequence active,
// which ends the program.
PROCEDURE Main()
   LOCAL e, x, b, bOld, i, a := {}
   ? "default", 1 / 0, 7 % 0
   b := ErrorBlock( {| err | Break( err ) } )
   ? ValType( b ), ValType( ErrorBlock() )
   BEGIN SEQUENCE
      x := 1 / 0
   RECOVER USING e
      Show( e )
   END
   BEGIN SEQUENCE
      x := "a" + 1
   RECOVER USING e
      Show( e )
   END
   BEGIN SEQUENCE
      x := a[ 3 ]
   RECOVER USING e
      Show( e )
   END
   BEGIN SEQUENCE
      x := SubStr( 1, 2 )
   RECOVER USING e
      Show( e )
   END
   BEGIN SEQUENCE
      ErrorNew():foo( 1 )
   RECOVER USING e
      Show( e )
   END
   BEGIN SEQUENCE
      x := Replicate( "ab", 3 ) $ 5
   RECOVER USING e
      Show( e )
   END

   // The block's value replaces the failed operation's result.
   bOld := ErrorBlock( {| err | Handler( err ) } )
   ? ValType( bOld )
   x := 1 / 0
   ? x, ValType( x ), x + "!"
   x := "a" + 1
   ? x
   x := 5
   x := x * "b"
   ? x
   ? SubStr( 1, 2 ), Len( 3 ), Eval( 7 ), ErrorNew():bar(), -"z", .NOT. 3
   i := "i"
   ? a[ i ]
   IF {} < 1
      ? "substituted comparison"
   ENDIF
   FOR i := 1 TO "z"
      ? "for", i
      IF i > 2
         EXIT
      ENDIF
   NEXT
   SWITCH {}
   CASE 1
      ? "case 1"
   OTHERWISE
      ? "otherwise"
   ENDSWITCH
   ErrorBlock( 5 )
   ? ValType( ErrorBlock() )
   ErrorBlock( bOld )
   ? 2 / 0
RETURN

STATIC PROCEDURE Show( e )
   LOCAL i
   ? e:ClassName(), e:subSystem, e:subCode, e:genCode, e:description, "[" + e:operation + "]", e:severity, ;
     e:canSubstitute, e:canRetry, e:canDefault, e:tries, "[" + e:filename + "]", e:osCode, ValType( e:cargo )
   QQOut( " args:" )
   FOR i := 1 TO Len( e:args )
      QQOut( " " + ValType( e:args[ i ] ) )
   NEXT
RETURN

STATIC FUNCTION Handler( e )
   SWITCH e:subCode
   CASE 1340
      RETURN "s"
   CASE 1083
      RETURN { 1 }
   CASE 1073
      RETURN .T.
   CASE 1075
      RETURN .T.
   CASE 3104
      RETURN 1
   ENDSWITCH
RETURN e:subCode
