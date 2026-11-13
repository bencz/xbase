// common.ch: TRUE, FALSE, YES, NO; each IS...() test on a value of every type (a memo field's value included: ISMEMO,
// not ISCHARACTER; integers and doubles ISNUMBER; a hash none of them); DEFAULT ... TO for one and several variables
// (only NIL ones change, and a value is evaluated only when it is used); UPDATE ... IF ... TO.
#include "common.ch"

PROCEDURE Main()
   LOCAL aValues, v, n := 0, a := NIL, b := 2, c := NIL, d := "kept"
   dbCreate( "common_ch", { { "M", "M", 10, 0 } } )
   USE common_ch
   APPEND BLANK
   REPLACE M WITH "memo text"
   aValues := { NIL, {}, {|| 1 }, "c", Date(), .T., FIELD->M, 1, 1.5, ErrorNew(), { => } }
   ? TRUE, FALSE, YES, NO
   FOR EACH v IN aValues
      ? ValType( v ), ISNIL( v ), ISARRAY( v ), ISBLOCK( v ), ISCHARACTER( v ), ISDATE( v ), ISLOGICAL( v ), ;
         ISMEMO( v ), ISNUMBER( v ), ISOBJECT( v )
   NEXT
   DEFAULT a TO Counted( @n, 10 )
   DEFAULT b TO Counted( @n, 20 ), c TO Counted( @n, 30 ), d TO Counted( @n, "x" )
   ? "default:", a, b, c, d, "evaluated:", n
   UPDATE d IF b > 1 TO "updated"
   UPDATE a IF b > 5 TO 0
   ? "update:", a, d
   USE
   FErase( "common_ch.dbf" )
   FErase( "common_ch.dbt" )
RETURN

STATIC FUNCTION Counted( n, x )
   n++
RETURN x
