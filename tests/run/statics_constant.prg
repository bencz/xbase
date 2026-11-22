// STATICs whose initializer alone writes them hold that constant wherever they are read (the loads become it), with
// their literal's display (width, decimals); next to STATICs written elsewhere: by a function, an append, a
// codeblock, a method, by reference; and an initializer whose decimals are SET DECIMALS' (not a constant).
STATIC s_nInt := 42, s_nDbl := 1.50, s_cText := "abc", s_lFlag := .T., s_nBig := 12345678901, s_nNeg := -7.125
STATIC s_nByFunc := 1, s_cAppend := "x", s_nByBlock := 10, s_nByRef := 3, s_nQuot := 10 / 4, s_nNone

PROCEDURE Main()
   LOCAL i, n := 0, b
   FOR i := 1 TO 10
      n += s_nInt
   NEXT
   ? "constant:", s_nInt, s_nDbl, s_cText, s_lFlag, s_nBig, s_nNeg, n
   ? "operations:", s_nInt / 2, s_nDbl * 3, s_nNeg + 1, s_cText + "d", Len( s_cText ), ! s_lFlag, s_nBig * 10
   ? "displayed:", Str( s_nDbl ), Str( s_nNeg ), Transform( s_nInt, "999.99" ), ValType( s_nNone )

   SetByFunc( 5 )
   ? "by a function:", s_nByFunc
   FOR i := 1 TO 3
      s_cAppend += "y"
   NEXT
   ? "appended:", s_cAppend
   b := {|| s_nByBlock := s_nByBlock * 2 }
   Eval( b )
   ? "by a block:", s_nByBlock
   Triple( @s_nByRef )
   ? "by reference:", s_nByRef
   ? "quotient:", s_nQuot
   ? "method:", TCounter():New():Bump(), TCounter():New():Bump()
   RETURN

STATIC PROCEDURE SetByFunc( n )
   s_nByFunc := n
   RETURN

STATIC PROCEDURE Triple( n )
   n *= 3
   RETURN

CLASS TCounter
   METHOD Bump()
ENDCLASS

METHOD Bump() CLASS TCounter
   STATIC s_nCalls := 0
   s_nCalls++
   RETURN s_nCalls
