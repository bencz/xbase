// Inline caches of sends: one send reaching objects of several classes in turn (methods, variables in different
// slots, a class variable, an ACCESS where another class has a variable), a cast and a plain object at the same send,
// a scalar class value and an object, an Error object, and a send whose scope check fails for one class and not for
// another; each must give what an uncached send gives.
PROCEDURE Main()
   LOCAL a := { TA():New(), TB():New(), TC():New(), TA():New(), TC():New(), TB():New() }, o, i, e, c := ""
   FOR i := 1 TO 3
      FOR EACH o IN a
         c += o:Name() + "=" + LTrim( Str( o:v ) ) + " "
         o:v := o:v + 10
      NEXT
   NEXT
   ? "mixed:", c
   c := ""
   FOR EACH o IN { a[ 2 ], a[ 2 ]:TA, a[ 2 ], a[ 2 ]:TA }
      c += LTrim( Str( o:v ) ) + "/" + o:ClassName() + " "
   NEXT
   ? "casts:", c
   ASSOCIATE CLASS TNum WITH TYPE NUMERIC
   c := ""
   FOR EACH o IN { 5, a[ 1 ], 7, a[ 1 ] }
      c += o:Name() + " "
   NEXT
   ? "scalar:", c
   c := ""
   FOR EACH o IN { ErrorNew(), a[ 1 ], ErrorNew() }
      c += ValType( o:cargo ) + " "
   NEXT
   ? "runtime class:", c
   ErrorBlock( {| oErr | Break( oErr ) } )
   c := ""
   FOR EACH o IN { a[ 1 ], a[ 3 ], a[ 1 ], a[ 3 ] }
      BEGIN SEQUENCE
         c += LTrim( Str( o:secret ) ) + " "
      RECOVER USING e
         c += "error " + LTrim( Str( e:subCode ) ) + " "
      END SEQUENCE
   NEXT
   ? "scopes:", c
   ? "class var:", TC():New():shared, TA():New():Bump(), TC():New():shared
RETURN

CLASS TA
   VAR cargo INIT "x"
   VAR v INIT 1
   VAR secret INIT 42
   METHOD Name() INLINE "A"
   METHOD Bump() INLINE TC():New():shared := 99
ENDCLASS

CLASS TB INHERIT TA
   VAR pad1
   VAR pad2
   VAR v INIT 2
   METHOD Name() INLINE "B"
ENDCLASS

CLASS TC
   VAR store INIT 3
   CLASSVAR shared INIT 7
   ACCESS v INLINE ::store * 100
   ASSIGN v( x ) INLINE ::store := x
   METHOD Name() INLINE "C"
   HIDDEN:
   VAR secret INIT 0
ENDCLASS

CLASS TNum
   METHOD Name() INLINE "N" + LTrim( Str( Self ) )
ENDCLASS
