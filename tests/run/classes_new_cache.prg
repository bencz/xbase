// New, Init and ClassName the runtime answers itself, at one send reaching objects of several classes in turn, each
// time twice (the second from the send's cache): New with an Init of the class, of a parent, with none, with a New of
// the class's own; New whose Init is HIDDEN in a parent (an error each time); New whose Init raises an error (the
// object is not the result); Init sent to an object without one; ClassName of each class.
PROCEDURE Main()
   LOCAL a := { TA(), TB(), TC(), TD(), TA(), TB(), TC(), TD() }, o, i, k, x, c := ""
   FOR i := 1 TO 2
      FOR k := 1 TO Len( a )
         o := a[ k ]
         x := o:New( k )
         c += x:ClassName() + ":" + Describe( x ) + " "
      NEXT
   NEXT
   ? "new:", c
   c := ""
   FOR EACH o IN { TB(), TA(), TB(), TA() }
      x := o:Init( 5 )
      c += x:ClassName() + ":" + Describe( x ) + " "
   NEXT
   ? "init:", c
   c := ""
   FOR EACH o IN { TE(), TA(), TE(), TA() }
      BEGIN SEQUENCE WITH {| e | Break( e ) }
         x := o:New( 9 )
         c += "made " + x:ClassName() + " "
      RECOVER USING x
         c += "error " + LTrim( Str( x:subCode ) ) + " " + x:operation + " "
      END SEQUENCE
   NEXT
   ? "scope:", c
   c := ""
   FOR EACH o IN { TF(), TA(), TF(), TA() }
      x := "none"
      BEGIN SEQUENCE WITH {| e | Break( e ) }
         x := o:New( 3 )
      RECOVER USING x
         x := "broke " + x:description
      END SEQUENCE
      c += IIf( ValType( x ) == "O", x:ClassName(), x ) + " "
   NEXT
   ? "failing init:", c
   RETURN

STATIC FUNCTION Describe( x )
   RETURN IIf( x:v == NIL, "NIL", LTrim( Str( x:v ) ) )

CLASS TA
   VAR v
   METHOD Init( n )
ENDCLASS

METHOD Init( n ) CLASS TA
   ::v := n * 10
   RETURN Self

CLASS TB
   VAR v
ENDCLASS

CLASS TC INHERIT TA
   VAR w INIT 1
ENDCLASS

CLASS TD
   VAR v
   METHOD New( n )
ENDCLASS

METHOD New( n ) CLASS TD
   ::v := -n
   RETURN Self

CLASS TP
   VAR v
   HIDDEN:
   METHOD Init( n )
ENDCLASS

METHOD Init( n ) CLASS TP
   ::v := n
   RETURN Self

CLASS TE INHERIT TP
ENDCLASS

CLASS TF
   VAR v
   METHOD Init( n )
ENDCLASS

METHOD Init( n ) CLASS TF
   ::v := n
   RETURN Error( "init fails" )

STATIC FUNCTION Error( cText )
   LOCAL e := ErrorNew()
   e:description := cText
   RETURN Eval( ErrorBlock(), e )
