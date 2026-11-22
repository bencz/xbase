// Sends to objects whose class the compiler does not know (made by a macro, read from an array) are versioned on the
// object's class when the classes of the module that answer the message all reach one member (speculate-sends): an
// object of one of them reaches that member in place, any other value what the runtime finds. Only() and Twice() are
// answered by TBase's methods in every class of this module that answers them (TSame and TOver inherit Twice(); a
// test of the three classes); TElse (another module) answers both with methods of its own, and so does TMix, whose
// parent is TElse (a line the compiler does not know); Hello() is overridden by TOver, so it is not versioned. The
// variable w is TWide's alone (an object.get and object.set when the object is a TWide), and TElse has a w of its
// own. A value that is no object gets the runtime's error, which a send gives.
PROCEDURE Main()
   LOCAL aAll := { &( "TBase():New()" ), &( "TSame():New()" ), &( "TOver():New()" ), &( "TElse():New()" ), ;
                   &( "TMix():New()" ) }, o, aWide := { &( "TWide():New()" ), &( "TElse():New()" ) }, x
   FOR EACH o IN aAll
      ? o:ClassName(), o:Only(), o:Twice( 21 ), o:Hello()
   NEXT
   FOR EACH o IN aWide
      o:w := o:w + 5
      ? o:ClassName(), o:w
   NEXT
   FOR EACH x IN { NIL, 3, "text", { 1 } }
      ? ValType( x ), Fails( x )
   NEXT
RETURN

STATIC FUNCTION Fails( x )
   LOCAL cResult := "no error"
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      x:Only()
   RECOVER USING cResult
      cResult := cResult:Description + " " + cResult:Operation
   END SEQUENCE
RETURN cResult

CLASS TBase
   METHOD Only() INLINE "base only"
   METHOD Twice( n ) INLINE n * 2
   METHOD Hello() INLINE "base hello"
ENDCLASS

CLASS TSame INHERIT TBase
ENDCLASS

CLASS TOver INHERIT TBase
   METHOD Hello() INLINE "over hello"
ENDCLASS

CLASS TMix INHERIT TElse
ENDCLASS

CLASS TWide
   VAR w INIT 10
ENDCLASS
