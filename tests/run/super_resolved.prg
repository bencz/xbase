// Super sends resolved when compiling (resolve-sends: a method's Super send to its own Self reaches the parent's member
// of the method's class, as the runtime finds it from that class alone), at -O0 and -O2 (run0.*): three levels, the
// grandparent's method running for an object of the grandchild; the first parent under multiple inheritance; a method
// whose Self is assigned another object, there or on one path (left to the runtime: its error); Super in a codeblock of
// the method (left to the runtime); Super:ClassName(), which no member answers; a PROTECTED and a HIDDEN member of the
// parent; a Super variable read and assigned (the runtime's: its slot is the object's class's, a subclass's own
// variables before it, or another parent's); a value that is no object whose type a method's own code associates with
// another class before its Super send (the method's class's parent all the same, as at -O0: runtime.md).
PROCEDURE Main()
   LOCAL o := TC():New(), e
   ? "levels:", o:Who(), o:Up(), TB():New():Who()
   ? "multiple:", TM():New():Who(), TM():New():First()
   BEGIN SEQUENCE WITH {| x | Break( x ) }
      ? TB():New():Reassigned()
   RECOVER USING e
      ? "self reassigned:", e:Description, e:Operation
   END SEQUENCE
   ? "on one path:", TB():New():Maybe( .F. ), Try( {|| TB():New():Maybe( .T. ) } )
   ? "in a block:", o:InBlock(), "classname:", o:ParentName()
   ? "scopes:", o:Prot(), Try( {|| o:Hid() } )
   ? "variable:", o:Var()
   o := TD():New()
   ? "a subclass's variables first:", o:Get(), o:SetIt(), o:v, o:d, o:b, TQ():New():Get()
   ASSOCIATE CLASS TS WITH TYPE NUMERIC
   ? "association changed while the method runs:", ( 5 ):Switch()
RETURN

CLASS TA
   VAR v INIT "va"
   METHOD Who() INLINE "A"
   METHOD Up() INLINE "A.up"
   PROTECTED:
   METHOD P() INLINE "A.protected"
   HIDDEN:
   METHOD H() INLINE "A.hidden"
ENDCLASS

CLASS TB INHERIT TA
   METHOD Who() INLINE "B<" + ::Super:Who() + ">"
   METHOD Reassigned()
   METHOD Maybe( l )
   VAR b INIT "b"
   METHOD Get() INLINE ::Super:v
   METHOD SetIt() INLINE ::Super:v := "set", ::Super:v
ENDCLASS

// Without variables, to be associated with numbers: its method associates another class, then sends Super.
CLASS TP
   METHOD Who() INLINE "P"
ENDCLASS

CLASS TS INHERIT TP
   METHOD Switch() INLINE ClassAssociate( TOther(), "NUMERIC" ), ::Super:Who()
ENDCLASS

// Variables of their own before TB's and TA's in the object: a Super variable is the object's class's slot.
CLASS TD INHERIT TB
   VAR d INIT "d"
ENDCLASS

CLASS TQ INHERIT TX2, TB
ENDCLASS

CLASS TX2
   VAR x1 INIT "x1"
   VAR x2 INIT "x2"
ENDCLASS

METHOD Maybe( l ) CLASS TB
   IF l
      Self := TOther():New()
   ENDIF
RETURN ::Super:Who()

METHOD Reassigned() CLASS TB
   Self := TOther():New()
RETURN ::Super:Who()

CLASS TC INHERIT TB
   METHOD Who() INLINE "C<" + ::Super:Who() + ">"
   METHOD Up() INLINE "C.up/" + ::Super:Up()
   METHOD InBlock() INLINE Eval( {|| ::Super:Who() } )
   METHOD ParentName() INLINE ::Super:ClassName()
   METHOD Prot() INLINE ::Super:P()
   METHOD Hid() INLINE ::Super:H()
   METHOD Var() INLINE ::Super:v := ::Super:v + "!", ::v
ENDCLASS

CLASS TX
   METHOD Who() INLINE "X"
ENDCLASS

CLASS TM INHERIT TX, TA
   METHOD Who() INLINE "M<" + ::Super:Who() + ">"
   METHOD First() INLINE ::Super:Up()
ENDCLASS

CLASS TOther
   METHOD Who() INLINE "other"
ENDCLASS

STATIC FUNCTION Try( b )
   LOCAL x
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      x := Eval( b )
   RECOVER USING x
      x := x:subsystem + "/" + LTrim( Str( x:subCode ) ) + " " + x:description + " " + x:operation
   END SEQUENCE
RETURN x
