// Casts to an ancestor and reflection. Compared with the reference: o:Parent:name reads and assigns the variable of
// that ancestor's area, o:Parent:Method() runs the ancestor's method with the object as Self (its own sends still
// reach the overrides), o:Super:name is the first parent's, a cast kept in a variable, ClassName() of a cast, and
// PCount() and PValue() in a method (Self is not counted). Deliberate differences: a cast equals its object (the
// reference makes a new object), and a method of TB sees TB's variable `a` (each ancestor has its own area).
// Also: casts inside methods (::TA:a), ::Super alone, a cast through a codeblock, and the reflection functions
// ObjIsKindOf, ObjHasVar, ObjHasMethod, ObjVars, ObjMethods and ObjSend on compiled and runtime classes.
PROCEDURE Main()
   LOCAL o := TM():New(), x, e
   ? "casts:", o:TA:a, o:TB:a, o:a, o:Super:a
   o:TB:a := "b2"
   o:TA:n += 5
   ? "assigned:", o:TA:a, o:TB:a, o:a, o:n
   x := o:TB
   ? "kept:", ValType( x ), x:a, x:ClassName(), o:TB:ClassName(), o:TM:ClassName(), x == o
   ? "methods:", o:TA:Show(), o:TA:Who(), o:Show(), o:TB:Who()
   ? "inside:", o:Both(), Eval( o:Getter() ), o:Parent():ClassName()
   ? "args:", o:Args( 1, 2 ), o:Args(), o:Args( , "x", .T. )

   ? "kind:", ObjIsKindOf( o, "tb" ), ObjIsKindOf( o, "TM" ), ObjIsKindOf( o, "TX" ), ObjIsKindOf( 1, "TM" ), ;
      ObjIsKindOf( ErrorNew(), "Error" )
   ? "has:", ObjHasVar( o, "a" ), ObjHasVar( o, "show" ), ObjHasMethod( o, "Show" ), ObjHasMethod( o, "secret" ), ;
      ObjHasVar( o, "secret" ), ObjHasVar( ErrorNew(), "Description" ), ObjHasMethod( ErrorNew(), "description" )
   ? "vars:", ArrayText( ObjVars( o ) ), "methods:", ArrayText( ObjMethods( o ) ), ;
      "error:", Len( ObjVars( ErrorNew() ) ), Len( ObjMethods( ErrorNew() ) )
   ? "send:", ObjSend( o, "show" ), ObjSend( o, "_a", "set" ), o:a, ObjSend( o, "args", 7 ), ObjSend( 12, "ClassName" )

   ErrorBlock( {| oErr | QOut( "error:", oErr:genCode, oErr:subCode, oErr:description, oErr:operation ), ;
      Break( oErr ) } )
   FOR EACH e IN { {|| o:TX:a }, {|| o:TA:secret }, {|| ObjSend( o, 5 ) }, {|| ObjVars( "x" ) }, ;
                   {|| TA():New():Super:a } }
      BEGIN SEQUENCE
         Eval( e )
      END SEQUENCE
   NEXT
RETURN

STATIC FUNCTION ArrayText( a )
   LOCAL c := "", s
   FOR EACH s IN a
      c += IIF( Empty( c ), "", "," ) + s
   NEXT
RETURN c

CLASS TA
   VAR a INIT "a"
   VAR n INIT 1
   METHOD Show() INLINE "TA:" + ::a
   METHOD Who() INLINE "TA who " + ::Show()
   HIDDEN:
   VAR secret INIT 0
ENDCLASS

CLASS TB
   VAR a INIT "b"
   METHOD Show() INLINE "TB:" + ::a
   METHOD Who() INLINE "TB who " + ::Show()
ENDCLASS

CLASS TM INHERIT TA, TB
   METHOD Show() INLINE "TM:" + ::a
   METHOD Both() INLINE ::TA:a + "/" + ::TB:a
   METHOD Getter()
   METHOD Parent() INLINE ::Super
   METHOD Args( x, y )
ENDCLASS

METHOD Getter() CLASS TM
RETURN {|| ::TB:a }

METHOD Args( x, y ) CLASS TM
RETURN LTrim( Str( PCount() ) ) + ValType( PValue( 1 ) ) + ValType( PValue( 2 ) ) + ValType( PValue( 3 ) ) + ;
   ValType( PValue( 0 ) )
