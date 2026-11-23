// ABSTRACT, FINAL and interfaces. An ABSTRACT CLASS makes no objects, its subclasses that implement every ABSTRACT
// method do (Super: reaching an ABSTRACT method is its error); a FINAL method or class stays as declared; an INTERFACE
// names messages (VAR: the variable read and assigned) that a class that IMPLEMENTS it, or one inheriting it, answers,
// as do the classes that inherit from it; ObjImplements() tells. What the module knows whole the compiler checks
// (tests/fail/class_*); with classes and an interface of another module (classes_abstract_final.with), the runtime
// does, when the class first makes an object: inheriting a FINAL class, leaving an ABSTRACT method, an interface's
// message, or overriding a FINAL method, each the class's error then, and inheriting from a class that breaks one
// (a FINAL method declared again by a parent after the FINAL one's class); a SCALAR class inheriting another module's
// interface is its values' error. Interfaces inheriting from one another across modules are gone through once; one
// may require what the runtime answers by itself (ClassName).
PROCEDURE Main()
   LOCAL aShapes := { TCircle():New( 1 ), TSquare():New( 2 ) }, o, oErr, b
   FOR EACH o IN aShapes
      ? o:Describe(), ObjImplements( o, "IDRAWABLE" ), ObjImplements( o, "ISHAPE" ), ObjImplements( o, "IOTHER" )
   NEXT
   o := TBigCircle():New( 3 )
   ? "inherited:", o:Draw(), ObjImplements( o, "ishape" ), ObjImplements( "x", "ISHAPE" ), ObjIsKindOf( o, "TSHAPE" )
   ? "final:", o:Id(), TGood():New():Ping(), TGood():New():Label
   o := TCyc():New()
   ? "cycle:", o:A(), o:B(), ObjImplements( o, "ICYCB" ), ObjImplements( o, "NOPE" ), TNamed():New():ClassName(), ;
      TFinA():New():Foo()
   o := TBagOf():New( { 3, 1, 2 } )
   ? "runtime interfaces:", o:Sum(), ObjImplements( o, "IENUMERABLE" ), ObjImplements( TCountdown():New( 3 ), ;
      "IENUMERATOR" ), ObjImplements( o, "IDISPOSABLE" ), Counted( TCountdown():New( 3 ) ), Disposed()
   FOR EACH b IN { {|| TShape():New() }, {|| TBigCircle():New( 1 ):SuperArea() }, {|| TSealedChild():New() }, ;
                   {|| TLacking():New() }, {|| TNoPing():New() }, {|| TRefix():New() }, {|| TLacking():Twice() }, ;
                   {|| TFinC():New():Foo() }, {|| Date():Ping() } }
      BEGIN SEQUENCE WITH {| e | Break( e ) }
         ? Eval( b )
      RECOVER USING oErr
         ? "error:", oErr:description, oErr:operation
      END SEQUENCE
   NEXT
RETURN

INTERFACE IShape
   METHOD Area()
   VAR Name
ENDINTERFACE

INTERFACE IDrawable INHERIT IShape
   METHOD Draw()
ENDINTERFACE

ABSTRACT CLASS TShape IMPLEMENTS IDrawable
   VAR Name INIT "shape"
   METHOD Area() ABSTRACT
   METHOD Describe() INLINE ::Name + " " + LTrim( Str( ::Area(), 10, 2 ) )
   METHOD Draw() INLINE "drawing " + ::Name
   METHOD Id() FINAL INLINE "id of " + ::Name
ENDCLASS

CLASS TCircle INHERIT TShape
   VAR r
   METHOD New( r ) INLINE ::r := r, ::Name := "circle", Self
   METHOD Area() INLINE 3.14 * ::r * ::r
ENDCLASS

CLASS TSquare INHERIT TShape
   VAR s
   METHOD New( s ) INLINE ::s := s, ::Name := "square", Self
   METHOD Area() INLINE ::s * ::s
ENDCLASS

CLASS TBigCircle INHERIT TCircle
   METHOD SuperArea() INLINE ::TShape:Area()
ENDCLASS

CLASS TBagOf INHERIT Enumerable IMPLEMENTS IEnumerable
   VAR aItems
   METHOD New( a ) INLINE ::aItems := a, Self
   METHOD GetEnumerator()
ENDCLASS

METHOD GetEnumerator() CLASS TBagOf
   LOCAL x
   FOR EACH x IN ::aItems
      YIELD x
   NEXT
RETURN

CLASS TCountdown IMPLEMENTS IEnumerator, IDisposable
   VAR nFrom, Current
   METHOD New( n ) INLINE ::nFrom := n, Self
   METHOD Reset() INLINE ::Current := ::nFrom + 1, NIL
   METHOD MoveNext() INLINE --::Current > 0
   METHOD Dispose() INLINE M->cDisposed += "disposed ", NIL
ENDCLASS

STATIC FUNCTION Counted( o )
   LOCAL n, c := ""
   FOR EACH n IN o
      c += LTrim( Str( n ) )
   NEXT
RETURN c

STATIC FUNCTION Disposed()
   PRIVATE cDisposed := ""
   USING TCountdown():New( 1 )
   END
RETURN cDisposed

INTERFACE ICycB INHERIT ICycA
   METHOD B()
ENDINTERFACE

CLASS TCyc IMPLEMENTS ICycA
   METHOD A() INLINE "a"
   METHOD B() INLINE "b"
ENDCLASS

INTERFACE INamed
   METHOD ClassName()
ENDINTERFACE

CLASS TNamed IMPLEMENTS INamed
ENDCLASS

CLASS TFinC INHERIT TFinA, TFinB
ENDCLASS

SCALAR CLASS DATE INHERIT IOther
ENDCLASS

CLASS TGood IMPLEMENTS IOther
   VAR Label INIT "labelled"
   METHOD Ping() INLINE "pong"
ENDCLASS

CLASS TSealedChild INHERIT TSealed
ENDCLASS

CLASS TLacking INHERIT TOtherBase
ENDCLASS

CLASS TNoPing IMPLEMENTS IOther
   VAR Label
ENDCLASS

CLASS TRefix INHERIT TFinalMethod
   METHOD Fixed() INLINE "again"
ENDCLASS
