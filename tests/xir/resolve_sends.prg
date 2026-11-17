// Sends resolved when the object's class is known (resolve-sends), and those left to the runtime: a method becomes a
// call (inlined when small), a variable an object.get or object.set of its slot (TPair's own variables first, then
// its parent's area); a cast, a class variable, an assignment without its value, a message no member answers, a
// HIDDEN variable from outside, and a send to an object whose class is not known stay sends.
PROCEDURE Main( x )
   LOCAL o := TPair():New(), a := { o }
   o:first := 1
   o:last := o:first + o:Doubled()
   ? o:Total(), o:TBasis:base, o:count, o:_first(), o:Nothing(), o:secret, a[ 1 ]:first, x:first
RETURN

CLASS TBasis
   VAR base INIT 10
   CLASSVAR count INIT 0
   METHOD Doubled() INLINE ::base * 2
ENDCLASS

CLASS TPair INHERIT TBasis
   VAR first INIT 0
   VAR last INIT 0
   METHOD Total()
   HIDDEN:
   VAR secret INIT "s"
ENDCLASS

METHOD Total() CLASS TPair
   LOCAL n := ::first + ::last + ::base
   IF n > 100
      n := 100
   ENDIF
RETURN n + ::Doubled() - ::base
