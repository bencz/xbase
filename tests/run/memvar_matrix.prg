// Memory variables seen from code that runs at different depths: a PUBLIC, a PRIVATE of the caller, a PRIVATE
// hiding a PUBLIC of the same name, a parameter (PARAMETERS), a PRIVATE released (RELEASE: NIL, still there), one made
// by a macro, and names that are only LOCAL or STATIC (no memory variable). Each name is read in every way a program
// has: undeclared, M->, &name, a macro of "M->" + name, a codeblock from MemVarBlock(), Type(), and assigned through
// each; then the same after the function that made the PRIVATEs returns. A read shows its value or E and the code.
MEMVAR pub, shadow, priv, parm, rel, made
STATIC st := "static"

PROCEDURE Main()
   PUBLIC pub := "public", shadow := "public shadow"
   Maker( "parameter" )
   ? "after return:"
   Show()
RETURN

PROCEDURE Maker()
   LOCAL loc := "local"
   ?? "", Len( loc )
   PARAMETERS parm
   PRIVATE priv := "private", shadow := "private shadow", rel := "released"
   RELEASE rel
   &( "made" ) := "made by a macro"
   ? "in the maker:"
   Show()
   ? "inner function:"
   Inner()
   ? "after assignments:"
   Show()
RETURN

STATIC PROCEDURE Inner()
   PRIVATE priv := "inner hides"
   Show()
   Assign()
RETURN

STATIC PROCEDURE Assign()
   LOCAL c, b
   FOR EACH c IN { "pub", "shadow", "priv", "parm", "rel", "made" }
      b := MemVarBlock( c )
      Eval( b, c + " by block" )
      IF c $ "priv parm"
         &c := c + " by macro"
      ENDIF
   NEXT
   M->pub += " +M"
   shadow += " +u"
RETURN

STATIC PROCEDURE Show()
   LOCAL c
   FOR EACH c IN { "pub", "shadow", "priv", "parm", "rel", "made", "loc", "st", "nothing" }
      ? PadR( c, 7 ), Read( {|| &c } ), Read( {|| &( "M->" + c ) } ), Read( {|| Eval( MemVarBlock( c ) ) } ), ;
         Type( c ), Type( "M->" + c ), ValType( MemVarBlock( c ) )
   NEXT
   ? "undeclared", Read( {|| pub } ), Read( {|| M->shadow } ), Read( {|| priv } ), Read( {|| parm } ), ;
      Read( {|| rel } ), Read( {|| made } )
RETURN

STATIC FUNCTION Read( b )
   LOCAL x, e
   BEGIN SEQUENCE WITH {| o | Break( o ) }
      x := Eval( b )
   RECOVER USING e
      RETURN "E" + LTrim( Str( e:subCode ) )
   END SEQUENCE
RETURN iif( x == NIL, "NIL", x )
