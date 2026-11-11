// Classes, compared with the reference where it has the feature: a subclass's override of a member HIDDEN in an
// ancestor, ACCESS and ASSIGN over an inherited variable (from the ancestor's methods too), New reaching a PROTECTED
// Init, INLINE with a list of expressions, the EXPORT: label, and methods implemented without CLASS after the #include
// that declares their class. Also: Super and Self as ordinary variables outside methods and a LOCAL Super in a method,
// Super when Self was assigned an object of another class, an Error object (a runtime class) used in a method,
// PARAMETERS in a method, INIT values that fail once, and INIT values that need an object of their own class or of a
// subclass being prepared.
STATIC nTries := 0
#include "classes_scope.ch"

METHOD Area()
RETURN ::w * ::h

METHOD Half()
RETURN ::Area() / 2

PROCEDURE Main()
   LOCAL super := "local super", o, e
   PRIVATE self := "private self"
   ? "include:", TBox():New():Area(), TBox():New():Half()
   ? "variables:", super, super:ClassName(), self, self:ClassName(), M->self
   ? "hidden:", TLeaf():New():Ask(), TLeaf():New():AskMid(), TLeaf():New():Secret(), TLeaf():New():hv
   o := TLogged():New()
   o:Put( 5 )
   ? "access:", o:Show(), o:value, o:log
   o := TGuarded():New( 9 )
   ? "init:", o:n, o:Twice(), o:n
   ? "local super:", TOdd():New():Peek()
   ? "runtime class:", TOdd():New():Described()
   ? "parameters:", TOdd():New():Params( 1, "two" )

   ErrorBlock( {| oErr | QOut( "error:", oErr:genCode, oErr:subCode, oErr:description, oErr:operation ), ;
      Break( oErr ) } )
   FOR EACH e IN { {|| o:Init( 2 ) }, {|| TOdd():New():Swap() }, {|| TFlaky():New() }, {|| TSelfish():New() }, ;
                   {|| TSelfish():New() }, {|| TChildMade():New() } }
      BEGIN SEQUENCE
         Eval( e )
      END SEQUENCE
   NEXT
   ? "flaky:", TFlaky():New():v, TFlaky():New():v, nTries
RETURN

CLASS TBase
   HIDDEN:
   METHOD Secret() INLINE "base secret"
   VAR hv INIT "base hv"
   EXPORTED:
   METHOD Ask() INLINE ::Secret() + " / " + ::hv
ENDCLASS

CLASS TMid INHERIT TBase
   METHOD AskMid() INLINE ::Secret() + " / " + ::hv
ENDCLASS

CLASS TLeaf INHERIT TMid
   METHOD Secret() INLINE "leaf secret"
   VAR hv INIT "leaf hv"
ENDCLASS

CLASS TStore
   VAR value INIT 1
   METHOD Show() INLINE ::value
   METHOD Put( v ) INLINE ::value := v
ENDCLASS

CLASS TLogged INHERIT TStore
   VAR log INIT ""
   ACCESS value INLINE ::log += "get ", ::Super:value * 10
   ASSIGN value( v ) INLINE ::log += "set ", ::Super:value := v
ENDCLASS

CLASS TGuarded
   VAR n INIT 0
   PROTECTED:
   METHOD Init( n )
   EXPORT:
   METHOD Twice() INLINE ::n := ::n * 2, ::n + 1
ENDCLASS

METHOD Init( n ) CLASS TGuarded
   ::n := n
RETURN Self

CLASS TOdd INHERIT TBox
   METHOD Swap()
   METHOD Peek()
   METHOD Described()
   METHOD Params()
ENDCLASS

METHOD Described() CLASS TOdd
   LOCAL e := ErrorNew()
   e:description := "described"
RETURN e:description + " " + e:ClassName()

METHOD Params() CLASS TOdd
   PARAMETERS a, b
RETURN ValType( a ) + ValType( b ) + " " + b

METHOD Swap() CLASS TOdd
   Self := TGuarded():New( 1 )
RETURN Super:Area()

METHOD Peek() CLASS TOdd
   LOCAL super := TGuarded():New( 3 )
RETURN super:n

CLASS TFlaky
   VAR v INIT Flaky()
ENDCLASS

STATIC FUNCTION Flaky()
   IF ++nTries == 1
      RETURN 1 + "a"
   ENDIF
RETURN nTries

CLASS TSelfish
   VAR other INIT TSelfish()
ENDCLASS

CLASS TMaker
   VAR made INIT TChildMade()
ENDCLASS

CLASS TChildMade INHERIT TMaker
ENDCLASS
