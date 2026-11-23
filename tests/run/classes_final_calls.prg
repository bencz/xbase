// A method's send to its own Self reaches one method whatever the object's class when the class is FINAL or the method
// is: the compiler calls it directly (resolve-sends). The results are the sends': for objects of the class and of its
// subclasses (of this module and of another, which a subclass that declared the FINAL method again could not make),
// through Super:, with the scope rules (a HIDDEN FINAL method reached from its own class), for variables of a FINAL
// class, and for a method that is not FINAL (still dispatched: a subclass overrides it).
PROCEDURE Main()
   LOCAL o
   o := TBase():New( 2 )
   ? "base:", o:Total(), o:Describe()
   o := TChild():New( 3 )
   ? "child:", o:Total(), o:Describe(), o:Again()
   o := TOther():New( 4 )
   ? "other module's child:", o:Total(), o:Describe()
   o := TSealed():New( 5 )
   ? "final class:", o:Sum(), o:Bumped(), o:n
RETURN

CLASS TBase
   VAR n
   METHOD New( n ) INLINE ::n := n, Self
   METHOD Twice() FINAL INLINE ::n * 2
   METHOD Kind() INLINE "base"
   METHOD Total()
   METHOD Describe()
   HIDDEN:
   METHOD Secret() FINAL INLINE "secret" + LTrim( Str( ::n ) )
ENDCLASS

METHOD Total() CLASS TBase
RETURN ::Twice() + ::Twice()

METHOD Describe() CLASS TBase
RETURN ::Kind() + "/" + ::Secret()

CLASS TChild INHERIT TBase
   METHOD Kind() INLINE "child"
   METHOD Again()
ENDCLASS

METHOD Again() CLASS TChild
RETURN ::Twice() + Super:Twice()

FINAL CLASS TSealed
   VAR n
   METHOD New( n ) INLINE ::n := n, Self
   METHOD Add( x ) INLINE ::n + x
   METHOD Sum()
   METHOD Bumped()
ENDCLASS

METHOD Sum() CLASS TSealed
RETURN ::Add( 1 ) + ::Add( 2 ) + ::n

METHOD Bumped() CLASS TSealed
   ::n := ::n + 10
RETURN ::n
