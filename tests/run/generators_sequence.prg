// YIELD inside BEGIN SEQUENCE and TRY: an error in a later step reaches the generator's RECOVER or CATCH; between the
// steps the caller runs with its own error block (a WITH handler is set aside at YIELD and comes back with the next
// step, nested ones too), the block the caller has then is the one the sequence puts back in the end, a generator
// left in the middle leaves the caller's block in place, and a BREAK of the caller is not the generator's.
PROCEDURE Main()
   LOCAL x, c := "", bMine := {| e | Break( "caller" ) }, bOther := {| e | Break( "other" ) }, g
   ErrorBlock( bMine )
   FOR EACH x IN Recovering()
      c += x + " "
   NEXT
   ? "recover:", c
   c := ""
   FOR EACH x IN WithHandler()
      c += x + "(" + Which( bMine, bOther ) + ") "
   NEXT
   ? "with:", c, Which( bMine, bOther )
   c := ""
   FOR EACH x IN Tried()
      c += x + " "
   NEXT
   ? "try:", c, Which( bMine, bOther )
   c := ""
   FOR EACH x IN Nested()
      c += x + "(" + Which( bMine, bOther ) + ") "
   NEXT
   ? "nested:", c, Which( bMine, bOther )
   c := ""
   g := WithHandler()
   g:MoveNext()
   ErrorBlock( bOther ) // the caller changes its block between the steps
   DO WHILE g:MoveNext()
      c += g:Current + "(" + Which( bMine, bOther ) + ") "
   ENDDO
   ? "changed:", c, Which( bMine, bOther )
   ErrorBlock( bMine )
   FOR EACH x IN WithHandler()
      EXIT
   NEXT
   ? "left:", Which( bMine, bOther )
   c := ""
   BEGIN SEQUENCE
      FOR EACH x IN Recovering()
         c += x + " "
         IF x == "b"
            Break( "mine" )
         ENDIF
      NEXT
   RECOVER USING x
      c += "caught " + x
   END SEQUENCE
   ? "caller's break:", c
RETURN

STATIC FUNCTION Which( bMine, bOther )
   LOCAL b := ErrorBlock()
RETURN IIF( b == bMine, "mine", IIF( b == bOther, "other", "handler" ) )

FUNCTION Recovering()
   LOCAL e
   BEGIN SEQUENCE
      YIELD "a"
      YIELD "b"
      YIELD 1 + "x"
   RECOVER USING e
      YIELD "recovered " + ValType( e )
   END SEQUENCE
   YIELD "after"
RETURN

FUNCTION WithHandler()
   LOCAL e
   BEGIN SEQUENCE WITH {| o | Break( o ) }
      YIELD "a"
      YIELD "b"
      e := 1 + "x"
      YIELD "unreached"
   RECOVER USING e
      YIELD "recovered " + e:operation
   END SEQUENCE
RETURN

FUNCTION Tried()
   LOCAL e
   TRY
      YIELD "a"
      Throw( ErrorNew() )
   CATCH e
      YIELD "caught " + ValType( e )
      YIELD "again"
   END
RETURN

FUNCTION Nested()
   LOCAL e
   BEGIN SEQUENCE WITH {| o | Break( "outer" ) }
      YIELD "o1"
      BEGIN SEQUENCE WITH {| o | Break( "inner" ) }
         YIELD "i1"
         e := 1 + "x"
      RECOVER USING e
         YIELD e
      END SEQUENCE
      YIELD "o2"
      e := 1 + "x"
   RECOVER USING e
      YIELD e
   END SEQUENCE
RETURN
