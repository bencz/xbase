// Left(), Right() and SubStr() of a string and numbers, done through their typed entries (xb_str_left, xb_str_right,
// xb_str_sub) where the code knows the types, at -O0 and -O2 (run0.*): starts and counts of every kind (negative, zero,
// past either end, with decimals, all of the string), from variables and literals, in a loop; and a memo field's
// value, which stays a memo when the result is all of it (the function's own path keeps the item; the reference's
// variable holds a character string, where xbase keeps the memo, XB_STR_MEMO).
PROCEDURE Main()
   LOCAL c := "abcdefghij", k, n, cOut := "", aN := { -12, -3, -1, 0, 1, 2, 5, 9, 10, 11, 25 }, x
   FOR EACH n IN aN
      cOut += "[" + Left( c, n ) + "|" + Right( c, n ) + "|" + SubStr( c, n ) + "|" + SubStr( c, n, 3 ) + "]"
   NEXT
   ? cOut
   ? Left( c, 2.9 ), Right( c, 3.5 ), SubStr( c, 2.7, 2.2 ), SubStr( c, -2.5 ), SubStr( c, 11 ), ;
     "[" + Left( "", 3 ) + "]"
   ? Left( c, 10 ) == c, Right( c, 99 ) == c, SubStr( c, 1 ) == c, SubStr( c, 1, 10 ) == c
   x := 0
   FOR k := 1 TO 1000
      x += Len( Left( c, k % 12 ) ) + Len( Right( c, k % 7 ) ) + Asc( SubStr( c, k % 10 + 1, 2 ) )
   NEXT
   ? "loop:", x
   ? "memo:", Memo()
RETURN

// A memo field's value: all of it through Left()/Right()/SubStr() stays a memo, a part is a character string.
STATIC FUNCTION Memo()
   LOCAL cOut, m
   dbCreate( "substr_typed", { { "NOTES", "M", 10, 0 } } )
   USE substr_typed EXCLUSIVE
   APPEND BLANK
   REPLACE NOTES WITH "memo text"
   m := FIELD->NOTES
   cOut := ValType( m ) + ValType( Left( m, 99 ) ) + ValType( Right( m, 9 ) ) + ValType( SubStr( m, 1 ) ) + ;
      ValType( Left( m, 4 ) ) + ValType( SubStr( m, 2, 3 ) ) + " " + Left( m, 4 ) + SubStr( m, 5 )
   USE
   FErase( "substr_typed.dbf" )
   FErase( "substr_typed.dbt" )
RETURN cOut
