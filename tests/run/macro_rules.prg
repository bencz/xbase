// Macro rules found by review, compared with the reference: long lists and deep texts, the PRIVATEs Type() creates,
// strings with `&` where a constant is required, long substitutions, Type()'s U and UI, a predefined Eval(), the
// prefix&name form inside a macro, and an error inside a function Type() reaches through a macro.
// Deliberate differences: a text nested deeper than 1024 levels is error 1449 (the reference may exhaust its stack);
// a text that keeps inserting itself (c := "&c") stops being expanded (the reference never ends).
#include "error.ch"
MEMVAR t, v, u, c, d, made, a, name, b, em, cName, xyz
STATIC s := "R&D"

PROCEDURE Main()
   LOCAL x := "R&D", e
   PRIVATE t := "{" + Replicate( "1,", 1100 ) + "1}", v := "x", u, c, d := 5
   ? "a long list:", Len( &t )
   t := Replicate( "1+", 500 ) + "1"
   ? "a long chain:", &t
   t := Replicate( "(", 300 ) + "1" + Replicate( ")", 300 )
   ? "deep parentheses:", &t
   ErrorBlock( {| oErr | Break( oErr ) } )
   BEGIN SEQUENCE
      t := Replicate( "1+", 5000 ) + "1"
      ? &t
   RECOVER USING e
      ? "too deep:", e:subCode
   END SEQUENCE

   MakePrivate()
   ? "Type() ends the PRIVATE it makes:", Type( "made" )

   ? "constants:", s, x
   SWITCH x
   CASE "R&D"
      ? "the CASE as written"
      EXIT
   OTHERWISE
      ? "other"
   ENDSWITCH

   t := Replicate( "&v. ", 5000 )
   u := "&t"
   ? "long substitution:", Len( u ), "&" $ u
   c := "&c"
   ? "a text inserting itself ends:", "&c"
   PRIVATE a := "&nam", name := "OK", b := "x&", em := "&cName", cName := "xy", xyz := "XYZ"
   ? "names run on past an insertion:", "&a.e", "&b.name"
   BEGIN SEQUENCE
      ? &em.z
   RECOVER USING e
      ? "the name the insertion made:", e:subCode, e:operation
   END SEQUENCE
   c := "1"
   ? "a blank after &:", & (c), Type( "& (c)" ), &( "& (c)" )

   ? "types:", Type( "Foo( zzq )" ), Type( "zzq + Foo()" ), Type( "Bar()" ), Type( "Eval( {|| 1 } )" )
   BEGIN SEQUENCE
      ? &( "a&d" )
   RECOVER USING e
      ? "a&d with a number:", e:subCode
   END SEQUENCE
   ? "an error past a BEGIN SEQUENCE:", Type( "&( 'Inn' + 'er()' )" )
RETURN

PROCEDURE MakePrivate()
   ? "made:", Type( "made := 5" )
RETURN

FUNCTION Foo()
RETURN 1

FUNCTION Inner()
   LOCAL n
   BEGIN SEQUENCE
      n := 1 + "a"
   RECOVER
      ? "recovered"
   END SEQUENCE
RETURN n
