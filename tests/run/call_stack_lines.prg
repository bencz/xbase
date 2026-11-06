// The line the call stack shows for a statement is where it ends: CASE and ELSEIF conditions,
// the step of FOR (its line), statements continued over several lines, codeblocks.
PROCEDURE Main()
   LOCAL x := 1, i, a
   ErrorBlock( {| e | Handler( e ) } )
   DO CASE
   CASE x > 5
      ? "no"
   CASE x < "a"
      ? "case"
   ENDCASE
   IF x == 2
      ? "no"
   ELSEIF x == {}
      ? "no"
   ENDIF
   FOR i := 1 TO 2
      i := "a"
      ? "body"
   NEXT
   ? "after for", i
   x := Two( 1 ) + ;
        Two( 2 ) + ;
        3
   Two( ;
      4 )
   IF Two( 5 ) > 0 .AND. ;
      Two( 6 ) > 0
      ? "if"
   ENDIF
   a := { Two( 8 ), ;
          Two( 9 ) }
   a := {|| Two( 10 ) + ;
            Two( 11 ) }
   Eval( a )
   WHILE x < "z"
      EXIT
   ENDDO
RETURN

FUNCTION Two( n )
   QOut( "two", n, ProcLine( 1 ) )
RETURN n

STATIC FUNCTION Handler( e )
   QOut( Left( e:operation, 1 ), ProcName( 2 ), ProcLine( 2 ) )
   IF e:operation $ "<=="
      RETURN .F.
   ENDIF
RETURN IIf( Left( e:operation, 1 ) == "+", 99, 0 )
