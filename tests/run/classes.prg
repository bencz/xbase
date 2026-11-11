// Classes, compared with the reference: variables with INIT (evaluated once, arrays copied for each object), methods
// (implemented and INLINE), ACCESS/ASSIGN, Self and ::, Super, New and Init, scopes (EXPORTED, PROTECTED, HIDDEN,
// READONLY) from outside, from the class and from a subclass, codeblocks made in methods, multiple inheritance with
// an area of variables per ancestor, STATIC CLASS, and the errors of a message no class answers. Deliberate
// differences: Super:message works without :: (the reference needs ::Super), and two ancestors' variables of the same
// name are two variables, each seen by its class's methods (the reference keeps one, the first ancestor's).
STATIC nCreated := 0

PROCEDURE Main()
   LOCAL o, p, a, i, e
   o := TPoint():New( 1, 2 )
   ? "point:", o:ClassName(), ValType( o ), o:x, o:y, o:Sum(), o:Twice, o:Label()
   p := TPoint():New()
   ? "defaults:", p:x, p:y, p:serial, nCreated, ValType( p:items ), Len( p:items ), p:items == o:items
   AAdd( p:items, 1 )
   ? "own arrays:", Len( p:items ), Len( o:items ), Len( TPoint():New():items )
   o:x := 10
   o:Move( 5, 5 )
   ? "moved:", o:x, o:y, o:Sum()
   ? "plain object:", ValType( TPoint() ), TPoint():x
   ? "block:", Eval( o:Adder(), 100 ), Eval( o:Hider() )

   ? "child:", TChild():New( 3, 4 ):Sum(), TChild():New( 3, 4 ):ClassName(), TChild():New( 1, 1 ):Describe(), ;
      TChild():New( 2, 2 ):Label()
   ? "init:", TCounted():New( 7 ):n, TCounted():New( 7, 8 ):n, ValType( TCounted():New():Init() )
   ? "temperature:", TTemp():New():Fahrenheit
   a := TTemp():New()
   a:Celsius := 100
   a:Kelvin := 0
   ? "access:", a:Celsius, a:Kelvin, a:Fahrenheit
   ? "multiple:", TMulti():New():Show(), TMulti():New():a, TMulti():New():FromB(), TMulti():New():Both()
   ? "static class:", TLocal():New():v

   ErrorBlock( {| oErr | QOut( "error:", oErr:genCode, oErr:subCode, oErr:description, oErr:operation, ;
      ValType( oErr:args ) ), Break( oErr ) } )
   FOR EACH e IN { {|| o:secret }, {|| o:prot }, {|| o:ro := 5 }, {|| o:nomethod( 1 ) }, {|| o:novar := 1 }, ;
                   {|| TChild():New( 1, 1 ):Peek() } }
      BEGIN SEQUENCE
         Eval( e )
      END SEQUENCE
   NEXT
   ? "readonly kept:", o:ro, o:ReadOnly()

   FOR i := 1 TO 2000 // objects come and go
      a := TPoint():New( i, i )
      a:items := { i, Str( i ) }
   NEXT
   __GcCollect()
   ? "after collecting:", a:x, a:items[ 2 ]
RETURN

CLASS TPoint
   VAR x INIT 0
   VAR y INIT 0
   VAR serial INIT Created()
   VAR items INIT {}
   VAR ro INIT 7 READONLY
   HIDDEN:
   VAR secret INIT 1
   PROTECTED:
   VAR prot INIT 2
   EXPORTED:
   METHOD New( x, y ) CONSTRUCTOR
   METHOD Sum()
   METHOD Move( dx, dy )
   METHOD Label()
   METHOD Adder()
   METHOD Hider()
   METHOD ReadOnly()
   METHOD Twice INLINE ::x * 2
ENDCLASS

METHOD New( x, y ) CLASS TPoint
   IF x != NIL
      ::x := x
   ENDIF
   IF y != NIL
      ::y := y
   ENDIF
RETURN Self

METHOD Sum() CLASS TPoint
RETURN ::x + ::y + ::secret + ::prot

METHOD Move( dx, dy ) CLASS TPoint
   ::x += dx
   Self:y += dy
RETURN Self

METHOD Label() CLASS TPoint
RETURN "(" + LTrim( Str( ::x ) ) + "," + LTrim( Str( ::y ) ) + ")"

METHOD Adder() CLASS TPoint
RETURN {| n | n + ::x }

METHOD Hider() CLASS TPoint
RETURN {|| ::secret } // a block has the access of the method that made it

METHOD ReadOnly() CLASS TPoint
   ::ro := 8
RETURN ::ro

CLASS TChild INHERIT TPoint
   METHOD Sum()
   METHOD Describe()
   METHOD Label()
   METHOD Peek()
ENDCLASS

METHOD Sum() CLASS TChild
RETURN ::Super:Sum() * 10 + ::prot

METHOD Describe() CLASS TChild
RETURN "child of " + ::Super:ClassName()

METHOD Label() CLASS TChild
RETURN "child " + Super:Label()

METHOD Peek() CLASS TChild
RETURN ::secret // HIDDEN in TPoint

CLASS TCounted
   VAR n INIT 0
   METHOD Init( a, b )
ENDCLASS

METHOD Init( a, b ) CLASS TCounted
   IF a != NIL
      ::n := a + IIF( b == NIL, 0, b )
   ENDIF
RETURN Self

CREATE CLASS TTemp
   VAR nC AS NUMERIC INIT 0
   ACCESS Celsius INLINE ::nC
   ASSIGN Celsius( v ) INLINE ::nC := v
   ACCESS Fahrenheit
   ACCESS Kelvin
   ASSIGN Kelvin( v )
END CLASS

ACCESS Fahrenheit CLASS TTemp
RETURN ::nC * 9 / 5 + 32

METHOD Kelvin CLASS TTemp
RETURN ::nC + 273

ASSIGN Kelvin( v ) CLASS TTemp
   ::nC := v - 273
RETURN v

CLASS TA
   VAR a INIT "A"
   METHOD Show() INLINE "TA:" + ::a
ENDCLASS

CLASS TB
   VAR a INIT "B"
   METHOD Show() INLINE "TB:" + ::a
   METHOD FromB() INLINE "seen from TB: " + ::a
ENDCLASS

CLASS TMulti INHERIT TA, TB
   METHOD Both()
ENDCLASS

METHOD Both() CLASS TMulti
RETURN ::Show() + " " + ::FromB()

STATIC CLASS TLocal
   VAR v INIT "local"
ENDCLASS

STATIC FUNCTION Created()
RETURN ++nCreated
