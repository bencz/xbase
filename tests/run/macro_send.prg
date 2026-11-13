// Macro messages: o:&name, o:&name.suffix, o:prefix&name, o:&( expr ), with arguments and `...`, assigned (:=, +=, ++),
// as a statement, chained (o:&c:&d), from a method (::&c, reaching a HIDDEN member), Super:&c, in WITH OBJECT (:&c),
// in a codeblock, to values that are not objects (a SCALAR CLASS's method), a name longer than 64 characters, the
// name in any letter case and with blanks around it; ::&c assigned and incremented, o:&c[1], RETURN :&c and IF :&c in
// WITH OBJECT, o:&c( ... ) with no other argument, and the order: the text, then the object, then the arguments; the
// errors: a value that is not a string (1065), a text that
// is not a name (1449), a message the class does not answer (1004, 1005 to assign), a scope violation (41).
MEMVAR c, d

PROCEDURE Main()
   LOCAL o := TChild():New(), e, b, x
   PRIVATE c := "x", d := "Sum"
   ? o:&c, o:&c.y, o:p&c, o:&d( 1, 2 ), o:&( "x" ), o:&( Lower( d ) )( 3, 4 ), Spread( o, 5, 6 )
   o:&c := 5
   ? o:x, o:&c, o:&c += 2, o:x, o:&c++, o:x, ++o:&c, o:x, o:&c.y := "Y", o:xy
   o:&( "Bump" )()
   ? "statement:", o:x
   o:other := TChild():New()
   d := "other"
   ? "chained:", o:&d:&c, ( o:&d:&c := 9 ), o:other:x
   ? "method:", o:Peek( "secret" ), o:Peek( "x" ), o:Up( "Sum" ), o:Store( "secret", "kept" ), o:Peek( "secret" ), ;
      o:Step( "x" ), o:x
   o:list := { "first", "second" }
   c := "list"
   ? "element:", o:&c[ 1 ], o:&c[ 2 ] := "two", o:list[ 2 ]
   ? "with return:", WithReturn( o, "xy" ), "only spread:", OnlySpread( o )
   ? "order:"
   ?? Said( TChild():New(), "obj" ):&( Said( "x", "txt" ) )( Said( 1, "arg" ) )
   Said( o, "obj" ):&( Said( "x", "txt" ) ) := Said( 3, "value" )
   c := "x"
   WITH OBJECT o
      ? "with:", :&c, :&( "xy" )
   END
   c := "Twice"
   b := {| v | v:&c() }
   ? "block:", Eval( b, "ab" ), Eval( b, 21 )
   c := Replicate( "a", 70 )
   ? "long:", o:&c, o:&c := "long", o:&c
   c := "  xY  "
   ? "case and blanks:", o:&c
   ErrorBlock( {| oErr | Break( oErr ) } )
   FOR EACH x IN { 12, "a b", "", "nope", "_nope", "secret" }
      c := x
      BEGIN SEQUENCE
         ? o:&c
      RECOVER USING e
         ? "error", e:subCode, e:operation, e:description
      END SEQUENCE
   NEXT
   BEGIN SEQUENCE
      c := "nope"
      o:&c := 1
   RECOVER USING e
      ? "error", e:subCode, e:operation, e:description
   END SEQUENCE
RETURN

STATIC FUNCTION Spread( o, ... )
RETURN o:&( "Sum" )( ... )

STATIC FUNCTION OnlySpread( ... )
   LOCAL o := TChild():New()
RETURN o:&( "Count" )( ... )

STATIC FUNCTION WithReturn( o, cName )
   WITH OBJECT o
      IF :&cName == "Y"
         RETURN :&cName + "!"
      ENDIF
   END
RETURN "no"

STATIC FUNCTION Said( x, cWhat )
   ?? "", cWhat
RETURN x

CLASS TBase
   VAR x INIT 1
   METHOD Sum( a, b ) INLINE a + b
   METHOD Bump() INLINE ::x += 100
ENDCLASS

CLASS TChild INHERIT TBase
   VAR xy INIT "xy"
   VAR px INIT "px"
   VAR other
   VAR list
   VAR aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa INIT "seventy"
   METHOD Peek( cName ) INLINE ::&cName
   METHOD Store( cName, x ) INLINE ::&cName := x
   METHOD Step( cName ) INLINE ::&cName++
   METHOD Count( ... )
   METHOD Up( cName )
   METHOD Sum( a, b ) INLINE a * b
   HIDDEN:
   VAR secret INIT "hidden"
ENDCLASS

METHOD Count( ... ) CLASS TChild
RETURN PCount()

METHOD Up( cName ) CLASS TChild
RETURN Super:&cName( 10, 20 ) * 2

SCALAR CLASS CHARACTER
   METHOD Twice() INLINE Self + Self
ENDCLASS

SCALAR CLASS NUMERIC
   METHOD Twice() INLINE Self * 2
ENDCLASS
