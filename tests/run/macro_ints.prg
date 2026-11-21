// Integer operations in a macro's text as the operators make them (an integer sum, difference or product, a double
// once it overflows; comparisons; a double or a string operand: the operator's own path), and the cache of compiled
// texts: 300 texts evaluated twice in turn, each still giving its own value when many share their sets.
MEMVAR nBig, nOne, nTwo
PROCEDURE Main()
   LOCAL k, aText := {}, nBad := 0, c
   PUBLIC nBig := 9223372036854775807, nOne := 1, nTwo := 2
   ? &( "nOne + nTwo" ), &( "nTwo - nOne * 5" ), &( "nTwo * 21" ), &( "-7 * nTwo" )
   ? ValType( &( "nBig + nOne" ) ), &( "nBig + nOne" ) > nBig, ValType( &( "nBig * nTwo" ) ), ;
      ValType( &( "-nBig - nTwo" ) )
   ? &( "nOne < nTwo" ), &( "nOne <= nOne" ), &( "nTwo > nOne" ), &( "nOne >= nTwo" ), &( "nOne = nOne" ), ;
      &( "nOne == nTwo" ), &( "nOne != nTwo" ), &( "nOne <> nOne" )
   ? &( "nOne + 0.5" ), &( "nTwo * 1.25" ), &( "nOne < 1.5" ), &( "'a' + 'b'" ), &( "nTwo / nTwo" ), &( "7 % nTwo" )
   FOR k := 1 TO 300
      AAdd( aText, "nOne * " + LTrim( Str( k ) ) + " + " + LTrim( Str( k % 7 ) ) )
   NEXT
   FOR c := 1 TO 2
      FOR k := 1 TO 300
         IF &( aText[ k ] ) != k + k % 7
            ++nBad
         ENDIF
      NEXT
   NEXT
   ? "cached texts:", nBad, "wrong"
RETURN
